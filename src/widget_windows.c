#include "filesystem.h"
#include "global.h"
#include "widget.h"
#include <WebView2.h>
#include <consoleapi.h>
#include <limits.h>
#include <minwindef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringapiset.h>
#include <windows.h>
#include <json.h>
#include <winerror.h>
#include <winnls.h>
#include <winnt.h>
#include <winscard.h>
#include <mmc.h>

static constexpr char CLASS_NAME[] = "WidgetClass";
static constexpr char TAG_APP_NAME[] = "applicationTitle";
static constexpr char TAG_APP_TOPMOST[] = "topMost";
static constexpr char TAG_WIN_SIZE[] = "windowSize";
static constexpr char TAG_WIN_LOCATION[] = "windowLocation";
static constexpr char TAG_WIN_PREV[] = "previewSize";
static constexpr char TAG_WIN_BORD_RAD[] = "windowBorderRadius";
static constexpr char TAG_WIN_OPACITY[] = "windowOpacity";

static constexpr wchar_t LBL_CTX_MENU_MOVE[] = L"Toggle move";
static constexpr wchar_t LBL_CTX_MENU_CLOSE[] = L"Close";

static constexpr uint8_t MAX_ALPHA = UINT8_MAX;
static constexpr uint8_t HASH_PRIME = 31;
static constexpr uint8_t HANDLE_PREFIX_OFFSET = 7;
static constexpr uint8_t FILE_PREFIX_OFFSET = 8;

static constexpr uint16_t BUFFSIZ = USHRT_MAX;
static constexpr uint16_t DEF_WIDTH = 500;
static constexpr uint16_t DEF_HEIGHT = 500;
static constexpr uint16_t DEF_PREV_WIDTH = DEF_WIDTH;
static constexpr uint16_t DEF_PREV_HEIGHT = DEF_HEIGHT;
static constexpr uint16_t DEF_X = 0;
static constexpr uint16_t DEF_Y = 0;
static constexpr uint16_t DEF_OPACITY = 1;
static constexpr uint16_t DEF_RADIUS = 0;
static constexpr uint16_t KEY_STATE_HELD = SHRT_MAX + 1;

static constexpr bool DEF_CHILD = true;
static constexpr bool DEF_TOPMOST = false;

typedef enum : uint8_t
{
        EVT_OPEN_DEFAULT_DIR,
        EVT_GET_WGT_FILENAMES,
        EVT_OPEN_WGT_FILENAME
} widget_events_t;

typedef enum : uint8_t
{
        widget_char_space = 32,
        widget_char_quote = 34,
        widget_char_slash = 47,
        widget_char_b_slash = 92,
        widget_char_gt = 62
} widget_char_t;

// ----------------------------------------------------------
// Global variables to control the state of the main window |
// ----------------------------------------------------------
static EventRegistrationToken g_closeEventHandlerToken = {};
static EventRegistrationToken g_moveEventHandlerToken = {};
static ICoreWebView2Environment *g_env = nullptr;
static ICoreWebView2Controller *g_controllers[MAX_WIDGETS] = {};
static ICoreWebView2 *g_windows[MAX_WIDGETS] = {};
static HWND g_hWnds[MAX_WIDGETS] = {};
static HWND g_hWndTable[MAX_WIDGETS] = {};

static size_t g_hWndSelectedHash = {};
static size_t g_nCmdShow = {};
static size_t g_widgets = 0;

static HINSTANCE g_hInstance = nullptr;
static ULONG g_handlerRefCount = 0;
static bool g_envCreated = false;

// ----------------------------------------------------------------
// Global variables to control the state of current opened widget |
// ----------------------------------------------------------------
static char g_tmplName[BUFFSIZE] = {};
static char g_tmplPath[BUFFSIZE] = {};

// TODO: Enable this only for the development build later
/**
 * @brief Creates a console window and writes debug messages to it
 * @param message Text to print out
 */
static void
Debug(const char *const message)
{
        AllocConsole();
        HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdOut != NULL && stdOut != INVALID_HANDLE_VALUE)
        {
                DWORD written = 0;
                WriteConsoleA(stdOut, message, strlen(message), &written, NULL);
        }
}

// ---------------------------------------------------------------------
// Forward declaration of function definitions that will be used later |
// ---------------------------------------------------------------------
static HRESULT
HandlerInvoke(const IUnknown *const this,
              const HRESULT errorCode,
              const ICoreWebView2Controller *const arg);

static HRESULT
WebView2ContextMenuRequestEventHandlerInvoke(
        ICoreWebView2ContextMenuRequestedEventHandler *const this,
        ICoreWebView2 *const sender,
        ICoreWebView2ContextMenuRequestedEventArgs *const args);

static HRESULT
WebView2WebMessageReceivedEventHandlerInvoke(
        ICoreWebView2WebMessageReceivedEventHandler *const handler,
        ICoreWebView2 *const webview,
        ICoreWebView2WebMessageReceivedEventArgs *const args);

static HRESULT
OnCloseContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                               IUnknown *const args);

static HRESULT
OnMoveContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                              IUnknown *const args);
static bool
OpenDefaultDirectory();

static bool
AppendWidgetToBrowserList(ICoreWebView2 *const webview);

static bool
create_widget_window(ww_window_ctx *const context);

// --------------------------------------------------
// Vtable functions for creating the default window |
// --------------------------------------------------
static ULONG
HandlerAddRef(IUnknown *)
{
        return ++g_handlerRefCount;
}

static ULONG
HandlerRelease(IUnknown *)
{
        return --g_handlerRefCount;
}

static HRESULT
HandlerQueryInterface(IUnknown *this, const IID *riid, void **ppvObject)
{
        *ppvObject = this;
        HandlerAddRef(this);
        return S_FALSE;
}

static ICoreWebView2WebMessageReceivedEventHandlerVtbl
        messageReceivedEventHandlerVtbl = {
                .AddRef = (void *)HandlerAddRef,
                .Release = (void *)HandlerRelease,
                .QueryInterface = (void *)HandlerQueryInterface,
                .Invoke = (void *)WebView2WebMessageReceivedEventHandlerInvoke};

static ICoreWebView2WebMessageReceivedEventHandler messageReceivedEventHandler =
        {.lpVtbl = &messageReceivedEventHandlerVtbl};

static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl
        completedHandlerVtbl = {.AddRef = (void *)HandlerAddRef,
                                .Release = (void *)HandlerRelease,
                                .QueryInterface = (void *)HandlerQueryInterface,
                                .Invoke = (void *)HandlerInvoke};

static ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
        completedHandler = {.lpVtbl = &completedHandlerVtbl};

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl
        envHandlerVtbl = {.AddRef = (void *)HandlerAddRef,
                          .Release = (void *)HandlerRelease,
                          .QueryInterface = (void *)HandlerQueryInterface,
                          .Invoke = (void *)HandlerInvoke};

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler envHandler = {
        .lpVtbl = &envHandlerVtbl};

static ICoreWebView2ContextMenuRequestedEventHandlerVtbl
        contextMenuEventHandlerVtbl = {
                .AddRef = (void *)HandlerAddRef,
                .Release = (void *)HandlerRelease,
                .QueryInterface = (void *)HandlerQueryInterface,
                .Invoke = (void *)WebView2ContextMenuRequestEventHandlerInvoke};

static ICoreWebView2ContextMenuRequestedEventHandler contextMenuEventHandler = {
        .lpVtbl = &contextMenuEventHandlerVtbl};

static ICoreWebView2CustomItemSelectedEventHandlerVtbl
        closeMenuSelectedHandlerVtbl = {
                .AddRef = (void *)HandlerAddRef,
                .Release = (void *)HandlerRelease,
                .QueryInterface = (void *)HandlerQueryInterface,
                .Invoke = (void *)OnCloseContextItemMenuSelected};

static ICoreWebView2CustomItemSelectedEventHandler closeMenuSelectedHandler = {
        .lpVtbl = &closeMenuSelectedHandlerVtbl};

static ICoreWebView2CustomItemSelectedEventHandlerVtbl
        moveMenuSelectedHandlerVtbl = {
                .AddRef = (void *)HandlerAddRef,
                .Release = (void *)HandlerRelease,
                .QueryInterface = (void *)HandlerQueryInterface,
                .Invoke = (void *)OnMoveContextItemMenuSelected};

static ICoreWebView2CustomItemSelectedEventHandler moveMenuSelectedHandler = {
        .lpVtbl = &moveMenuSelectedHandlerVtbl};

/**
 * @brief Hashes the given string into a number
 * @param src String to be hashed
 * @returns The hashed string
 */
static size_t
GetHashFromString(const char *const src)
{
        size_t hash = HASH_PRIME;
        const size_t length = strlen(src);
        for (size_t i = 0; i < length; i++)
        {
                hash = ((hash << 5) + hash) + src[i];
        }
        return hash % MAX_WIDGETS;
}

/**
 * @brief Search and replaces all chars in a string
 * @param srcDest Source string to be edited
 * @param target Delimiter to search for
 * @param replace Char to replace the delimiter
 */
static void
ReplaceChars(char *const srcDest, const char target, const char replace)
{
        const size_t len = strlen(srcDest);
        for (size_t i = 0; i < len; i++)
        {
                srcDest[i] = srcDest[i] == target ? replace : srcDest[i];
        }
}

/**
 * @brief Trimps the start of a string by a specified offset
 * @param src Original string to edit
 * @param dest Where the output will be saved to
 * @param offset offset to trimp up to
 * @returns True if successful, else false
 */
static bool
TrimStart(const char *const src, char *const dest, const size_t offset)
{
        const size_t len = strlen(src);
        if (snprintf(dest, (len - offset) + 1, "%s", &src[offset]) < 0)
        {
                fprintf(stderr, "Failed to copy string into buffer\n");
                return false;
        }
        return true;
}

/*
 * @brief Triggers when the close menu item is selected
 * @param sender Object that triggered the event
 * @param args Arguments of the event
 */
static HRESULT
OnCloseContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                               IUnknown *const args)
{
        const HWND handle = g_hWndTable[g_hWndSelectedHash];
        if (DestroyWindow(handle) == 0)
        {
                fprintf(stderr, "Failed to destroy window\n");
                return S_FALSE;
        }
        return S_OK;
}

/*
 * @brief Triggers when the move menu item is selected. Its purpose is to track
 * the position of the user cursor and move the window accordingly.
 *
 * @param sender Object that triggered the event
 * @param args Arguments of the event
 * @returns 0 if successful, else some other number
 */
static HRESULT
OnMoveContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                              IUnknown *const args)
{
        const HWND handle = g_hWndTable[g_hWndSelectedHash];
        POINT position;
        while (!(KEY_STATE_HELD & GetAsyncKeyState(VK_LBUTTON)))
        {
                if (!GetCursorPos(&position))
                {
                        fprintf(stderr, "Failed to get cursor position\n");
                        return S_FALSE;
                }

                if (!SetWindowPos(handle,
                                  0,
                                  position.x,
                                  position.y,
                                  0,
                                  0,
                                  SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE))
                {
                        fprintf(stderr, "Failed to move window\n");
                        return S_FALSE;
                }
        }
        return S_OK;
}

/**
 * @brief Adds a new item to the Context Menu
 * @param environment Default WebView2 Environment
 * @param items Pointer to the items object
 * @param itemsCount Pointer to the count of items
 * @param token Pointer to the registration token
 * @param label Name of the context menu item
 * @param eventHandler Pointer to the event handler function
 * @returns A pointer to the new context menu item, or nullptr on failure
 */
static ICoreWebView2ContextMenuItem *
AddContextMenuItem(
        ICoreWebView2Environment10 *const environment,
        ICoreWebView2ContextMenuItemCollection *const items,
        UINT32 *const itemsCount,
        EventRegistrationToken *const token,
        ICoreWebView2CustomItemSelectedEventHandler *const eventHandler,
        const wchar_t *const label)
{
        ICoreWebView2ContextMenuItem *menuItem = nullptr;
        if (environment->lpVtbl->CreateContextMenuItem(
                    environment,
                    label,
                    nullptr,
                    COREWEBVIEW2_CONTEXT_MENU_ITEM_KIND_COMMAND,
                    &menuItem) != S_OK)
        {
                fprintf(stderr, "Failed to add menu item\n");
                return nullptr;
        }

        if (menuItem->lpVtbl->remove_CustomItemSelected(menuItem, *token) !=
            S_OK)
        {
                fprintf(stderr, "Failed to unsubscribe from event\n");
                return nullptr;
        }

        if (menuItem->lpVtbl->add_CustomItemSelected(
                    menuItem, eventHandler, token) != S_OK)
        {
                fprintf(stderr, "Failed to add event handler\n");
                return nullptr;
        }

        if (items->lpVtbl->InsertValueAtIndex(items, *itemsCount, menuItem) !=
            S_OK)
        {
                fprintf(stderr, "Failed to insert item to list\n");
                return nullptr;
        }

        return menuItem;
}

/**
 * @brief Gets called whenver the Context Menu opens
 * @param this Reference to the event handler
 * @param sender Webview2 core object
 * @param args Arguments of the caller
 * @returns 0 if successful, else any other value
 */
static HRESULT
WebView2ContextMenuRequestEventHandlerInvoke(
        ICoreWebView2ContextMenuRequestedEventHandler *const this,
        ICoreWebView2 *const sender,
        ICoreWebView2ContextMenuRequestedEventArgs *const args)
{
        HRESULT status = S_OK;
        ICoreWebView2ContextMenuItemCollection *items = nullptr;
        if (args->lpVtbl->get_MenuItems(args, &items) != S_OK)
        {
                fprintf(stderr, "Failed to get menu items\n");
                return S_FALSE;
        }

        UINT32 itemsCount;
        if (items->lpVtbl->get_Count(items, &itemsCount) != S_OK)
        {
                fprintf(stderr, "Failed to get items count\n");
                status = S_FALSE;
                goto cleanup;
        }

        WCHAR pathWcharPtr[BUFFSIZE];
        LPWSTR pathPtr = pathWcharPtr;
        if (sender->lpVtbl->get_Source(sender, &pathPtr) != S_OK)
        {
                fprintf(stderr, "Failed to get path to HTML file\n");
                status = S_FALSE;
                goto cleanup;
        }

        char pathCharPtr[BUFFSIZE];
        if (wcstombs(pathCharPtr, pathPtr, BUFFSIZE) < 0)
        {
                fprintf(stderr, "Failed to convert wide to normal char\n");
                status = S_FALSE;
                goto cleanup;
        }

        ReplaceChars(pathCharPtr, widget_char_slash, widget_char_b_slash);
        char out[BUFFSIZE];
        if (!TrimStart(pathCharPtr, out, FILE_PREFIX_OFFSET))
        {
                fprintf(stderr, "Failed to trim start of the path\n");
                status = S_FALSE;
                goto cleanup;
        }

        const size_t hash = GetHashFromString(out);
        g_hWndSelectedHash = hash;
        ICoreWebView2Environment10 *environment =
                (ICoreWebView2Environment10 *)g_env;

        ICoreWebView2ContextMenuItem *moveBtnMenuItem = nullptr;
        if ((moveBtnMenuItem = AddContextMenuItem(environment,
                                                  items,
                                                  &itemsCount,
                                                  &g_moveEventHandlerToken,
                                                  &moveMenuSelectedHandler,
                                                  LBL_CTX_MENU_MOVE)) ==
            nullptr)
        {
                fprintf(stderr, "Failed to add move menu item\n");
                status = S_FALSE;
                goto cleanup;
        }

        ICoreWebView2ContextMenuItem *closeBtnMenuItem = nullptr;
        if ((closeBtnMenuItem = AddContextMenuItem(environment,
                                                   items,
                                                   &itemsCount,
                                                   &g_closeEventHandlerToken,
                                                   &closeMenuSelectedHandler,
                                                   LBL_CTX_MENU_CLOSE)) ==
            nullptr)
        {
                fprintf(stderr, "Failed to add close menu item\n");
                status = S_FALSE;
                goto cleanup;
        }

cleanup:
        if (closeBtnMenuItem != nullptr)
        {
                closeBtnMenuItem->lpVtbl->Release(closeBtnMenuItem);
        }

        if (moveBtnMenuItem != nullptr)
        {
                moveBtnMenuItem->lpVtbl->Release(moveBtnMenuItem);
        }

        if (items != nullptr)
        {
                items->lpVtbl->Release(items);
        }

        return status;
}

/**
 * @brief Gets the value of a meta tag as a string
 * @param filenaem Path to the file
 * @param src Name of the tag to search for
 * @param dest Destination string to save the result to
 * @returns Whether the function was successful or not
 */
static bool
GetMetaTagValue(const char *const filename,
                const char *const src,
                char *const dest,
                const size_t destLen)
{
        char path[destLen];
        if (!TrimStart(filename, path, HANDLE_PREFIX_OFFSET))
        {
                fprintf(stderr, "Failed to strip the prefix from filename\n");
                return false;
        }

        char content[destLen];
        if (ww_get_file_content(path, content, destLen) == true)
        {
                fprintf(stderr, "Failed to get contents of HTML file\n");
                return false;
        }

        const size_t len = strlen(content);
        bool isTarget = false;
        bool isMeta = false;
        bool isName = false;
        bool isValue = false;
        ssize_t start = -1, end = -1;
        const char *const metaTag = "<meta";
        const char *const nameAttr = "name=";
        const char *const valueAttr = "value=";
        const size_t metaTagLen = strlen(metaTag);
        const size_t nameAttrLen = strlen(nameAttr);
        const size_t valueAttrLen = strlen(valueAttr);
        for (size_t i = 0, j = 0; i < len; i++)
        {
                const char c = content[i];

                // Put values between start and end inside this variable
                char value[destLen];
                size_t valueLen = 0;
                if (start > -1 && end > -1)
                {
                        valueLen = end - start;
                        memcpy(value, &content[start], valueLen);
                        value[valueLen] = '\0';
                }

                // Searching for the Meta tag
                if (c == metaTag[j] && !isMeta)
                {
                        j++;
                }

                if (j >= metaTagLen && !isMeta)
                {
                        j = 0;
                        isMeta = true;
                        continue;
                }

                if (!isMeta)
                {
                        continue;
                }

                // Searching for meta close tag then resetting all flags
                if (c == widget_char_gt)
                {
                        isTarget = false;
                        isMeta = false;
                        isName = false;
                        isValue = false;
                        start = -1;
                        end = -1;
                        continue;
                }

                // Searching for the name attribute
                if (c == nameAttr[j] && !isName)
                {
                        j++;
                }

                if (j >= nameAttrLen && !isName)
                {
                        j = 0;
                        isName = true;
                        continue;
                }

                if (!isName)
                {
                        continue;
                }

                // Getting the value of name between quotes
                if (c == widget_char_quote && start < 0 && !isTarget)
                {
                        start = i + 1;
                        continue;
                }

                if (c == widget_char_quote && end < 0 && !isTarget)
                {
                        end = i;
                        continue;
                }

                if ((start < 0 || end < 0) && !isTarget)
                {
                        continue;
                }

                if (valueLen > 0 && !isTarget)
                {
                        isTarget = strcmp(value, src) == 0;
                        j = 0;
                        start = -1;
                        end = -1;
                        valueLen = 0;
                        continue;
                }

                // Searching for the value attribute
                if (c == valueAttr[j] && !isValue)
                {
                        j++;
                }

                if (j >= valueAttrLen && !isValue)
                {
                        j = 0;
                        isValue = true;
                        continue;
                }

                if (!isValue)
                {
                        continue;
                }

                // Getting the value of the attribute "value" between quotes
                if (c == widget_char_quote && start < 0)
                {
                        start = i + 1;
                        continue;
                }

                if (c == widget_char_quote && end < 0)
                {
                        end = i;
                        continue;
                }

                if (valueLen <= 0)
                {
                        continue;
                }

                memcpy(dest, value, destLen);
                dest[destLen] = '\0';

                return true;
        }

        return false;
}

/**
 * @brief Gets a set of values separated by a whitespace from a string
 * @param src Source containing the numeric values
 * @param a First numeric value
 * @param b Second numeric value
 * @retuns true if successful, else false
 */
static bool
Get2DValue(const char *const src, size_t *const a, size_t *const b)
{
        char substrA[512], substrB[512];
        const size_t size = strlen(src);
        size_t spaceIndex = 0;
        for (size_t i = 0; i < size; i++)
        {
                const char c = src[i];
                if (c == widget_char_space)
                {
                        memcpy(substrA, src, i);
                        substrA[i] = '\0';

                        const size_t substrBLen = size - i - 1;
                        memcpy(substrB, &src[i + 1], substrBLen);
                        substrB[substrBLen] = '\0';

                        *a = strtol(substrA, nullptr, 10);
                        *b = strtol(substrB, nullptr, 10);

                        return true;
                }
        }

        return false;
}

/**
 * @brief Handles incoming events from JavaScript
 * @param handler Received event handler
 * @param webview Context of the webview
 * @param args Arguments of the event handler
 */
static HRESULT
WebView2WebMessageReceivedEventHandlerInvoke(
        ICoreWebView2WebMessageReceivedEventHandler *const handler,
        ICoreWebView2 *const webview,
        ICoreWebView2WebMessageReceivedEventArgs *const args)
{
        // Getting the message sent from JavaScript
        LPWSTR wMessage;
        args->lpVtbl->TryGetWebMessageAsString(args, &wMessage);

        // Now we need to conver this message to the standard C char str
        size_t ret = WideCharToMultiByte(
                CP_UTF8, 0, wMessage, -1, nullptr, 0, nullptr, nullptr);
        if (ret == 0)
        {
                fprintf(stderr, "Error getting default size for utf8 string\n");
                return S_FALSE;
        }

        char cMessage[BUFFSIZE];
        ret = WideCharToMultiByte(
                CP_UTF8, 0, wMessage, -1, cMessage, ret, nullptr, nullptr);
        if (ret == 0)
        {
                fprintf(stderr, "Failed to convert string to UTF-8\n");
                return S_FALSE;
        }

        // The result is returned as a JSON string, parse the string here
        string_json_t dataString;
        if (ConvertStringToJson(cMessage, &dataString) != FUNC_SUCCESS)
        {
                fprintf(stderr, "Error converting to JSON\n");
                return S_FALSE;
        }

        string_json_t eventIdStr;
        if (GetProperty(dataString, &eventIdStr, "eventId") != FUNC_SUCCESS)
        {
                fprintf(stderr, "Event ID could not be found\n");
                return S_FALSE;
        }

        string_json_t argsStr;
        if (GetProperty(dataString, &argsStr, "args") != FUNC_SUCCESS)
        {
                fprintf(stderr, "Event arguments could not be found\n");
                return S_FALSE;
        }

        // Each event has an associated id to trigger default functions. Some
        // may also have arguments with relevant information of what
        // instructions to perform
        int eventId;
        if (ConvertJsonToStandardType(eventIdStr, JSON_INT, &eventId) !=
            FUNC_SUCCESS)
        {
                fprintf(stderr, "Error converting JSON to Integer type\n");
                return S_FALSE;
        }

        char path[BUFFSIZE];
        if (ConvertJsonToStandardType(argsStr, JSON_CHAR_ARR, &path) !=
            FUNC_SUCCESS)
        {
                fprintf(stderr, "Error converting JSON to char array type\n");
                return S_FALSE;
        }

        switch (eventId)
        {
        case EVT_OPEN_DEFAULT_DIR:
                if (!OpenDefaultDirectory())
                {
                        fprintf(stderr, "Failed to open default directory\n");
                        return EXIT_REASON_IO_FAILURE;
                }
                break;
        case EVT_GET_WGT_FILENAMES:
                if (!AppendWidgetToBrowserList(webview))
                {
                        fprintf(stderr,
                                "Could not add local widgets to the web "
                                "list\n");
                        return EXIT_REASON_IO_FAILURE;
                }
                break;
        case EVT_OPEN_WGT_FILENAME:
                g_widgets++;

                ww_window_ctx context = {.width = DEF_WIDTH,
                                         .height = DEF_HEIGHT,
                                         .prevWidth = DEF_PREV_WIDTH,
                                         .prevHeight = DEF_PREV_HEIGHT,
                                         .x = DEF_X,
                                         .y = DEF_HEIGHT,
                                         .child = DEF_CHILD,
                                         .top_most = DEF_TOPMOST,
                                         .opacity = DEF_OPACITY,
                                         .radius = DEF_RADIUS};

                memcpy(context.filename, path, argsStr.length);

                char buf[BUFFSIZE] = {};
                if (GetMetaTagValue(path, TAG_APP_NAME, buf, BUFFSIZE))
                {
                        memcpy(context.title, buf, BUFFSIZE);
                        context.title[BUFFSIZE - 1] = '\0';
                }

                if (GetMetaTagValue(path, TAG_WIN_SIZE, buf, BUFFSIZE))
                {
                        size_t width, height;
                        const bool isSet = Get2DValue(buf, &width, &height);
                        context.width = isSet ? width : DEF_WIDTH;
                        context.height = isSet ? height : DEF_HEIGHT;
                }

                if (GetMetaTagValue(path, TAG_WIN_LOCATION, buf, BUFFSIZE))
                {
                        size_t x, y;
                        const bool isSet = Get2DValue(buf, &x, &y);
                        context.x = isSet ? x : DEF_X;
                        context.y = isSet ? y : DEF_Y;
                }

                if (GetMetaTagValue(path, TAG_WIN_PREV, buf, BUFFSIZE))
                {
                        size_t width, height;
                        const bool isSet = Get2DValue(buf, &width, &height);
                        context.prevWidth = isSet ? width : DEF_X;
                        context.prevHeight = isSet ? height : DEF_Y;
                }

                if (GetMetaTagValue(path, TAG_APP_TOPMOST, buf, BUFFSIZE))
                {
                        context.top_most = strcmp(buf, "true") == 0;
                }

                if (GetMetaTagValue(path, TAG_WIN_OPACITY, buf, BUFFSIZE))
                {
                        const double opacity = strtod(buf, nullptr);
                        context.opacity = opacity;
                }

                if (GetMetaTagValue(path, TAG_WIN_BORD_RAD, buf, BUFFSIZE))
                {
                        const double radius = strtod(buf, nullptr);
                        context.radius = radius;
                }

                if (!create_widget_window(&context))
                {
                        fprintf(stderr, "Failed to create child widget\n");
                }
                break;
        }

        return S_FALSE;
}

/**
 * @brief Appends the widget to the global list of browsers
 * @param webview Webview component
 */
static bool
AppendWidgetToBrowserList(ICoreWebView2 *const webview)
{
        char app_dir[BUFFSIZE];
        if (ww_default_widgets_dir(app_dir) == true)
        {
                fprintf(stderr, "Failed to get default app directory\n");
                return false;
        }

        size_t count = 0;
        char widgets[MAX_WIDGETS][BUFFSIZE];
        if ((count = ww_get_files_from_dir(app_dir, widgets, MAX_WIDGETS)) == 0)
        {
                fprintf(stderr, "Failed to read directory\n");
                return false;
        }

        size_t j = 0;
        const size_t len = (MAX_WIDGETS * BUFFSIZE) + (MAX_WIDGETS * 2) +
                           (MAX_WIDGETS - 1) + 2;
        char list[len];
        for (size_t i = 0; i < count; i++)
        {
                const size_t offset = 3;
                const size_t size = strlen(widgets[i]);
                char temp[size + offset];
                if (snprintf(temp, sizeof(temp) + 1, "'%s',", widgets[i]) < 0)
                {
                        fprintf(stderr, "Failed to put widget into list\n");
                        return false;
                }

                memcpy(&list[j], temp, sizeof(temp));
                j = j + sizeof(temp);
        }
        list[j - 1] = '\0';

        const size_t offset = 4;
        const size_t list_len = strlen(list) + offset;
        char json[list_len];
        if (snprintf(json, list_len + 1, "\"[%s]\"", list) < 0)
        {
                fprintf(stderr, "Failed to convert list to JSON object\n");
                return false;
        }
        json[list_len] = '\0';

        char command[EXTBUFFSIZE];
        if (snprintf(command, EXTBUFFSIZE, "addWidgets(%s)", json) < 0)
        {
                fprintf(stderr, "Failed to build correct command string\n");
                return false;
        }

        // Converting to wide characters from UTF-8
        wchar_t wCommand[EXTBUFFSIZE];
        MultiByteToWideChar(CP_ACP, 0, command, -1, wCommand, BUFFSIZE);
        webview->lpVtbl->ExecuteScript(webview, wCommand, nullptr);

        return true;
}

/**
 * @brief Opens the default directory where the widgets are located
 * @returns `true` if successful `false` if failed
 */
static bool
OpenDefaultDirectory()
{
        char dir[BUFFSIZE];
        if (ww_default_widgets_dir(dir) == true)
        {
                fprintf(stderr,
                        "Failed to create the default widgets directory\n");
                return false;
        }

        if (ww_open_folder(dir) == true)
        {
                fprintf(stderr, "Failed to open folder to directory\n");
                return false;
        }

        return true;
}

/**
 * @brief Is called when the webview context is ready
 * @param IUnknown Interface invoked
 * @param erroCode Status code
 * @param arg WebView2 Controller
 */
static HRESULT
HandlerInvoke(const IUnknown *const this,
              const HRESULT errorCode,
              const ICoreWebView2Controller *const arg)
{
        // Initializing the environment if it isn't already initialized
        if (!g_envCreated)
        {
                g_envCreated = true;
                g_env = (ICoreWebView2Environment *)arg;

                const HWND handle = g_hWnds[g_widgets];
                if (g_env->lpVtbl->CreateCoreWebView2Controller(
                            g_env, handle, &completedHandler) != S_OK)
                {
                        fprintf(stderr, "Failed to create controller\n");
                        return S_FALSE;
                }
                return S_FALSE;
        }

        g_controllers[g_widgets] = (ICoreWebView2Controller *)arg;
        ICoreWebView2Controller *controller = g_controllers[g_widgets];
        if (controller != nullptr)
        {
                if (controller->lpVtbl->get_CoreWebView2(
                            controller, &g_windows[g_widgets]) != S_OK)
                {
                        fprintf(stderr, "Failed to get webview core\n");
                        return S_FALSE;
                }

                controller->lpVtbl->AddRef(controller);
        }

        // Browser settings
        ICoreWebView2Settings *settings;
        ICoreWebView2_11 *window = (ICoreWebView2_11 *)g_windows[g_widgets];
        if (window->lpVtbl->get_Settings(window, &settings) != S_OK)
        {
                fprintf(stderr, "Failed to get settings\n");
                return S_FALSE;
        }

        if (settings->lpVtbl->put_IsScriptEnabled(settings, true) != S_OK)
        {
                fprintf(stderr, "Failed to enable script\n");
                return S_FALSE;
        }

        if (settings->lpVtbl->put_AreDefaultScriptDialogsEnabled(settings,
                                                                 true) != S_OK)
        {
                fprintf(stderr, "Failed to enable dialogs script\n");
                return S_FALSE;
        }

        if (settings->lpVtbl->put_IsWebMessageEnabled(settings, true) != S_OK)
        {
                fprintf(stderr, "Failed to enable messages\n");
                return S_FALSE;
        }

        if (settings->lpVtbl->put_AreDevToolsEnabled(settings, true) != S_OK)
        {
                fprintf(stderr, "Failed to enable dev tools\n");
                return S_FALSE;
        }

        if (settings->lpVtbl->put_AreDefaultContextMenusEnabled(settings,
                                                                true) != S_OK)
        {
                fprintf(stderr, "Failed to enable context menu\n");
                return S_FALSE;
        }

        if (settings->lpVtbl->put_IsStatusBarEnabled(settings, true) != S_OK)
        {
                fprintf(stderr, "Failed to enable status bar\n");
                return S_FALSE;
        }

        const HWND hWnd = g_hWnds[g_widgets];
        RECT bounds;
        if (!GetClientRect(hWnd, &bounds))
        {
                fprintf(stderr, "Failed to get client rect\n");
                return S_FALSE;
        }

        if (controller->lpVtbl->put_Bounds(controller, bounds) != S_OK)
        {
                fprintf(stderr, "Failed to put bounds\n");
                return S_FALSE;
        }

        wchar_t wTpmlPath[BUFFSIZE];
        if (MultiByteToWideChar(
                    CP_ACP, 0, g_tmplPath, -1, wTpmlPath, BUFFSIZE) == 0)
        {
                fprintf(stderr, "Failed to convert char array to wide char\n");
                return S_FALSE;
        }

        /**
         *  Registering event handlers
         *  1- Event handler to receive JavaScript messages
         *  2- Event handler to detect when the context menu opens
         */
        EventRegistrationToken token;
        if (window->lpVtbl->add_WebMessageReceived(
                    window, &messageReceivedEventHandler, &token) != S_OK)
        {
                fprintf(stderr, "Failed to add js message event handler\n");
                return S_FALSE;
        }

        if (window->lpVtbl->add_ContextMenuRequested(
                    window, &contextMenuEventHandler, &token) != S_OK)
        {
                fprintf(stderr, "Failed to addcontext menu event handler\n");
                return S_FALSE;
        }

        if (window->lpVtbl->Navigate(window, wTpmlPath) != S_OK)
        {
                fprintf(stderr, "Failed to navigate to the URI\n");
                return S_FALSE;
        }

        return S_FALSE;
}

/**
 * @brief Finds the target HWND in the list of global g_hWnds
 * @param target The target to be searched for
 * @returns The index where the target was found; on error -1
 */
static ssize_t
FindHwnd(const HWND target)
{
        for (size_t i = 0; i < MAX_WIDGETS; i++)
        {
                if (target == g_hWnds[i])
                {
                        return i;
                }
        }
        return -1;
}

/**
 * @brief Callback function used by the window cration process of the
 * default messages
 * @param hwnd Default handle of th application
 * @param uMsg Code of the event triggered
 * @param WPARAM wParam
 * @param lParam lParam
 */
LRESULT CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
        switch (uMsg)
        {
        case WM_DESTROY:
                const ssize_t indexHwnd = FindHwnd(hwnd);
                g_hWnds[indexHwnd] = g_hWnds[g_widgets];
                g_controllers[indexHwnd] = g_controllers[g_widgets];
                g_windows[indexHwnd] = g_windows[g_widgets];
                g_hWndTable[g_hWndSelectedHash] = nullptr;

                if (g_widgets-- > 0 && indexHwnd != 0)
                {
                        return S_FALSE;
                }

                PostQuitMessage(S_FALSE);
                return S_FALSE;
        case WM_SIZE:
                if (g_controllers[g_widgets] != nullptr)
                {
                        RECT bounds;
                        GetClientRect(g_hWnds[g_widgets], &bounds);
                        g_controllers[g_widgets]->lpVtbl->put_Bounds(
                                g_controllers[g_widgets], bounds);
                }
                return S_FALSE;
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

/**
 * @brief Default event loop of th application
 * @returns The state of the function at its conclusion
 */
static bool
event_loop()
{
        MSG msg;
        BOOL bRet;
        while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
        {
                // An error occured
                if (bRet == -1)
                {
                        break;
                }
                else
                {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                }
        }
        return false;
}

/**
 * @brief Sets the transparency of the window
 * @param hWnd Handle to the window
 * @param alpha Value between 0 to 1 for opacity
 */
static bool
SetWindowOpacity(const HWND hWnd, const double alpha)
{
        LONG style = GetWindowLong(hWnd, GWL_EXSTYLE);
        style |= WS_EX_LAYERED;

        if (SetWindowLong(hWnd, GWL_EXSTYLE, style) == 0)
        {
                fprintf(stderr, "Failed to change the window attribute\n");
                return false;
        }

        const byte byteAlpha = alpha * MAX_ALPHA;
        if (SetLayeredWindowAttributes(hWnd, 0, byteAlpha, LWA_ALPHA) == 0)
        {
                fprintf(stderr, "Failed to set window attributes\n");
                return false;
        }

        return true;
}

/**
 * @brief Set the border radius for the corners of the window
 * @param hWnd Handle to the window
 * @param context Window context of the widget
 * @returns True if successful, else false
 */
static bool
SetWindowBorderRadius(const HWND hWnd, const ww_window_ctx *const context)
{
        const size_t width = context->width;
        const size_t height = context->height;
        const size_t radius = context->radius;
        const bool child = context->child;
        if (radius <= 0 || !child)
        {
                return true;
        }

        const HRGN hRgn =
                CreateRoundRectRgn(0, 0, width, height, radius, radius);
        if (hRgn == nullptr)
        {
                fprintf(stderr, "Faield to create round rect\n");
                return false;
        }

        if (SetWindowRgn(hWnd, hRgn, true) == 0)
        {
                fprintf(stderr, "Failed to set the window region\n");
                return false;
        }

        DeleteObject(hRgn);

        return true;
}

/**
 * @brief Creates a new window
 * @param context Context of the window with all options
 * @returns true if successful, else false on failure
 */
static bool
create_widget_window(ww_window_ctx *const context)
{
        bool status = true;

        if (context->child)
        {
                TrimStart(context->filename,
                          context->filename,
                          HANDLE_PREFIX_OFFSET);
        }

        ReplaceChars(context->filename, widget_char_slash, widget_char_b_slash);
        const size_t hash = GetHashFromString(context->filename);
        if (g_hWndTable[hash] != nullptr)
        {
                fprintf(stderr, "Warning: Only one widget type once\n");
                status = true;
                goto cleanup;
        }

        const size_t titleLen = strlen(context->title);
        if (titleLen >= BUFFSIZE)
        {
                fprintf(stderr, "Window name was bigger than expectd\n");
                status = false;
                goto cleanup;
        }
        memcpy(g_tmplName, context->title, titleLen);
        g_tmplName[titleLen] = '\0';

        const size_t filenameLen = strlen(context->filename);
        if (filenameLen >= BUFFSIZE)
        {
                fprintf(stderr, "Path to the file location was too large\n");
                status = false;
                goto cleanup;
        }
        memcpy(g_tmplPath, context->filename, filenameLen);
        g_tmplPath[filenameLen] = '\0';

        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = g_hInstance;
        wc.lpszClassName = CLASS_NAME;

        if (RegisterClass(&wc) == 0)
        {
                fprintf(stderr, "Failed to register class\n");
        }

        HWND *const handle = &g_hWnds[g_widgets];
        const size_t wsStyle = context->child ? WS_POPUP : WS_OVERLAPPEDWINDOW;
        if ((*handle = CreateWindowEx(WS_EX_LAYERED,
                                      CLASS_NAME,
                                      g_tmplName,
                                      wsStyle,
                                      context->x,
                                      context->y,
                                      context->width,
                                      context->height,
                                      nullptr,
                                      nullptr,
                                      g_hInstance,
                                      nullptr)) == nullptr)
        {
                fprintf(stderr, "Failed to create window\n");
                status = false;
                goto cleanup;
        }

        g_hWndTable[hash] = *handle;
        ShowWindow(*handle, g_nCmdShow);

        if (!SetWindowBorderRadius(*handle, context))
        {
                fprintf(stderr, "Failed to set window radius\n");
                status = false;
                goto cleanup;
        }

        if (!SetWindowOpacity(*handle, context->opacity))
        {
                fprintf(stderr, "Failed to set window opacity\n");
                status = false;
                goto cleanup;
        }

        if (!UpdateWindow(*handle))
        {
                fprintf(stderr, "Failed to update window\n");
                status = false;
                goto cleanup;
        }

        if (!g_envCreated)
        {
                if (CreateCoreWebView2EnvironmentWithOptions(
                            nullptr, nullptr, nullptr, &envHandler) != S_OK)
                {
                        fprintf(stderr, "Failed to create environment\n");
                        status = false;
                        goto cleanup;
                }
                return true;
        }

        if (g_env->lpVtbl->CreateCoreWebView2Controller(
                    g_env, *handle, &completedHandler) != S_OK)
        {
                fprintf(stderr, "Failed to create controller\n");
                status = false;
                goto cleanup;
        }

cleanup:
        if (!status)
        {
                CoUninitialize();
        }
        return status;
}

/*
 * @brief Initializing the main window of the application. It will be
 * responsible for spawning children widgets.
 * @returns The status code of the function at its conclusion
 */
bool
ww_init_main(HINSTANCE hInstance,
             int nCmdShow,
             ww_window_ctx *const context,
             ww_widget_ctx *const widgets)
{
        g_hInstance = hInstance;
        g_nCmdShow = nCmdShow;

        if (FAILED(CoInitialize(nullptr)))
        {
                fprintf(stderr, "COM Failed to initialize\n");
                return true;
        }

        if (!create_widget_window(context))
        {
                fprintf(stderr, "WebView2 Failed to create a window\n");
                return false;
        }

        return event_loop();
}
