#include "filesystem.h"
#include "global.h"
#include "utils.h"
#include "widget.h"
#include "json.h"
#include "parser.h"

#include <WebView2.h>
#include <stdio.h>
#include <unistd.h>
#include <ddraw.h>
#include <pthread.h>
#include <dwmapi.h>

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000
#endif

/**
 * @brief Whether the function call is not recoverable
 */
#define BAD(expression) ((expression) > FUNC_STATUS_USR_ERR)

static constexpr char PROG_NAME[] = "WinWidgets";
static constexpr uint8_t PROG_NAME_SIZE = lengthof(PROG_NAME);

static constexpr char PROG_SEM_VER[] = "2.0.0";
static constexpr char PROG_START_PATH[] =
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";

static constexpr char CONFIG_NAME[] = "\\config.json";
static constexpr char FAVICON_PATH[] = "assets/icons/favicon.ico";

static constexpr wchar_t LBL_CTX_MENU_MOVE[] = L"Toggle move";
static constexpr wchar_t LBL_CTX_MENU_CLOSE[] = L"Close";
static constexpr wchar_t LBL_CTX_MENU_TOP_MOST[] = L"Toggle always on top";

static constexpr uint8_t PARENT_INDEX = 0;
static constexpr uint8_t MAX_ALPHA = UINT8_MAX;
static constexpr uint8_t HASH_PRIME = 31;
static constexpr uint8_t HANDLE_PREFIX_OFFSET = 7;
static constexpr uint8_t FILE_PREFIX_OFFSET = 8;

static constexpr uint16_t KEY_STATE_HELD = SHRT_MAX + 1;
static constexpr uint16_t PROG_WM_ICONNOTIFY = WM_APP + 1;

static constexpr uint8_t UID_SYSTRAY_EXIT = 0;
static constexpr uint8_t UID_SYSTRAY_CLOSE = 1;
static constexpr char LBL_SYSTRAY_EXIT[] = "Exit application";
static constexpr char LBL_SYSTRAY_CLOSE[] = "Close widgets";

typedef enum : uint8_t
{
        STATE_ORDER_BOTTOM,
        STATE_ORDER_TOP,
} state_order_t;

typedef enum : uint8_t
{
        EVT_OPEN_DEFAULT_DIR,
        EVT_GET_WGT_FILENAMES,
        EVT_OPEN_WGT_FILENAME,
        EVT_TOGGLE_SETTING,
        EVT_THEME_CHANGED,
} widget_events_t;

typedef enum : uint8_t
{
        FUNC_STATUS_OK,
        FUNC_STATUS_USR_ERR,
        FUNC_STATUS_WBV_ERR,
        FUNC_STATUS_MEM_ERR,
        FUNC_STATUS_ERR
} func_status_t;

typedef enum : uint8_t
{
        APPLICATION_SETTING_FULLSCREEN,
        APPLICATION_SETTING_AUTOSTART,
        APPLICATION_SETTING_START_WIDGETS,
} application_runtime_setting_t;

typedef enum : uint8_t
{
        WEBVIEW_CONTEXT_MENU_KIND_COMMAND,
        WEBVIEW_CONTEXT_MENU_KIND_CHECKBOX
} webview_context_menu_kind_t;

typedef struct
{
        ww_window_ctx context;
        ICoreWebView2Controller *controller;
        ICoreWebView2 *window;
        HWND hWnd;
} webview_widget_t;

typedef struct
{
        const size_t x, y;
        const bool topMost;
        char filename[BUFFSIZE];
} stack_item_t;

typedef struct
{
        const bool widgetAutostart;
        const bool fullscreenHide;
        const bool appAutostart;
} application_settings_t;

// ----------------------------------------------------------
// Global variables to control the state of the application |
// ----------------------------------------------------------
static ICoreWebView2Environment *g_env = nullptr;

static application_settings_t g_settings;
static webview_widget_t g_widgets[MAX_WIDGETS] = {};
static HWND g_parentHwnd = nullptr;
static HWND g_hWndTable[MAX_WIDGETS] = {};

static size_t g_hWndSelectedHash = {};
static size_t g_nCmdShow = {};
static size_t g_widgetCount = 0;

static HINSTANCE g_hInstance = nullptr;
static ULONG g_handlerRefCount = 0;
static bool g_silentMode = false;
static volatile bool g_dirChangesDetected = false;

static stack_item_t g_stack[MAX_WIDGETS];
static volatile ssize_t g_stackHeight = 0;

#define DEBUG 0
#if DEBUG
/**
 * @brief Creates a console window and writes debug messages to it
 * @param message Text to print out
 * @returns true on success, else false on failure
 */
static bool
Debug(const char *const message)
{
        const size_t messageLen = strlen(message);
        char newMessage[USHRT_MAX];

        if (messageLen >= lengthof(newMessage))
        {
                return false;
        }

        if (snprintf(newMessage, lengthof(newMessage), "%s\n", message) < 0)
        {
                return false;
        }

        static bool consoleAlloced = false;
        if (AllocConsole() == 0 && !consoleAlloced)
        {
                return false;
        }
        consoleAlloced = true;

        HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdOut == NULL || stdOut == INVALID_HANDLE_VALUE)
        {
                return false;
        }

        DWORD written = 0;
        if (!WriteConsoleA(
                    stdOut, newMessage, strlen(newMessage), &written, NULL))
        {
                return false;
        }
        return true;
}
#endif

// ---------------------------------------------------------------------
// Forward declaration of function definitions that will be used later |
// ---------------------------------------------------------------------
static void CALLBACK
WinEventProc(HWINEVENTHOOK hWinEventHook,
             DWORD event,
             HWND hWnd,
             LONG idObj,
             LONG idChild,
             DWORD dwEventThread,
             DWORD dwEventTimeMs);

static func_status_t
EnvironmentCompletedHandlerInvoke(const IUnknown *const this,
                                  const HRESULT errorCode,
                                  const ICoreWebView2Environment *const arg);

static func_status_t
ControllerCompletedHandlerInvoke(const IUnknown *const this,
                                 const HRESULT errorCode,
                                 const ICoreWebView2Controller *const arg);

static func_status_t
WebView2ContextMenuRequestEventHandlerInvoke(
        ICoreWebView2ContextMenuRequestedEventHandler *const this,
        ICoreWebView2 *const sender,
        ICoreWebView2ContextMenuRequestedEventArgs *const args);

static func_status_t
WebView2WebMessageReceivedEventHandlerInvoke(
        ICoreWebView2WebMessageReceivedEventHandler *const handler,
        ICoreWebView2 *const webview,
        ICoreWebView2WebMessageReceivedEventArgs *const args);

static func_status_t
OnCloseContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                               IUnknown *const args);

static func_status_t
OnMoveContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                              IUnknown *const args);

static func_status_t
OnTopMostContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                                 IUnknown *const args);

static func_status_t
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
HandlerQueryInterface(IUnknown *this, const IID *, void **ppvObject)
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
        controllerCompletedHandlerVtbl = {
                .AddRef = (void *)HandlerAddRef,
                .Release = (void *)HandlerRelease,
                .QueryInterface = (void *)HandlerQueryInterface,
                .Invoke = (void *)ControllerCompletedHandlerInvoke};

static ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
        controllerCompletedHandler = {.lpVtbl =
                                              &controllerCompletedHandlerVtbl};

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl
        environmentCompletedHandlerVtbl = {
                .AddRef = (void *)HandlerAddRef,
                .Release = (void *)HandlerRelease,
                .QueryInterface = (void *)HandlerQueryInterface,
                .Invoke = (void *)EnvironmentCompletedHandlerInvoke};

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
        environmentCompletedHandler = {
                .lpVtbl = &environmentCompletedHandlerVtbl};

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

static ICoreWebView2CustomItemSelectedEventHandlerVtbl
        topMostMenuSelectedHandlerVtbl = {
                .AddRef = (void *)HandlerAddRef,
                .Release = (void *)HandlerRelease,
                .QueryInterface = (void *)HandlerQueryInterface,
                .Invoke = (void *)OnTopMostContextItemMenuSelected};

static ICoreWebView2CustomItemSelectedEventHandler topMostMenuSelectedHandler =
        {.lpVtbl = &topMostMenuSelectedHandlerVtbl};

/**
 * @brief Opens the default directory where the widgets are located
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
OpenDefaultDirectory()
{
        char dir[BUFFSIZE];
        if (ww_default_widgets_dir(dir) == true)
        {
                return FUNC_STATUS_ERR;
        }

        if (ww_open_folder(dir) == true)
        {
                return FUNC_STATUS_ERR;
        }

        return FUNC_STATUS_OK;
}

/**
 * @brief Modifies the app's autostart entry
 * @param addEntry True to add, else false to remove the entry
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
ModifyAutostartEntry(const bool addEntry)
{
        func_status_t status = FUNC_STATUS_OK;
        char binary[BUFFSIZE];
        if (ww_get_executable_path(binary, lengthof(binary)))
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        strcat(binary, " --silent");

        HKEY hKey = nullptr;
        if (RegOpenKeyEx(
                    HKEY_CURRENT_USER, PROG_START_PATH, 0, KEY_WRITE, &hKey))
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        if (!addEntry)
        {
                if (RegDeleteValue(hKey, PROG_NAME))
                {
                        status = FUNC_STATUS_ERR;
                }
                goto cleanup;
        }

        if (RegSetValueEx(hKey,
                          PROG_NAME,
                          0,
                          REG_SZ,
                          (BYTE *)binary,
                          (strlen(binary) + 1) * sizeof(wchar_t)))
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

cleanup:
        if (hKey != nullptr)
        {
                RegCloseKey(hKey);
        }

        return status;
}

/**
 * @brief Gets whether a window is top most
 * @param hWnd Handle to the window
 * @returns true if topmost, else false if not
 */
static bool
isWindowTopMost(const HWND hWnd)
{
        const LONG_PTR style = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
        return style & WS_EX_TOPMOST;
}

/**
 * @brief Creates a json context in-memory and saves to a json file
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
SaveConfigurationToFile()
{
        size_t bytesWritten = 0;
        char session[JSONBUFFSIZE] = {};

        for (size_t i = 1; i < g_widgetCount + 1; i++)
        {
                const ww_window_ctx window = g_widgets[i].context;
                const HWND hWnd = g_widgets[i].hWnd;
                const size_t pathLength = strlen(window.filename);

                if (pathLength >= BUFFSIZE)
                {
                        return FUNC_STATUS_MEM_ERR;
                }

                char filename[BUFFSIZE];
                if (snprintf(filename,
                             lengthof(filename),
                             "%s",
                             window.filename) < 0)
                {
                        return FUNC_STATUS_MEM_ERR;
                }

                RECT rect;
                if (!GetWindowRect(hWnd, &rect))
                {
                        return FUNC_STATUS_ERR;
                }

                int16_t x = rect.left, y = rect.top;
                if ((bytesWritten +=
                     snprintf(&session[bytesWritten],
                              lengthof(session),
                              "{\"path\":\"%s\",\"position\":\"%hd, "
                              "%hd\",\"alwaysOnTop\":%s},",
                              filename,
                              x,
                              y,
                              isWindowTopMost(hWnd) == true ? "true"
                                                            : "false")) >=
                    lengthof(session))
                {
                        return FUNC_STATUS_MEM_ERR;
                }
        }
        session[bytesWritten - 1] = '\0';

        char json[USHRT_MAX];
        if (snprintf(json,
                     lengthof(json),
                     "{\"version\":\"%s\",\"isWidgetAutostartEnabled\":%s,"
                     "\"isWidgetFullscreenHideEnabled\":%s,"
                     "\"isOpenAppOnStartupEnabled\":%s,"
                     "\"lastSessionWidgets\":[%s]}",
                     PROG_SEM_VER,
                     g_settings.widgetAutostart ? "true" : "false",
                     g_settings.fullscreenHide ? "true" : "false",
                     g_settings.appAutostart ? "true" : "false",
                     session) < 0)
        {
                return FUNC_STATUS_MEM_ERR;
        }

        char directory[BUFFSIZE];
        if (ww_default_widgets_dir(directory))
        {
                return FUNC_STATUS_ERR;
        }
        strncat(directory,
                CONFIG_NAME,
                strlen(directory) + strlen(CONFIG_NAME));

        if (ww_write_to_file(directory, json, WRITE_OVERWRITE))
        {
                return FUNC_STATUS_ERR;
        }

        return FUNC_STATUS_OK;
}
/**
 * @brief Changes some setting from the application settings. This function
 * serves as a sort of getter-setter for the settings. Settings must only be
 * changed in this function.
 *
 * @param setting Setting to change
 * @param value Value to set the setting to
 * @param save Save changes to config file
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
ChangeApplicationSetting(application_runtime_setting_t setting,
                         const bool value,
                         const bool save)
{
        switch (setting)
        {
        case APPLICATION_SETTING_FULLSCREEN:
        {
                const bool *ptr = &g_settings.fullscreenHide;
                memcpy((bool *)ptr, &value, sizeof(bool));
                break;
        }
        case APPLICATION_SETTING_AUTOSTART:
        {
                const bool *ptr = &g_settings.appAutostart;
                memcpy((bool *)ptr, &value, sizeof(bool));
                ModifyAutostartEntry(value);
                break;
        }
        case APPLICATION_SETTING_START_WIDGETS:
        {
                const bool *ptr = &g_settings.widgetAutostart;
                memcpy((bool *)ptr, &value, sizeof(bool));
                break;
        }
        }

        if (save && BAD(SaveConfigurationToFile()))
        {
                return FUNC_STATUS_ERR;
        }

        return FUNC_STATUS_OK;
}

/**
 * @brief Opens a Widget by its absolute path. Arguments may be
 * `nullptr`, those will be ignore.
 *
 * @param path Full absolute path to where the widget is
 * @param x Pointer to the X position
 * @param y Pointer to the Y position
 * @param topMost Pointer to the top most state
 * @param content Pointer to the widget content string
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t __attribute__((optimize("01")))
OpenWidgetByFilename(const char *const path,
                     const size_t *const x,
                     const size_t *const y,
                     const bool *const topMost,
                     char *const content,
                     const size_t contentSize)
{
        ww_window_ctx context = {
                .width = DEF_WIDTH,
                .height = DEF_HEIGHT,
                .prevWidth = DEF_PREV_WIDTH,
                .prevHeight = DEF_PREV_HEIGHT,
                .x = x == nullptr ? DEF_X : *x,
                .y = y == nullptr ? DEF_HEIGHT : *y,
                .child = DEF_CHILD,
                .top_most = topMost == nullptr ? DEF_TOPMOST : *topMost,
                .radius = DEF_RADIUS};

        const size_t pathLength = strlen(path);
        memcpy(context.filename, path, pathLength);
        context.filename[pathLength] = '\0';

        char trimStr[BUFFSIZE];
        if (!TrimStart(path, trimStr, HANDLE_PREFIX_OFFSET))
        {
                return FUNC_STATUS_ERR;
        }

        if (ww_get_file_content(trimStr, content, contentSize) == true)
        {
                return FUNC_STATUS_ERR;
        }

        char buf[BUFFSIZE] = {};
        const size_t bufLen = lengthof(buf);

        if (ww_begin_parsing(content, lengthof(buf), TAG_APP_NAME, buf, bufLen))
        {
                memcpy(context.title, buf, lengthof(buf));
                context.title[lengthof(buf) - 1] = '\0';
        }

        if (ww_begin_parsing(content, lengthof(buf), TAG_WIN_SIZE, buf, bufLen))
        {
                size_t width, height;
                const bool isSet = Get2DValue(buf, &width, &height);
                context.width = isSet ? width : DEF_WIDTH;
                context.height = isSet ? height : DEF_HEIGHT;
        }

        if (ww_begin_parsing(
                    content, lengthof(buf), TAG_WIN_LOCATION, buf, bufLen) &&
            x == nullptr && y == nullptr)
        {
                size_t xPos, yPos;
                const bool isSet = Get2DValue(buf, &xPos, &yPos);
                context.x = isSet ? xPos : DEF_X;
                context.y = isSet ? yPos : DEF_Y;
        }

        if (ww_begin_parsing(content, lengthof(buf), TAG_WIN_PREV, buf, bufLen))
        {
                size_t width, height;
                const bool isSet = Get2DValue(buf, &width, &height);
                context.prevWidth = isSet ? width : DEF_X;
                context.prevHeight = isSet ? height : DEF_Y;
        }

        if (ww_begin_parsing(
                    content, lengthof(buf), TAG_APP_TOPMOST, buf, bufLen) &&
            topMost == nullptr)
        {
                context.top_most = strcmp(buf, "true") == 0;
        }

        if (ww_begin_parsing(
                    content, lengthof(buf), TAG_WIN_BORD_RAD, buf, bufLen))
        {
                const double radius = strtod(buf, nullptr);
                context.radius = radius;
        }

        g_widgetCount++;

        if (BAD(create_widget_window(&context)))
        {
                return FUNC_STATUS_ERR;
        }

        memcpy(&g_widgets[g_widgetCount].context,
               &context,
               sizeof(ww_window_ctx));

        return FUNC_STATUS_OK;
}

/**
 * @brief Places an item in the stack
 * @param item Item to be stacked
 */
static void
Push(stack_item_t item)
{
        memcpy(&g_stack[g_stackHeight++], &item, sizeof(stack_item_t));
}

/**
 * @brief Pops the next element in the stack
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
Pop()
{
        if (--g_stackHeight < 0)
        {
                g_stackHeight = 0;
                return FUNC_STATUS_OK;
        }

        stack_item_t item = g_stack[g_stackHeight];
        char content[USHRT_MAX];
        if (BAD(OpenWidgetByFilename(item.filename,
                                     &item.x,
                                     &item.y,
                                     &item.topMost,
                                     content,
                                     lengthof(content))))
        {
                return FUNC_STATUS_ERR;
        }
        return FUNC_STATUS_OK;
}

/**
 * @brief Loads all configurations from the JSON file. Opens widgets that were
 * previously opened and apply settings from the last session.
 *
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
LoadConfigurationFromFile()
{
        char absolutePath[BUFFSIZE];
        if (ww_default_widgets_dir(absolutePath))
        {
                return FUNC_STATUS_ERR;
        }
        ReplaceChars(absolutePath, widget_char_slash, widget_char_b_slash);
        strcat(absolutePath, CONFIG_NAME);

        if (access(absolutePath, F_OK) != 0)
        {
                return FUNC_STATUS_USR_ERR;
        }

        char fileContent[JSONBUFFSIZE];
        if (ww_get_file_content(
                    absolutePath, fileContent, lengthof(fileContent)))
        {
                return FUNC_STATUS_ERR;
        }

        string_json_t jsonContent;
        if (ConvertStringToJson(fileContent, &jsonContent) != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        string_json_t lastSessionWidgets;
        if (GetProperty(jsonContent,
                        &lastSessionWidgets,
                        "lastSessionWidgets") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        string_json_t isWidgetAutostartEnabledJson;
        if (GetProperty(jsonContent,
                        &isWidgetAutostartEnabledJson,
                        "isWidgetAutostartEnabled") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        char isWidgetAutostartEnabled[JSONBUFFSIZE];
        if (ConvertJsonToString(isWidgetAutostartEnabledJson,
                                isWidgetAutostartEnabled) != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        string_json_t isOpenAppOnStartupEnabledJson;
        if (GetProperty(jsonContent,
                        &isOpenAppOnStartupEnabledJson,
                        "isOpenAppOnStartupEnabled") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        char isOpenAppOnStartupEnabled[JSONBUFFSIZE];
        if (ConvertJsonToString(isOpenAppOnStartupEnabledJson,
                                isOpenAppOnStartupEnabled) != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        string_json_t isWidgetFullscreenHideEnabledJson;
        if (GetProperty(jsonContent,
                        &isWidgetFullscreenHideEnabledJson,
                        "isWidgetFullscreenHideEnabled") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        char isWidgetFullscreenHideEnabled[JSONBUFFSIZE];
        if (ConvertJsonToString(isWidgetFullscreenHideEnabledJson,
                                isWidgetFullscreenHideEnabled) != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        if (BAD(ChangeApplicationSetting(
                    APPLICATION_SETTING_START_WIDGETS,
                    strcmp(isWidgetAutostartEnabled, "true") == 0,
                    false)))
        {
                return FUNC_STATUS_ERR;
        }

        if (BAD(ChangeApplicationSetting(
                    APPLICATION_SETTING_AUTOSTART,
                    strcmp(isOpenAppOnStartupEnabled, "true") == 0,
                    false)))
        {
                return FUNC_STATUS_ERR;
        }

        if (BAD(ChangeApplicationSetting(
                    APPLICATION_SETTING_FULLSCREEN,
                    strcmp(isWidgetFullscreenHideEnabled, "true") == 0,
                    false)))
        {
                return FUNC_STATUS_ERR;
        }

        if (!g_settings.widgetAutostart)
        {
                return FUNC_STATUS_OK;
        }

        size_t start = 0;
        for (size_t i = 0; i < lastSessionWidgets.length; i++)
        {
                if (lastSessionWidgets.str[i] == '{' && start == 0)
                {
                        start = i;
                }

                if (lastSessionWidgets.str[i] == '}')
                {
                        char obj[JSONBUFFSIZE];
                        GetSubstring(lastSessionWidgets.str, obj, start, i);

                        string_json_t jsonObj;
                        if (ConvertStringToJson(obj, &jsonObj) != FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ERR;
                        }

                        string_json_t jsonPath;
                        if (GetProperty(jsonObj, &jsonPath, "path") !=
                            FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ERR;
                        }

                        char path[BUFFSIZE];
                        if (ConvertJsonToString(jsonPath, path) != FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ERR;
                        }

                        string_json_t jsonPosition;
                        if (GetProperty(jsonObj, &jsonPosition, "position") !=
                            FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ERR;
                        }

                        char position[BUFFSIZE];
                        if (ConvertJsonToString(jsonPosition, position) !=
                            FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ERR;
                        }

                        const char *const prefix = "file:\\\\%s";
                        if (strlen(path) + strlen(prefix) >= BUFFSIZE)
                        {
                                return FUNC_STATUS_MEM_ERR;
                        }

                        char prefixPath[BUFFSIZE];
                        if (snprintf(prefixPath,
                                     lengthof(prefixPath),
                                     prefix,
                                     path) < 0)
                        {
                                return FUNC_STATUS_MEM_ERR;
                        }

                        size_t x, y;
                        if (!Get2DValue(position, &x, &y))
                        {
                                return FUNC_STATUS_ERR;
                        }

                        string_json_t jsonTopMost;
                        if (GetProperty(jsonObj, &jsonTopMost, "alwaysOnTop") !=
                            FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ERR;
                        }

                        char topMost[BUFFSIZE];
                        if (ConvertJsonToString(jsonTopMost, topMost) !=
                            FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ERR;
                        }

                        // Loading each individual child into the stack
                        stack_item_t item = {
                                .x = x,
                                .y = y,
                                .topMost = strcmp(topMost, "true") == 0};
                        memcpy(item.filename, prefixPath, strlen(prefixPath));
                        item.filename[strlen(prefixPath)] = '\0';
                        Push(item);

                        start = 0;
                }
        }

        return FUNC_STATUS_OK;
}

/**
 * @brief Destroys a window by its HWND
 * @param hWnd Handle to destroy
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
DestroyWidowByHWND(const HWND hWnd)
{
        if (DestroyWindow(hWnd) == 0)
        {
                return FUNC_STATUS_ERR;
        }
        return FUNC_STATUS_OK;
}

/**
 * @brief Destroys all open widget windows
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
DestroyWidgetWindows()
{
        const size_t total = g_widgetCount;
        for (size_t i = 1; i <= total; i++)
        {
                const HWND hWnd = g_widgets[i].hWnd;
                if (BAD(DestroyWidowByHWND(hWnd)))
                {
                        return FUNC_STATUS_ERR;
                }
        }
        memset(&g_hWndTable, 0, sizeof(HWND) * MAX_WIDGETS);
        return FUNC_STATUS_OK;
}

/*
 * @brief Triggers when the close menu item is selected
 * @param sender Object that triggered the event
 * @param args Arguments of the event
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
OnCloseContextItemMenuSelected(ICoreWebView2ContextMenuItem *const,
                               IUnknown *const)
{
        const HWND hWnd = g_hWndTable[g_hWndSelectedHash];
        if (BAD(DestroyWidowByHWND(hWnd)))
        {
                return FUNC_STATUS_ERR;
        }

        if (BAD(SaveConfigurationToFile()))
        {
                return FUNC_STATUS_ERR;
        }
        return FUNC_STATUS_OK;
}

/**
 * @brief Sets the position of a window by its HWND
 * @param hWnd Handle to the window
 * @param x Position on the x axis
 * @param y Position on the y axis
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
SetWindowPosition(const HWND hWnd, const ssize_t x, const ssize_t y)
{
        if (!SetWindowPos(hWnd,
                          0,
                          x,
                          y,
                          0,
                          0,
                          SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE))
        {
                return FUNC_STATUS_ERR;
        }
        return FUNC_STATUS_OK;
}

/*
 * @brief Triggers when the move menu item is selected. Its purpose is to track
 * the position of the user cursor and move the window accordingly.
 *
 * @param sender Object that triggered the event
 * @param args Arguments of the event
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
OnMoveContextItemMenuSelected(ICoreWebView2ContextMenuItem *const,
                              IUnknown *const)
{
        const HWND handle = g_hWndTable[g_hWndSelectedHash];

        RECT rect;
        if (!GetWindowRect(handle, &rect))
        {
                return FUNC_STATUS_ERR;
        }

        POINT position;
        while (!(KEY_STATE_HELD & GetAsyncKeyState(VK_LBUTTON)))
        {
                if (!GetCursorPos(&position))
                {
                        return FUNC_STATUS_ERR;
                }

                if (BAD(SetWindowPosition(
                            handle,
                            position.x - (rect.right - rect.left) / 2,
                            position.y - (rect.bottom - rect.top) / 2)))
                {
                        return FUNC_STATUS_ERR;
                }
        }

        if (BAD(SaveConfigurationToFile()))
        {
                return FUNC_STATUS_ERR;
        }

        return FUNC_STATUS_OK;
}

/**
 * @brief Sets the window to be below all others
 * @param hWnd HWND of the window
 * @param state State of the window to set
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
SetWindowOrderState(const HWND hWnd, const state_order_t state)
{
        const HWND flags[] = {HWND_BOTTOM, HWND_TOP};
        if (!SetWindowPos(hWnd,
                          flags[state],
                          0,
                          0,
                          0,
                          0,
                          SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE))
        {
                return FUNC_STATUS_ERR;
        }
        return FUNC_STATUS_OK;
}

/**
 * @brief Set the top most status of a window
 * @param hWnd Handle to the window to be modified
 * @param src Flag to set the top most state
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
SetWindowTopMost(const HWND hWnd, const bool src)
{
        const HWND flags[] = {HWND_NOTOPMOST, HWND_TOPMOST};
        if (!SetWindowPos(
                    hWnd, flags[src], 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE))
        {
                return FUNC_STATUS_ERR;
        }

        if (src)
        {
                SetActiveWindow(hWnd);
                SetWindowOrderState(hWnd, STATE_ORDER_TOP);
        }
        return FUNC_STATUS_OK;
}

/*
 * @brief Triggers when the close menu item is selected
 * @param sender Object that triggered the event
 * @param args Arguments of the event
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
OnTopMostContextItemMenuSelected(ICoreWebView2ContextMenuItem *const,
                                 IUnknown *const)
{
        const HWND hWnd = g_hWndTable[g_hWndSelectedHash];
        const bool state = isWindowTopMost(hWnd);
        if (BAD(SetWindowTopMost(hWnd, !state)))
        {
                return FUNC_STATUS_ERR;
        }

        if (BAD(SaveConfigurationToFile()))
        {
                return FUNC_STATUS_ERR;
        }
        return FUNC_STATUS_OK;
}

/**
 * @brief Adds a new item to the Context Menu
 * @param environment Default WebView2 Environment
 * @param items Pointer to the items object
 * @param itemsCount Pointer to the count of items
 * @param token Pointer to the registration token
 * @param label Name of the context menu item
 * @param eventHandler Pointer to the event handler function
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
AddContextMenuItem(
        ICoreWebView2Environment10 *const environment,
        ICoreWebView2ContextMenuItemCollection *const items,
        UINT32 *const itemsCount,
        EventRegistrationToken *const token,
        ICoreWebView2CustomItemSelectedEventHandler *const eventHandler,
        const wchar_t *const label,
        const webview_context_menu_kind_t kind,
        const bool *const checkboxState,
        ICoreWebView2ContextMenuItem *menuItem)
{

        switch (kind)
        {
        case WEBVIEW_CONTEXT_MENU_KIND_COMMAND:
        {
                if (environment->lpVtbl->CreateContextMenuItem(
                            environment,
                            label,
                            nullptr,
                            COREWEBVIEW2_CONTEXT_MENU_ITEM_KIND_COMMAND,
                            &menuItem) != S_OK)
                {
                        return FUNC_STATUS_WBV_ERR;
                }
                break;
        }
        case WEBVIEW_CONTEXT_MENU_KIND_CHECKBOX:
        {
                if (environment->lpVtbl->CreateContextMenuItem(
                            environment,
                            label,
                            nullptr,
                            COREWEBVIEW2_CONTEXT_MENU_ITEM_KIND_CHECK_BOX,
                            &menuItem) != S_OK)
                {
                        return FUNC_STATUS_WBV_ERR;
                }

                if (menuItem->lpVtbl->put_IsChecked(
                            menuItem, *checkboxState == true ? TRUE : FALSE) !=
                    S_OK)
                {
                        return FUNC_STATUS_WBV_ERR;
                }
                break;
        }
        }

        if (menuItem->lpVtbl->remove_CustomItemSelected(menuItem, *token) !=
            S_OK)
        {
                return FUNC_STATUS_WBV_ERR;
        }

        if (menuItem->lpVtbl->add_CustomItemSelected(
                    menuItem, eventHandler, token) != S_OK)
        {
                return FUNC_STATUS_WBV_ERR;
        }

        if (items->lpVtbl->InsertValueAtIndex(items, *itemsCount, menuItem) !=
            S_OK)
        {
                return FUNC_STATUS_WBV_ERR;
        }

        return FUNC_STATUS_OK;
}

/**
 * @brief Removes all items from the default context menu
 * @param args Arguments of the context menu event
 * @returns 0 if successful, else some other number
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
RemoveContextMenuItems(ICoreWebView2ContextMenuRequestedEventArgs *const args)
{
        ICoreWebView2ContextMenuItemCollection *items = nullptr;
        if (args->lpVtbl->get_MenuItems(args, &items) != S_OK)
        {
                goto cleanup;
        }

        UINT32 itemsCount;
        if (items->lpVtbl->get_Count(items, &itemsCount) != S_OK)
        {
                goto cleanup;
        }

        ICoreWebView2ContextMenuTarget *target = nullptr;
        if (args->lpVtbl->get_ContextMenuTarget(args, &target) != S_OK)
        {
                goto cleanup;
        }

        ICoreWebView2ContextMenuItem *current = nullptr;
        UINT32 i = itemsCount;
        while (i-- > 0)
        {
                if (items->lpVtbl->GetValueAtIndex(items, i, &current) != S_OK)
                {
                        goto cleanup;
                }

                if (items->lpVtbl->RemoveValueAtIndex(items, i) != S_OK)
                {
                        goto cleanup;
                }

                current->lpVtbl->Release(current);
                current = nullptr;
        }

cleanup:
        if (current != nullptr)
        {
                current->lpVtbl->Release(current);
        }

        if (target != nullptr)
        {
                target->lpVtbl->Release(target);
        }

        if (items != nullptr)
        {
                items->lpVtbl->Release(items);
        }

        return FUNC_STATUS_OK;
}

/**
 * @brief Gets called whenver the Context Menu opens
 * @param this Reference to the event handler
 * @param sender Webview2 core object
 * @param args Arguments of the caller
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
WebView2ContextMenuRequestEventHandlerInvoke(
        ICoreWebView2ContextMenuRequestedEventHandler *const,
        ICoreWebView2 *const sender,
        ICoreWebView2ContextMenuRequestedEventArgs *const args)
{
        HRESULT status = FUNC_STATUS_OK;

        if (BAD(RemoveContextMenuItems(args)))
        {
                return FUNC_STATUS_ERR;
        }

        ICoreWebView2ContextMenuItemCollection *items = nullptr;
        if (args->lpVtbl->get_MenuItems(args, &items) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        UINT32 itemsCount;
        if (items->lpVtbl->get_Count(items, &itemsCount) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        WCHAR pathWcharPtr[BUFFSIZE];
        LPWSTR pathPtr = pathWcharPtr;
        if (sender->lpVtbl->get_Source(sender, &pathPtr) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        char pathCharPtr[BUFFSIZE];
        if (wcstombs(pathCharPtr, pathPtr, lengthof(pathCharPtr)) == (size_t)-1)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        ReplaceChars(pathCharPtr, widget_char_slash, widget_char_b_slash);
        char out[BUFFSIZE];
        if (!TrimStart(pathCharPtr, out, FILE_PREFIX_OFFSET))
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        const size_t hash = GetHashFromString(out, HASH_PRIME, MAX_WIDGETS);
        const HWND hWndSelected = g_hWndTable[hash];
        g_hWndSelectedHash = hash;

        ICoreWebView2Environment10 *environment =
                (ICoreWebView2Environment10 *)g_env;

        static EventRegistrationToken moveEventHandlerToken = {};
        ICoreWebView2ContextMenuItem *moveBtnMenuItem = nullptr;
        if (BAD(AddContextMenuItem(environment,
                                   items,
                                   &itemsCount,
                                   &moveEventHandlerToken,
                                   &moveMenuSelectedHandler,
                                   LBL_CTX_MENU_MOVE,
                                   WEBVIEW_CONTEXT_MENU_KIND_COMMAND,
                                   nullptr,
                                   moveBtnMenuItem)))
        {
                return FUNC_STATUS_ERR;
        }

        static EventRegistrationToken topMostEventHandlerToken = {};
        ICoreWebView2ContextMenuItem *topMostBtnMenuItem = nullptr;
        const bool isTopMost = isWindowTopMost(hWndSelected);
        if (BAD(AddContextMenuItem(environment,
                                   items,
                                   &itemsCount,
                                   &topMostEventHandlerToken,
                                   &topMostMenuSelectedHandler,
                                   LBL_CTX_MENU_TOP_MOST,
                                   WEBVIEW_CONTEXT_MENU_KIND_CHECKBOX,
                                   &isTopMost,
                                   topMostBtnMenuItem)))
        {
                return FUNC_STATUS_ERR;
        }

        static EventRegistrationToken closeEventHandlerToken = {};
        ICoreWebView2ContextMenuItem *closeBtnMenuItem = nullptr;
        if (BAD(AddContextMenuItem(environment,
                                   items,
                                   &itemsCount,
                                   &closeEventHandlerToken,
                                   &closeMenuSelectedHandler,
                                   LBL_CTX_MENU_CLOSE,
                                   WEBVIEW_CONTEXT_MENU_KIND_COMMAND,
                                   nullptr,
                                   closeBtnMenuItem)))
        {
                return FUNC_STATUS_ERR;
        }

cleanup:
        if (closeBtnMenuItem != nullptr)
        {
                closeBtnMenuItem->lpVtbl->Release(closeBtnMenuItem);
        }

        if (topMostBtnMenuItem != nullptr)
        {
                topMostBtnMenuItem->lpVtbl->Release(topMostBtnMenuItem);
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
 * @brief Appends the widget to the global list of browsers.
 * @param webview Webview component
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
AppendWidgetsToDOM(ICoreWebView2 *const webview)
{
        char app_dir[BUFFSIZE];
        if (ww_default_widgets_dir(app_dir))
        {
                return FUNC_STATUS_ERR;
        }

        size_t count = 0;
        char widgets[MAX_WIDGETS][BUFFSIZE];
        if ((count = ww_get_files_from_dir(app_dir, widgets, MAX_WIDGETS)) < 1)
        {
                webview->lpVtbl->ExecuteScript(
                        webview, L"addWidgets([])", nullptr);
                return FUNC_STATUS_USR_ERR;
        }

        ssize_t bytes = 0;
        const uint8_t offset = 3;
        const ssize_t maxLength = (MAX_WIDGETS * BUFFSIZE) +
                                  (MAX_WIDGETS * (BUFFSIZE + offset)) -
                                  (MAX_WIDGETS * BUFFSIZE);

        char files[maxLength];
        for (size_t i = 0; i < count; i++)
        {
                const size_t filenameLen = strlen(widgets[i]);
                if (filenameLen >= BUFFSIZE - offset)
                {
                        continue;
                }

                if (ww_get_file_bytes(widgets[i]) >= USHRT_MAX)
                {
                        continue;
                }

                char content[USHRT_MAX];
                if (ww_get_file_content(widgets[i], content, lengthof(content)))
                {
                        continue;
                }

                char appTitle[BUFFSIZE];
                if (!ww_begin_parsing(content,
                                      lengthof(content),
                                      TAG_APP_NAME,
                                      appTitle,
                                      lengthof(appTitle)))
                {
                        continue;
                }

                bytes += snprintf(&files[bytes],
                                  lengthof(appTitle),
                                  "{\"title\":\"%s\",\"path\":\"%s\"},",
                                  appTitle,
                                  widgets[i]);
        }
        files[bytes - 1] = '\0';

        if (bytes >= maxLength)
        {
                return FUNC_STATUS_MEM_ERR;
        }

        const char format[] = "addWidgets([%s])";
        char command[maxLength + lengthof(format)];
        if ((bytes = snprintf(command, maxLength, format, files)) < 0)
        {
                return FUNC_STATUS_MEM_ERR;
        }

        wchar_t wideBuffCommand[maxLength];
        if (mbstowcs(wideBuffCommand, command, bytes) == (size_t)-1)
        {
                return FUNC_STATUS_MEM_ERR;
        }
        wideBuffCommand[bytes] = '\0';

        webview->lpVtbl->ExecuteScript(webview, wideBuffCommand, nullptr);

        return FUNC_STATUS_OK;
}

/**
 * @brief Loads the settings in-memory into the DOM
 * @param webview Webview core object
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
AppendSettingsToDOM(ICoreWebView2 *const webview)
{
        char settings[BUFFSIZE];
        snprintf(settings,
                 lengthof(settings),
                 "toggleSettings({\"isWidgetAutostartEnabled\":%s,"
                 "\"isWidgetFullscreenHideEnabled\":%s,"
                 "\"isOpenAppOnStartupEnabled\":%s})",
                 g_settings.widgetAutostart ? "true" : "false",
                 g_settings.fullscreenHide ? "true" : "false",
                 g_settings.appAutostart ? "true" : "false");

        wchar_t command[BUFFSIZE];
        if (mbstowcs(command, settings, lengthof(command)) == (size_t)-1)
        {
                return FUNC_STATUS_MEM_ERR;
        }

        if (webview->lpVtbl->ExecuteScript(webview, command, nullptr) != S_OK)
        {
                return FUNC_STATUS_ERR;
        }

        return FUNC_STATUS_OK;
}

/**
 * @brief Flips the internal flag of a setting by name
 * @param src Name of the flag to flip
 */
static void
ToggleSettingByName(const char *const src)
{
        if (strcmp(src, "isOpenAppOnStartupEnabled") == 0)
        {
                const bool status = !g_settings.appAutostart;
                ChangeApplicationSetting(
                        APPLICATION_SETTING_AUTOSTART, status, true);
        }

        if (strcmp(src, "isWidgetFullscreenHideEnabled") == 0)
        {
                const bool status = !g_settings.fullscreenHide;
                ChangeApplicationSetting(
                        APPLICATION_SETTING_FULLSCREEN, status, true);
        }

        if (strcmp(src, "isWidgetAutostartEnabled") == 0)
        {
                const bool status = !g_settings.widgetAutostart;
                ChangeApplicationSetting(
                        APPLICATION_SETTING_START_WIDGETS, status, true);
        }
}

/**
 * @brief Handles incoming events from JavaScript
 * @param handler Received event handler
 * @param webview Context of the webview
 * @param args Arguments of the event handler
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
WebView2WebMessageReceivedEventHandlerInvoke(
        ICoreWebView2WebMessageReceivedEventHandler *const,
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
                return FUNC_STATUS_MEM_ERR;
        }

        char cMessage[BUFFSIZE];
        ret = WideCharToMultiByte(
                CP_UTF8, 0, wMessage, -1, cMessage, ret, nullptr, nullptr);
        if (ret == 0)
        {
                return FUNC_STATUS_MEM_ERR;
        }

        // The result is returned as a JSON string, parse the string here
        string_json_t dataString;
        if (ConvertStringToJson(cMessage, &dataString) != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        string_json_t eventIdStr;
        if (GetProperty(dataString, &eventIdStr, "eventId") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        string_json_t argsJson;
        if (GetProperty(dataString, &argsJson, "args") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        // Each event has an associated id to trigger default functions. Some
        // may also have arguments with relevant information of what
        // instructions to perform
        int eventId;
        if (ConvertJsonToStandardType(eventIdStr, JSON_INT, &eventId) !=
            FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        char argument[BUFFSIZE];
        if (ConvertJsonToString(argsJson, argument) != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ERR;
        }

        switch (eventId)
        {
        case EVT_OPEN_DEFAULT_DIR:
                if (BAD(OpenDefaultDirectory()))
                {
                        return FUNC_STATUS_ERR;
                }
                break;
        case EVT_GET_WGT_FILENAMES:
                if (BAD(AppendWidgetsToDOM(webview)))
                {
                        return FUNC_STATUS_ERR;
                }

                if (BAD(AppendSettingsToDOM(webview)))
                {
                        return FUNC_STATUS_ERR;
                }
                break;
        case EVT_OPEN_WGT_FILENAME:
        {
                char content[USHRT_MAX];
                if (BAD(OpenWidgetByFilename(argument,
                                             nullptr,
                                             nullptr,
                                             nullptr,
                                             content,
                                             lengthof(content))))
                {
                        return FUNC_STATUS_ERR;
                }
                break;
        }
        case EVT_TOGGLE_SETTING:
        {
                ToggleSettingByName(argument);
                break;
        }
        case EVT_THEME_CHANGED:
        {
                BOOL themeDark = strcmp(argument, "dark") == 0 ? TRUE : FALSE;
                if (FAILED(DwmSetWindowAttribute(g_parentHwnd,
                                                 DWMWA_USE_IMMERSIVE_DARK_MODE,
                                                 &themeDark,
                                                 sizeof(themeDark))))
                {
                        return FUNC_STATUS_ERR;
                }

                break;
        }
        }

        return FUNC_STATUS_OK;
}

/*
 * @brief Called when the environment finishes being created
 * @param this Pointer to itself
 * @param errorCode status of the operation
 * @param arg Argument of the event
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
EnvironmentCompletedHandlerInvoke(const IUnknown *const,
                                  const HRESULT,
                                  const ICoreWebView2Environment *const arg)
{
        g_env = (ICoreWebView2Environment *)arg;

        const HWND hWnd = g_widgets[g_widgetCount].hWnd;
        if (g_env->lpVtbl->CreateCoreWebView2Controller(
                    g_env, hWnd, &controllerCompletedHandler) != S_OK)
        {
                return FUNC_STATUS_ERR;
        }

        return FUNC_STATUS_OK;
}

/**
 * @brief Is called when the webview context is ready
 * @param IUnknown Interface invoked
 * @param erroCode Status code
 * @param arg WebView2 Controller
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
ControllerCompletedHandlerInvoke(const IUnknown *const,
                                 const HRESULT,
                                 const ICoreWebView2Controller *const arg)
{
        HRESULT status = FUNC_STATUS_OK;

        g_widgets[g_widgetCount].controller = (ICoreWebView2Controller *)arg;
        ICoreWebView2Controller2 *controller =
                (ICoreWebView2Controller2 *)g_widgets[g_widgetCount].controller;
        if (controller != nullptr)
        {
                if (controller->lpVtbl->get_CoreWebView2(
                            controller, &g_widgets[g_widgetCount].window) !=
                    S_OK)
                {
                        status = FUNC_STATUS_ERR;
                        goto cleanup;
                }

                controller->lpVtbl->AddRef(controller);
        }

        // Browser settings
        ICoreWebView2_11 *window =
                (ICoreWebView2_11 *)g_widgets[g_widgetCount].window;

        COREWEBVIEW2_COLOR transparency = {0, 0, 0, 0};
        controller->lpVtbl->put_DefaultBackgroundColor(controller,
                                                       transparency);

        ICoreWebView2Settings *settings = nullptr;
        if (window->lpVtbl->get_Settings(window, &settings) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_IsScriptEnabled(settings, true) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_AreDefaultScriptDialogsEnabled(settings,
                                                                 true) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_IsWebMessageEnabled(settings, true) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_AreDevToolsEnabled(settings, true) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_AreDefaultContextMenusEnabled(settings,
                                                                true) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_IsStatusBarEnabled(settings, true) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        const HWND hWnd = g_widgets[g_widgetCount].hWnd;
        RECT bounds;
        if (!GetClientRect(hWnd, &bounds))
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        if (controller->lpVtbl->put_Bounds(controller, bounds) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        /**
         *  Registering event handlers
         *  1- Event handler to receive JavaScript messages
         *  2- Event handler to detect when the context menu opens
         */
        EventRegistrationToken token;
        const webview_widget_t widget = g_widgets[g_widgetCount];
        const ww_window_ctx windowCtx = widget.context;

        if (window->lpVtbl->add_WebMessageReceived(
                    window, &messageReceivedEventHandler, &token) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        // Only children should have the context menu
        if (windowCtx.child == true)
        {
                if (window->lpVtbl->add_ContextMenuRequested(
                            window, &contextMenuEventHandler, &token) != S_OK)
                {
                        status = FUNC_STATUS_ERR;
                        goto cleanup;
                }
        }

        // Converting the URI to a widestr so that we can pass to Navigate(...)
        wchar_t path[BUFFSIZE];
        const size_t pathLen = strlen(windowCtx.filename);
        if (mbstowcs(path, windowCtx.filename, pathLen) == (size_t)-1)
        {
                status = FUNC_STATUS_MEM_ERR;
                goto cleanup;
        }
        path[pathLen] = '\0';

        if (window->lpVtbl->Navigate(window, path) != S_OK)
        {
                status = FUNC_STATUS_ERR;
                goto cleanup;
        }

        // Pops widgets if there are any pending
        if (g_stackHeight > 0)
        {
                Pop();
        }

cleanup:
        if (settings != nullptr)
        {
                settings->lpVtbl->Release(settings);
        }

        return status;
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
                if (target == g_widgets[i].hWnd)
                {
                        return i;
                }
        }
        return -1;
}

/**
 * @brief Adds a context menu item to the system tray
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
AddSystrayContextMenuItem(const HMENU hMenu,
                          const uint8_t uID,
                          const char *const label)
{
        if (!InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, uID, label))
        {
                return FUNC_STATUS_ERR;
        }
        return FUNC_STATUS_OK;
}

/**
 * @brief Callback function function for various window messages/events
 * @param hwnd Default handle of th application
 * @param uMsg Code of the event triggered
 * @param WPARAM wParam
 * @param lParam lParam
 */
LRESULT CALLBACK
WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
        switch (uMsg)
        {
        case WM_CLOSE:
        {
                ShowWindow(hWnd, SW_MINIMIZE);
                return S_OK;
        }
        case WM_DESTROY:
        {
                const ssize_t indexHwnd = FindHwnd(hWnd);
                if (indexHwnd < 0)
                {
                        return S_FALSE;
                }

                g_hWndTable[g_hWndSelectedHash] = nullptr;
                memcpy(&g_widgets[indexHwnd],
                       &g_widgets[g_widgetCount],
                       sizeof(webview_widget_t));

                if (g_widgetCount-- > 0 && indexHwnd != 0)
                {
                        return S_FALSE;
                }

                PostQuitMessage(S_FALSE);
                return S_OK;
        }
        case WM_SIZE:
        {
                switch (wParam)
                {
                case SIZE_MINIMIZED:
                        RECT desktop;
                        const HWND hWndDesktop = GetDesktopWindow();
                        if (!GetWindowRect(hWndDesktop, &desktop))
                        {
                                return S_FALSE;
                        }

                        const ssize_t width = desktop.right * -1;
                        const ssize_t height = desktop.bottom * -1;
                        if (BAD(SetWindowPosition(hWnd, width, height)))
                        {
                                return S_FALSE;
                        }
                        break;
                }

                size_t i = 0;
                do
                {
                        const HWND hWndW = g_widgets[i].hWnd;
                        RECT bounds;
                        if (!GetClientRect(hWndW, &bounds))
                        {
                                return S_FALSE;
                        }

                        ICoreWebView2Controller *controller =
                                g_widgets[i].controller;
                        if (controller == nullptr)
                        {
                                return S_FALSE;
                        }

                        if (controller->lpVtbl->put_Bounds(controller,
                                                           bounds) != S_OK)
                        {
                                return S_FALSE;
                        }
                } while (++i < g_widgetCount);

                return S_OK;
        }
        case PROG_WM_ICONNOTIFY:
        {
                switch (lParam)
                {
                case WM_LBUTTONUP:
                {
                        ShowWindow(g_parentHwnd, SW_RESTORE);
                        SetForegroundWindow(g_parentHwnd);
                        break;
                }
                case WM_RBUTTONUP:
                {
                        POINT point;
                        if (!GetCursorPos(&point))
                        {
                                return S_FALSE;
                        }

                        const HMENU hMenu = CreatePopupMenu();
                        if (hMenu == nullptr)
                        {
                                return S_FALSE;
                        }

                        if (BAD(AddSystrayContextMenuItem(
                                    hMenu, UID_SYSTRAY_EXIT, LBL_SYSTRAY_EXIT)))
                        {
                                return S_FALSE;
                        }

                        if (BAD(AddSystrayContextMenuItem(hMenu,
                                                          UID_SYSTRAY_CLOSE,
                                                          LBL_SYSTRAY_CLOSE)))
                        {
                                return S_FALSE;
                        }

                        if (!SetForegroundWindow(hWnd))
                        {
                                return S_FALSE;
                        }

                        if (!TrackPopupMenu(hMenu,
                                            TPM_LEFTALIGN | TPM_LEFTBUTTON |
                                                    TPM_BOTTOMALIGN,
                                            point.x,
                                            point.y,
                                            0,
                                            hWnd,
                                            nullptr))
                        {
                                return S_FALSE;
                        }

                        if (!PostMessage(hWnd, WM_NULL, 0, 0))
                        {
                                return S_FALSE;
                        }
                        break;
                }
                }
                return S_OK;
        }
        case WM_COMMAND:
        {
                switch (wParam)
                {
                case UID_SYSTRAY_EXIT:
                {
                        if (BAD(DestroyWidowByHWND(hWnd)))
                        {
                                return S_FALSE;
                        }
                        break;
                }
                case UID_SYSTRAY_CLOSE:
                {
                        if (BAD(DestroyWidgetWindows()))
                        {
                                return S_FALSE;
                        }
                        break;
                }
                }
                break;
        }

                return S_OK;
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/**
 * Gets the dimensions of a window and its position by its hWnd
 * @param hWnd HWND of the window
 * @param width Width of the window
 * @param height Height of the window
 * @param x Position on the X axis
 * @param y Position on the Y axis
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
GetHwndDimensions(const HWND hWnd,
                  size_t *const width,
                  size_t *const height,
                  size_t *const x,
                  size_t *const y)
{
        RECT rect;
        if (!GetWindowRect(hWnd, &rect))
        {
                return FUNC_STATUS_ERR;
        }

        if (width != nullptr)
        {
                *width = rect.right - rect.left;
        }

        if (height != nullptr)
        {
                *height = rect.bottom - rect.top;
        }

        if (x != nullptr)
        {
                *x = rect.left;
        }

        if (y != nullptr)
        {
                *y = rect.top;
        }

        return FUNC_STATUS_OK;
}

/**
 * @brief Used to get whether a Direct3D device can render.
 * Essentially the same as detecting whether a game using the GPU is in the
 * foreground.
 *
 * @param canRender Whether the GPU can render
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
CanGPURender(bool *const canRender)
{
        func_status_t status = FUNC_STATUS_OK;
        HRESULT hResult;
        IDirectDraw7 *dDraw = nullptr;
        if (FAILED((hResult = DirectDrawCreateEx(nullptr,
                                                 (void **)&dDraw,
                                                 &IID_IDirectDraw7,
                                                 nullptr))))
        {
                status = FUNC_STATUS_MEM_ERR;
                *canRender = false;
                goto cleanup;
        }

        if ((hResult = dDraw->lpVtbl->TestCooperativeLevel(dDraw)) ==
            DDERR_EXCLUSIVEMODEALREADYSET)
        {
                *canRender = true;
                goto cleanup;
        }

cleanup:
        if (dDraw != nullptr)
        {
                dDraw->lpVtbl->Release(dDraw);
        }

        return status;
}

/**
 * @brief Gets whether the current window is active
 * @param hWnd The window to compare
 * @returns true if active, false if inactive
 */
static bool
IsWindowActive(const HWND hWnd)
{
        const HWND activehWnd = GetForegroundWindow();
        return hWnd == activehWnd;
}

/**
 * @brief Triggers events for all processes in the entire system
 */
static void CALLBACK
WinEventProc(HWINEVENTHOOK,
             DWORD,
             HWND hWnd,
             LONG idObj,
             LONG idChild,
             DWORD,
             DWORD)
{
        const HWND hWndShell = GetShellWindow();
        const HWND hDefView =
                FindWindowEx(hWndShell, nullptr, "SHELLDLL_DefView", nullptr);
        const HWND hFolderView =
                FindWindowEx(hDefView, nullptr, "SysListView32", nullptr);

        if (!hWnd || idObj != OBJID_WINDOW || idChild != CHILDID_SELF ||
            !(GetWindowLong(hWnd, GWL_STYLE) & WS_VISIBLE) ||
            GetWindowLong(hWnd, GWL_EXSTYLE) &
                    (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE))
        {
                return;
        }

        bool canRender = false;
        if (g_settings.fullscreenHide && BAD(CanGPURender(&canRender)))
        {
                return;
        }

        static ssize_t maxStackHeight = 0;
        if (hWnd != hFolderView)
        {
                if (g_widgetCount == 0)
                {
                        return;
                }

                g_stackHeight = 0;
                maxStackHeight = g_widgetCount;
                for (size_t i = 1; i <= g_widgetCount; i++)
                {
                        const ssize_t index = FindHwnd(g_widgets[i].hWnd);
                        if (index < 0)
                        {
                                return;
                        }

                        if (IsWindowActive(g_widgets[i].hWnd))
                        {
                                SetWindowOrderState(g_widgets[i].hWnd,
                                                    STATE_ORDER_TOP);
                                continue;
                        }

                        if (!isWindowTopMost(g_widgets[i].hWnd))
                        {
                                if (BAD(SetWindowOrderState(
                                            g_widgets[i].hWnd,
                                            STATE_ORDER_BOTTOM)))
                                {
                                        continue;
                                }
                        }

                        if (!canRender)
                        {
                                continue;
                        }

                        const ww_window_ctx context = g_widgets[index].context;
                        const HWND hWndW = g_widgets[index].hWnd;

                        size_t x, y;
                        if (BAD(GetHwndDimensions(
                                    hWndW, nullptr, nullptr, &x, &y)))
                        {
                                return;
                        }

                        stack_item_t item = {
                                .topMost = context.top_most, .x = x, .y = y};
                        const size_t strLen = strlen(context.filename);
                        const char format[] = "file:\\\\%s";
                        snprintf(item.filename,
                                 lengthof(format) + strLen,
                                 format,
                                 context.filename);
                        Push(item);
                }

                if (!canRender)
                {
                        return;
                }

                DestroyWidgetWindows();
                return;
        }

        if (g_stackHeight > -1 && g_stackHeight == maxStackHeight)
        {
                Pop();
        }
}

/**
 * @brief Used to reload changes in the default widgets directy when something
 * inside the directory changes.
 */
static void *
OnDirectoryChangedReload(void *)
{
        HANDLE hDir = nullptr;
        char dirPath[BUFFSIZE];
        if (ww_default_widgets_dir(dirPath))
        {
                goto cleanup;
        }

        wchar_t lpDirPath[BUFFSIZE];
        if (mbstowcs(lpDirPath, dirPath, lengthof(dirPath)) == (size_t)-1)
        {
                goto cleanup;
        }

        if ((hDir = CreateFileW(
                     lpDirPath,
                     FILE_LIST_DIRECTORY,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     nullptr,
                     OPEN_EXISTING,
                     FILE_FLAG_BACKUP_SEMANTICS,
                     nullptr)) == nullptr)
        {
                goto cleanup;
        }

        BYTE buffer[1024];
        BYTE *pBuffer = buffer;
        DWORD bytes;
        while (true)
        {
                if (!ReadDirectoryChangesW(
                            hDir,
                            &buffer,
                            lengthof(buffer),
                            true,
                            FILE_NOTIFY_CHANGE_FILE_NAME |
                                    FILE_NOTIFY_CHANGE_DIR_NAME |
                                    FILE_NOTIFY_CHANGE_ATTRIBUTES |
                                    FILE_NOTIFY_CHANGE_SIZE |
                                    FILE_NOTIFY_CHANGE_LAST_WRITE,
                            &bytes,
                            nullptr,
                            nullptr))
                {
                        continue;
                }

                FILE_NOTIFY_INFORMATION *info = nullptr;
                bool canUpdateChanges = true;
                do
                {
                        info = (FILE_NOTIFY_INFORMATION *)pBuffer;
                        const size_t len = info->FileNameLength / sizeof(WCHAR);
                        char filename[BUFFSIZE];
                        wcstombs(filename, info->FileName, len);
                        filename[len] = '\0';

                        if (strcmp(filename, &CONFIG_NAME[1]) == 0)
                        {
                                canUpdateChanges = false;
                        }

                        pBuffer += info->NextEntryOffset;
                } while (info->NextEntryOffset);

                g_dirChangesDetected = canUpdateChanges;
        }

cleanup:
        if (hDir != nullptr)
        {
                CloseHandle(hDir);
        }
        return nullptr;
}

/**
 * @brief Default event loop of th application
 */
static void
event_loop()
{
        MSG msg;
        BOOL bRet;
        while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
        {
                if (g_dirChangesDetected)
                {
                        ICoreWebView2 *parentWebview =
                                g_widgets[PARENT_INDEX].window;

                        AppendWidgetsToDOM(parentWebview);

                        g_dirChangesDetected = false;
                }

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
}

/**
 * @brief Set the border radius for the corners of the window
 * @param hWnd Handle to the window
 * @param context Window context of the widget
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
SetWindowBorderRadius(const HWND hWnd, const ww_window_ctx *const context)
{
        const size_t width = context->width;
        const size_t height = context->height;
        const size_t radius = context->radius;
        const bool child = context->child;
        if (radius <= 0 || !child)
        {
                return FUNC_STATUS_OK;
        }

        const HRGN hRgn =
                CreateRoundRectRgn(0, 0, width, height, radius, radius);
        if (hRgn == nullptr)
        {
                return FUNC_STATUS_ERR;
        }

        if (SetWindowRgn(hWnd, hRgn, true) == 0)
        {
                return FUNC_STATUS_ERR;
        }

        DeleteObject(hRgn);

        return FUNC_STATUS_OK;
}

/**
 * @brief Hides the window from the taskbar
 * @param hWnd Handle to the window
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
HideWindowFromTaskbar(const HWND hWnd)
{
        LONG style = GetWindowLong(hWnd, GWL_EXSTYLE);
        if (SetWindowLong(hWnd, GWL_EXSTYLE, style | WS_EX_TOOLWINDOW) == 0)
        {
                return FUNC_STATUS_ERR;
        }

        ShowWindow(hWnd, SW_SHOW);

        return FUNC_STATUS_OK;
}

/**
 * @brief Set window icon
 * @param hWnd Handle to the window
 * @param src Resource ico to set the icon to
 * @param hIcon Handle to the icon
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
SetWindowIcon(const HWND hWnd, const char *const src, HANDLE *hIcon)
{
        if ((*hIcon = LoadImage(0,
                                src,
                                IMAGE_ICON,
                                0,
                                0,
                                LR_DEFAULTSIZE | LR_LOADFROMFILE | LR_SHARED |
                                        LR_DEFAULTCOLOR)) == nullptr)
        {
                return FUNC_STATUS_ERR;
        }

        const LPARAM icon = (LPARAM)*hIcon;
        const HWND hWndParent = GetWindow(hWnd, GW_OWNER);
        SendMessage(hWnd, WM_SETICON, ICON_SMALL, icon);
        SendMessage(hWnd, WM_SETICON, ICON_BIG, icon);
        SendMessage(hWndParent, WM_SETICON, ICON_SMALL, icon);
        SendMessage(hWndParent, WM_SETICON, ICON_BIG, icon);
        return FUNC_STATUS_OK;
}

/**
 * @brief Creates a new window
 * @param context Context of the window with all options
 * @returns FUNC_STATUS_OK on success, else a numeric error code
 */
static func_status_t
create_widget_window(ww_window_ctx *const context)
{
        TrimStart(context->filename, context->filename, HANDLE_PREFIX_OFFSET);
        ReplaceChars(context->filename, widget_char_slash, widget_char_b_slash);

        const size_t hash =
                GetHashFromString(context->filename, HASH_PRIME, MAX_WIDGETS);

        // Refuse to open duplicate widgets
        if (g_hWndTable[hash] != nullptr)
        {
                g_widgetCount--;
                return FUNC_STATUS_ERR;
        }

        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = g_hInstance;
        wc.lpszClassName = PROG_NAME;
        RegisterClass(&wc);

        HWND *const hWnd = &g_widgets[g_widgetCount].hWnd;
        if ((*hWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP,
                                    PROG_NAME,
                                    context->title,
                                    WS_POPUP,
                                    context->x,
                                    context->y,
                                    context->width,
                                    context->height,
                                    nullptr,
                                    nullptr,
                                    g_hInstance,
                                    nullptr)) == nullptr)
        {
                return FUNC_STATUS_ERR;
        }

        g_hWndTable[hash] = *hWnd;
        ShowWindow(*hWnd, g_nCmdShow);

        if (g_parentHwnd == nullptr)
        {
                g_parentHwnd = *hWnd;
        }

        HANDLE hIcon;
        if (BAD(SetWindowIcon(*hWnd, FAVICON_PATH, &hIcon)))
        {
                return FUNC_STATUS_ERR;
        }

        if (BAD(SetWindowBorderRadius(*hWnd, context)))
        {
                return FUNC_STATUS_ERR;
        }

        SetLayeredWindowAttributes(*hWnd, 0, 255, LWA_ALPHA);

        const bool topMost = context->top_most;
        if (topMost)
        {
                if (BAD(SetWindowTopMost(*hWnd, true)))
                {
                        return FUNC_STATUS_ERR;
                }
        }

        if (BAD(HideWindowFromTaskbar(*hWnd)))
        {
                return FUNC_STATUS_ERR;
        }

        if (!UpdateWindow(*hWnd))
        {
                return FUNC_STATUS_ERR;
        }

        if (g_env->lpVtbl->CreateCoreWebView2Controller(
                    g_env, *hWnd, &controllerCompletedHandler) != S_OK)
        {
                return FUNC_STATUS_ERR;
        }

        return FUNC_STATUS_OK;
}

/**
 * @brief Used to create the main window of the application. This function
 * should only be called once at the start of the program.
 *
 * @param context Information about the window
 */
static func_status_t
create_widget_manager(ww_window_ctx *const context)
{
        memcpy(&g_widgets[g_widgetCount].context,
               context,
               sizeof(ww_window_ctx));
        ReplaceChars(context->filename, widget_char_slash, widget_char_b_slash);

        const size_t hash =
                GetHashFromString(context->filename, HASH_PRIME, MAX_WIDGETS);

        // Trick for the main window so that it doesn't appear in the taskbar.
        // Basically we create an invisible window and set it as the parent of
        // the real window.
        WNDCLASS invisibleWc = {};
        invisibleWc.lpfnWndProc = WindowProc;
        invisibleWc.hInstance = g_hInstance;
        invisibleWc.lpszClassName = PROG_NAME;
        RegisterClass(&invisibleWc);

        const HWND hWndInvisible = CreateWindowEx(0,
                                                  PROG_NAME,
                                                  nullptr,
                                                  WS_OVERLAPPED,
                                                  CW_USEDEFAULT,
                                                  CW_USEDEFAULT,
                                                  CW_USEDEFAULT,
                                                  CW_USEDEFAULT,
                                                  nullptr,
                                                  nullptr,
                                                  g_hInstance,
                                                  nullptr);

        // Setting the ICON of the invisible window
        ShowWindow(hWndInvisible, SW_HIDE);
        UpdateWindow(hWndInvisible);

        HANDLE hInvisIcon;
        if (BAD(SetWindowIcon(hWndInvisible, FAVICON_PATH, &hInvisIcon)))
        {
                return FUNC_STATUS_ERR;
        }

        // Creating the systemtray icon for this invisible window
        NOTIFYICONDATA nId;
        nId.cbSize = sizeof(nId);
        nId.hWnd = hWndInvisible;
        nId.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nId.uID = 0;
        nId.uCallbackMessage = PROG_WM_ICONNOTIFY;
        nId.hIcon = hInvisIcon;
        nId.uVersion = NOTIFYICON_VERSION_4;
        memcpy(nId.szTip, PROG_NAME, PROG_NAME_SIZE);
        Shell_NotifyIcon(NIM_ADD, &nId);

        // Creating the real window
        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = g_hInstance;
        wc.lpszClassName = PROG_NAME;
        RegisterClass(&wc);

        HWND *const hWnd = &g_widgets[g_widgetCount].hWnd;
        if ((*hWnd = CreateWindowEx(WS_EX_LAYERED,
                                    PROG_NAME,
                                    context->title,
                                    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                    context->x,
                                    context->y,
                                    context->width,
                                    context->height,
                                    hWndInvisible,
                                    nullptr,
                                    g_hInstance,
                                    nullptr)) == nullptr)
        {
                return FUNC_STATUS_ERR;
        }

        g_hWndTable[hash] = *hWnd;
        if (g_silentMode && g_parentHwnd == nullptr)
        {
                ShowWindow(*hWnd, SW_MINIMIZE);
        }
        else
        {
                ShowWindow(*hWnd, g_nCmdShow);
        }

        if (g_parentHwnd == nullptr)
        {
                g_parentHwnd = *hWnd;
        }

        HANDLE hIcon;
        if (BAD(SetWindowIcon(*hWnd, FAVICON_PATH, &hIcon)))
        {
                return FUNC_STATUS_ERR;
        }

        if (BAD(SetWindowBorderRadius(*hWnd, context)))
        {
                return FUNC_STATUS_ERR;
        }

        if (SetLayeredWindowAttributes(*hWnd, 0, MAX_ALPHA, LWA_ALPHA) == 0)
        {
                return FUNC_STATUS_ERR;
        }

        if (!UpdateWindow(*hWnd))
        {
                return FUNC_STATUS_ERR;
        }

        if (CreateCoreWebView2EnvironmentWithOptions(
                    nullptr, nullptr, nullptr, &environmentCompletedHandler) !=
            S_OK)
        {
                return FUNC_STATUS_ERR;
        }

        return FUNC_STATUS_OK;
}

/*
 * @brief Initializing the main window of the application. It will be
 * responsible for spawning children widgets.
 *
 * @param hInstance instance of the application
 * @param context Context of the main window
 * @param widgets List of all widgets
 * @returns true if successful, else false on failure
 */
bool
ww_init_main(const HINSTANCE hInstance,
             const int nCmdShow,
             const LPSTR pCmdLine,
             ww_window_ctx *const context)
{
        HWINEVENTHOOK hook = nullptr;
        pthread_t thread;
        bool comInitialized = true;
        int pThread = 0;

        g_hInstance = hInstance;
        g_nCmdShow = nCmdShow;
        g_silentMode = strcmp(pCmdLine, "--silent") == 0;

        HANDLE mutex = nullptr;
        if ((mutex = CreateMutex(nullptr, TRUE, PROG_NAME)) == nullptr)
        {
                goto cleanup;
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
                const HWND hWndActive = FindWindow(0, PROG_NAME);
                if (hWndActive)
                {
                        ShowWindow(hWndActive, SW_RESTORE);
                        SetForegroundWindow(hWndActive);
                }
                goto cleanup;
        }

        if (FAILED(CoInitialize(nullptr)))
        {
                comInitialized = false;
                goto cleanup;
        }

        const HWND hWndDesktop = GetDesktopWindow();
        static size_t g_screenWidth;
        static size_t g_screenHeight;
        if (BAD(GetHwndDimensions(hWndDesktop,
                                  &g_screenWidth,
                                  &g_screenHeight,
                                  nullptr,
                                  nullptr)))
        {
                goto cleanup;
        }

        if (BAD(LoadConfigurationFromFile()))
        {
                goto cleanup;
        }

        if (BAD(create_widget_manager(context)))
        {
                goto cleanup;
        }

        if ((hook = SetWinEventHook(
                     EVENT_SYSTEM_CAPTURESTART,
                     EVENT_SYSTEM_CAPTUREEND,
                     nullptr,
                     WinEventProc,
                     0,
                     0,
                     WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS)) ==
            nullptr)
        {
                goto cleanup;
        }

        if ((pThread = pthread_create(
                     &thread, nullptr, OnDirectoryChangedReload, nullptr)) != 0)
        {
                goto cleanup;
        }

        event_loop();

cleanup:
        if (pThread == 0)
        {
                pthread_detach(thread);
        }

        if (hook != nullptr)
        {
                UnhookWinEvent(hook);
        }

        if (comInitialized)
        {
                CoUninitialize();
        }

        if (mutex != nullptr)
        {
                CloseHandle(mutex);
        }

        return false;
}
