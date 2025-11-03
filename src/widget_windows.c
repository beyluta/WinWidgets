#include "filesystem.h"
#include "global.h"
#include "widget.h"
#include "utils.h"
#include <WebView2.h>
#include <consoleapi.h>
#include <io.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringapiset.h>
#include <shellapi.h>
#include <unistd.h>
#include <wchar.h>
#include <windows.h>
#include <json.h>
#include <winerror.h>
#include <winnls.h>
#include <winnt.h>
#include <winscard.h>
#include <mmc.h>
#include <pthread.h>

static constexpr char PROG_NAME[] = "WinWidgets";
static constexpr char INTERN_PROG_SEM_VER[] = "2.0.0";
static constexpr char CONFIG_NAME[] = "\\config.json";
static constexpr char FAVICON_PATH[] = "assets/icons/favicon.ico";

static constexpr char TAG_APP_NAME[] = "applicationTitle";
static constexpr char TAG_APP_TOPMOST[] = "topMost";
static constexpr char TAG_WIN_SIZE[] = "windowSize";
static constexpr char TAG_WIN_LOCATION[] = "windowLocation";
static constexpr char TAG_WIN_PREV[] = "previewSize";
static constexpr char TAG_WIN_BORD_RAD[] = "windowBorderRadius";
static constexpr char TAG_WIN_OPACITY[] = "windowOpacity";

static constexpr wchar_t LBL_CTX_MENU_MOVE[] = L"Toggle move";
static constexpr wchar_t LBL_CTX_MENU_CLOSE[] = L"Close";
static constexpr wchar_t LBL_CTX_MENU_TOP_MOST[] = L"Toggle always on top";

static constexpr uint8_t MAX_ALPHA = UINT8_MAX;
static constexpr uint8_t HASH_PRIME = 31;
static constexpr uint8_t HANDLE_PREFIX_OFFSET = 7;
static constexpr uint8_t FILE_PREFIX_OFFSET = 8;
static constexpr uint8_t PROG_NAME_SIZE =
        sizeof(PROG_NAME) / sizeof(PROG_NAME[0]);

static constexpr uint16_t BUFFSIZ = USHRT_MAX;
static constexpr uint16_t DEF_WIDTH = 500;
static constexpr uint16_t DEF_HEIGHT = 500;
static constexpr uint16_t DEF_PREV_WIDTH = DEF_WIDTH;
static constexpr uint16_t DEF_PREV_HEIGHT = DEF_HEIGHT;
static constexpr uint16_t DEF_X = 0;
static constexpr uint16_t DEF_Y = 0;
static constexpr uint16_t DEF_OPACITY = 1;
static constexpr uint16_t DEF_RADIUS = 0;
static constexpr bool DEF_CHILD = true;
static constexpr bool DEF_TOPMOST = false;

static constexpr uint16_t KEY_STATE_HELD = SHRT_MAX + 1;
static constexpr uint16_t PROG_WM_ICONNOTIFY = WM_APP + 1;

static constexpr uint8_t UID_SYSTRAY_EXIT = 0;
static constexpr uint8_t UID_SYSTRAY_CLOSE = 1;
static constexpr char LBL_SYSTRAY_EXIT[] = "Exit application";
static constexpr char LBL_SYSTRAY_CLOSE[] = "Close widgets";

typedef enum : uint8_t
{
        EVT_OPEN_DEFAULT_DIR,
        EVT_GET_WGT_FILENAMES,
        EVT_OPEN_WGT_FILENAME,
        EVT_TOGGLE_SETTING,
} widget_events_t;

typedef enum : uint8_t
{
        FUNC_STATUS_OK,
        FUNC_STATUS_USR_ERR,
        FUNC_STATUS_WBV_ERR,
        FUNC_STATUS_MEM_ERR,
        FUNC_STATUS_ENV_ERR
} func_status_t;

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
        bool isWidgetAutostartEnabled;
        bool isWidgetFullscreenHideEnabled;
        bool isOpenAppOnStartupEnabled;
} application_settings_t;

/**
 * @brief Evaluates the expression, quits and logs the error on failure.
 */
#define EVALEXPR(expression)                                                   \
        do                                                                     \
        {                                                                      \
                auto ret = (expression);                                       \
                if (ret > FUNC_STATUS_USR_ERR)                                 \
                {                                                              \
                        LogError(ret);                                         \
                        exit(ret);                                             \
                }                                                              \
        } while (0)

/**
 * @brief Evaluates the result of a function, jumps to `cleanup` label on error
 */
#define EVALEXPRCLEANUP(expression)                                            \
        do                                                                     \
        {                                                                      \
                auto ret = (expression);                                       \
                if (ret > FUNC_STATUS_USR_ERR)                                 \
                {                                                              \
                        LogError(ret);                                         \
                        status = ret;                                          \
                        goto cleanup;                                          \
                }                                                              \
        } while (0)

// ----------------------------------------------------------
// Global variables to control the state of the main window |
// ----------------------------------------------------------
static EventRegistrationToken g_closeEventHandlerToken = {};
static EventRegistrationToken g_moveEventHandlerToken = {};
static EventRegistrationToken g_topMostEventHandlerToken = {};
static ICoreWebView2Environment *g_env = nullptr;

static application_settings_t g_settings;
static webview_widget_t g_widgets[MAX_WIDGETS] = {};
static HWND g_hWndTable[MAX_WIDGETS] = {};
static HWND g_invisibleHwnd = nullptr;

static size_t g_hWndSelectedHash = {};
static size_t g_nCmdShow = {};
static size_t g_widgetCount = 0;

static HINSTANCE g_hInstance = nullptr;
static ULONG g_handlerRefCount = 0;
static bool g_envCreated = false;
static HANDLE g_eventLog = nullptr;

static stack_item_t g_stack[MAX_WIDGETS];
static volatile ssize_t g_stackHeight = 0;

// TODO: Enable this only for the debug build later
/**
 * @brief Creates a console window and writes debug messages to it
 * @param message Text to print out
 * @returns true on success, else false on failure
 */
static bool
Debug(const char *const message)
{
        const size_t messageLen = strlen(message);
        if (messageLen >= BUFFSIZE)
        {
                return false;
        }

        const size_t newMessageLen = BUFFSIZE + 1;
        char newMessage[newMessageLen];
        if (snprintf(newMessage, newMessageLen, "%s\n", message) < 0)
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

// ---------------------------------------------------------------------
// Forward declaration of function definitions that will be used later |
// ---------------------------------------------------------------------
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
 * @brief Writes an error to Window's event viewer
 * @param errCode Code of the error
 */
static void
LogError(const uint8_t errCode)
{
        char buffer[BUFFSIZE];
        snprintf(buffer,
                 BUFFSIZE,
                 "Error code %d found on line %d\n",
                 errCode,
                 __LINE__);
        ReportEvent(g_eventLog,
                    EVENTLOG_WARNING_TYPE,
                    0,
                    0,
                    nullptr,
                    1,
                    0,
                    (LPCSTR *)&buffer,
                    nullptr);
}

/**
 * @brief Opens the default directory where the widgets are located
 * @returns The status of the operation upon return
 */
static func_status_t
OpenDefaultDirectory()
{
        char dir[BUFFSIZE];
        if (ww_default_widgets_dir(dir) == true)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        if (ww_open_folder(dir) == true)
        {
                return FUNC_STATUS_ENV_ERR;
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
 * @returns The status of the operation upon return
 */
static func_status_t __attribute__((optimize("01")))
OpenWidgetByFilename(const char *const path,
                     const size_t *const x,
                     const size_t *const y,
                     const bool *const topMost,
                     char *const content)
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
                .opacity = DEF_OPACITY,
                .radius = DEF_RADIUS};

        const size_t pathLength = strlen(path);
        memcpy(context.filename, path, pathLength);
        context.filename[pathLength] = '\0';

        char trimStr[BUFFSIZE];
        if (!TrimStart(path, trimStr, HANDLE_PREFIX_OFFSET))
        {
                return FUNC_STATUS_ENV_ERR;
        }

        if (ww_get_file_content(trimStr, content, BUFFSIZE) == true)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        char buf[BUFFSIZE] = {};
        if (GetMetaTagValue(content, TAG_APP_NAME, buf, BUFFSIZE))
        {
                memcpy(context.title, buf, BUFFSIZE);
                context.title[BUFFSIZE - 1] = '\0';
        }

        if (GetMetaTagValue(content, TAG_WIN_SIZE, buf, BUFFSIZE))
        {
                size_t width, height;
                const bool isSet = Get2DValue(buf, &width, &height);
                context.width = isSet ? width : DEF_WIDTH;
                context.height = isSet ? height : DEF_HEIGHT;
        }

        if (GetMetaTagValue(content, TAG_WIN_LOCATION, buf, BUFFSIZE) &&
            x == nullptr && y == nullptr)
        {
                size_t x, y;
                const bool isSet = Get2DValue(buf, &x, &y);
                context.x = isSet ? x : DEF_X;
                context.y = isSet ? y : DEF_Y;
        }

        if (GetMetaTagValue(content, TAG_WIN_PREV, buf, BUFFSIZE))
        {
                size_t width, height;
                const bool isSet = Get2DValue(buf, &width, &height);
                context.prevWidth = isSet ? width : DEF_X;
                context.prevHeight = isSet ? height : DEF_Y;
        }

        if (GetMetaTagValue(content, TAG_APP_TOPMOST, buf, BUFFSIZE) &&
            topMost == nullptr)
        {
                context.top_most = strcmp(buf, "true") == 0;
        }

        if (GetMetaTagValue(content, TAG_WIN_OPACITY, buf, BUFFSIZE))
        {
                const double opacity = strtod(buf, nullptr);
                context.opacity = opacity;
        }

        if (GetMetaTagValue(content, TAG_WIN_BORD_RAD, buf, BUFFSIZE))
        {
                const double radius = strtod(buf, nullptr);
                context.radius = radius;
        }

        g_widgetCount++;

        EVALEXPR(create_widget_window(&context));

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
 * @brief Pops the last elements in the queue
 */
static void
Pop()
{
        stack_item_t item = g_stack[--g_stackHeight];
        char content[USHRT_MAX];
        EVALEXPR(OpenWidgetByFilename(
                item.filename, &item.x, &item.y, &item.topMost, content));
}

/**
 * @brief Loads all configurations from the JSON file. Opens widgets that were
 * previously opened and apply settings from the last session.
 *
 * @returns The status of the operation upon return
 */
static func_status_t
LoadConfigurationFromFile()
{
        char absolutePath[BUFFSIZE];
        if (ww_default_widgets_dir(absolutePath))
        {
                return FUNC_STATUS_ENV_ERR;
        }
        ReplaceChars(absolutePath, widget_char_slash, widget_char_b_slash);
        strcat(absolutePath, CONFIG_NAME);

        if (access(absolutePath, F_OK) != 0)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        char fileContent[JSONBUFFSIZE];
        if (ww_get_file_content(absolutePath, fileContent, JSONBUFFSIZE))
        {
                return FUNC_STATUS_ENV_ERR;
        }

        string_json_t jsonContent;
        if (ConvertStringToJson(fileContent, &jsonContent) != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        string_json_t lastSessionWidgets;
        if (GetProperty(jsonContent,
                        &lastSessionWidgets,
                        "lastSessionWidgets") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        string_json_t isWidgetAutostartEnabledJson;
        if (GetProperty(jsonContent,
                        &isWidgetAutostartEnabledJson,
                        "isWidgetAutostartEnabled") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        char isWidgetAutostartEnabled[JSONBUFFSIZE];
        if (ConvertJsonToString(isWidgetAutostartEnabledJson,
                                isWidgetAutostartEnabled) != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        string_json_t isOpenAppOnStartupEnabledJson;
        if (GetProperty(jsonContent,
                        &isOpenAppOnStartupEnabledJson,
                        "isOpenAppOnStartupEnabled") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        char isOpenAppOnStartupEnabled[JSONBUFFSIZE];
        if (ConvertJsonToString(isOpenAppOnStartupEnabledJson,
                                isOpenAppOnStartupEnabled) != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        string_json_t isWidgetFullscreenHideEnabledJson;
        if (GetProperty(jsonContent,
                        &isWidgetFullscreenHideEnabledJson,
                        "isWidgetFullscreenHideEnabled") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        char isWidgetFullscreenHideEnabled[JSONBUFFSIZE];
        if (ConvertJsonToString(isWidgetFullscreenHideEnabledJson,
                                isWidgetFullscreenHideEnabled) != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        g_settings.isWidgetAutostartEnabled =
                strcmp(isWidgetAutostartEnabled, "true") == 0;
        g_settings.isOpenAppOnStartupEnabled =
                strcmp(isOpenAppOnStartupEnabled, "true") == 0;
        g_settings.isWidgetFullscreenHideEnabled =
                strcmp(isWidgetFullscreenHideEnabled, "true") == 0;

        if (!g_settings.isWidgetAutostartEnabled)
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
                                return FUNC_STATUS_ENV_ERR;
                        }

                        string_json_t jsonPath;
                        if (GetProperty(jsonObj, &jsonPath, "path") !=
                            FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ENV_ERR;
                        }

                        char path[BUFFSIZE];
                        if (ConvertJsonToString(jsonPath, path) != FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ENV_ERR;
                        }

                        string_json_t jsonPosition;
                        if (GetProperty(jsonObj, &jsonPosition, "position") !=
                            FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ENV_ERR;
                        }

                        char position[BUFFSIZE];
                        if (ConvertJsonToString(jsonPosition, position) !=
                            FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ENV_ERR;
                        }

                        const char *const prefix = "file:\\\\%s";
                        if (strlen(path) + strlen(prefix) >= BUFFSIZE)
                        {
                                return FUNC_STATUS_MEM_ERR;
                        }

                        char prefixPath[BUFFSIZE];
                        if (snprintf(prefixPath, BUFFSIZE, prefix, path) < 0)
                        {
                                return FUNC_STATUS_MEM_ERR;
                        }

                        size_t x, y;
                        if (!Get2DValue(position, &x, &y))
                        {
                                return FUNC_STATUS_ENV_ERR;
                        }

                        string_json_t jsonTopMost;
                        if (GetProperty(jsonObj, &jsonTopMost, "alwaysOnTop") !=
                            FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ENV_ERR;
                        }

                        char topMost[BUFFSIZE];
                        if (ConvertJsonToString(jsonTopMost, topMost) !=
                            FUNC_SUCCESS)
                        {
                                return FUNC_STATUS_ENV_ERR;
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
 * @returns The status of the operation upon return
 */
static func_status_t
SaveConfigurationToFile()
{
        size_t bytesWritten = 0;
        char session[JSONBUFFSIZE];
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
                if (snprintf(filename, BUFFSIZE, "%s", window.filename) < 0)
                {
                        return FUNC_STATUS_MEM_ERR;
                }

                RECT rect;
                if (!GetWindowRect(hWnd, &rect))
                {
                        return FUNC_STATUS_ENV_ERR;
                }

                int16_t x = rect.left, y = rect.top;
                if ((bytesWritten +=
                     snprintf(&session[bytesWritten],
                              JSONBUFFSIZE,
                              "{\"path\":\"%s\",\"position\":\"%hd, "
                              "%hd\",\"alwaysOnTop\":%s},",
                              filename,
                              x,
                              y,
                              isWindowTopMost(hWnd) == true ? "true"
                                                            : "false")) >=
                    JSONBUFFSIZE)
                {
                        return FUNC_STATUS_MEM_ERR;
                }
        }
        session[strlen(session) - 1] = '\0';

        const char *const format =
                "{\"version\":\"%s\",\"isWidgetAutostartEnabled\":%s,"
                "\"isWidgetFullscreenHideEnabled\":%s,"
                "\"isOpenAppOnStartupEnabled\":%s,"
                "\"lastSessionWidgets\":[%s]}";
        char json[strlen(format) + strlen(INTERN_PROG_SEM_VER) + bytesWritten];
        if (snprintf(
                    json,
                    JSONBUFFSIZE,
                    format,
                    INTERN_PROG_SEM_VER,
                    g_settings.isWidgetAutostartEnabled ? "true" : "false",
                    g_settings.isWidgetFullscreenHideEnabled ? "true" : "false",
                    g_settings.isOpenAppOnStartupEnabled ? "true" : "false",
                    session) < 0)
        {
                return FUNC_STATUS_MEM_ERR;
        }

        char directory[BUFFSIZE];
        if (ww_default_widgets_dir(directory))
        {
                return FUNC_STATUS_ENV_ERR;
        }
        strncat(directory,
                CONFIG_NAME,
                strlen(directory) + strlen(CONFIG_NAME));

        if (ww_write_to_file(directory, json, WRITE_OVERWRITE))
        {
                return FUNC_STATUS_ENV_ERR;
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
                return FUNC_STATUS_ENV_ERR;
        }
        memset(&g_hWndTable, 0, sizeof(HWND) * MAX_WIDGETS);
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
                EVALEXPR(DestroyWidowByHWND(hWnd));
        }
        return FUNC_STATUS_OK;
}

/*
 * @brief Triggers when the close menu item is selected
 * @param sender Object that triggered the event
 * @param args Arguments of the event
 */
static func_status_t
OnCloseContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                               IUnknown *const args)
{
        const HWND hWnd = g_hWndTable[g_hWndSelectedHash];
        EVALEXPR(DestroyWidowByHWND(hWnd));
        EVALEXPR(SaveConfigurationToFile());
        return FUNC_STATUS_OK;
}

/**
 * @brief Sets the position of a window by its HWND
 * @param hWnd Handle to the window
 * @param x Position on the x axis
 * @param y Position on the y axis
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
                return FUNC_STATUS_ENV_ERR;
        }
        return FUNC_STATUS_OK;
}

/*
 * @brief Triggers when the move menu item is selected. Its purpose is to track
 * the position of the user cursor and move the window accordingly.
 *
 * @param sender Object that triggered the event
 * @param args Arguments of the event
 * @returns 0 if successful, else some other number
 */
static func_status_t
OnMoveContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                              IUnknown *const args)
{
        const HWND handle = g_hWndTable[g_hWndSelectedHash];

        RECT rect;
        if (!GetWindowRect(handle, &rect))
        {
                return FUNC_STATUS_ENV_ERR;
        }

        POINT position;
        while (!(KEY_STATE_HELD & GetAsyncKeyState(VK_LBUTTON)))
        {
                if (!GetCursorPos(&position))
                {
                        return FUNC_STATUS_ENV_ERR;
                }

                EVALEXPR(SetWindowPosition(
                        handle,
                        position.x - (rect.right - rect.left) / 2,
                        position.y - (rect.bottom - rect.top) / 2));
        }

        EVALEXPR(SaveConfigurationToFile());

        return FUNC_STATUS_OK;
}

/**
 * @brief Set the top most status of a window
 * @param hWnd Handle to the window to be modified
 * @param src Flag to set the top most state
 * @returns The status of the operation upon return
 */
static func_status_t
SetWindowTopMost(const HWND hWnd, const bool src)
{
        const HWND flags[] = {HWND_TOPMOST, HWND_NOTOPMOST};
        if (!SetWindowPos(
                    hWnd, flags[src], 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE))
        {
                return FUNC_STATUS_ENV_ERR;
        }
        return FUNC_STATUS_OK;
}

/*
 * @brief Triggers when the close menu item is selected
 * @param sender Object that triggered the event
 * @param args Arguments of the event
 */
static func_status_t
OnTopMostContextItemMenuSelected(ICoreWebView2ContextMenuItem *const sender,
                                 IUnknown *const args)
{
        const HWND hWnd = g_hWndTable[g_hWndSelectedHash];
        const bool topMost = isWindowTopMost(hWnd);
        EVALEXPR(SetWindowTopMost(hWnd, topMost));
        EVALEXPR(SaveConfigurationToFile());
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
 * @returns A pointer to the new context menu item, or nullptr on failure
 */
static func_status_t
AddContextMenuItem(
        ICoreWebView2Environment10 *const environment,
        ICoreWebView2ContextMenuItemCollection *const items,
        UINT32 *const itemsCount,
        EventRegistrationToken *const token,
        ICoreWebView2CustomItemSelectedEventHandler *const eventHandler,
        const wchar_t *const label,
        ICoreWebView2ContextMenuItem *menuItem)
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
 */
static func_status_t
RemoveContextMenuItems(ICoreWebView2ContextMenuRequestedEventArgs *const args)
{
        HRESULT status = FUNC_STATUS_OK;

        ICoreWebView2ContextMenuItemCollection *items = nullptr;
        if (args->lpVtbl->get_MenuItems(args, &items) != S_OK)
        {
                status = FUNC_STATUS_WBV_ERR;
                goto cleanup;
        }

        UINT32 itemsCount;
        if (items->lpVtbl->get_Count(items, &itemsCount) != S_OK)
        {
                status = FUNC_STATUS_WBV_ERR;
                goto cleanup;
        }

        ICoreWebView2ContextMenuTarget *target = nullptr;
        if (args->lpVtbl->get_ContextMenuTarget(args, &target) != S_OK)
        {
                status = FUNC_STATUS_WBV_ERR;
                goto cleanup;
        }

        ICoreWebView2ContextMenuItem *current = nullptr;
        for (UINT32 i = itemsCount - 1; i >= 0; i--)
        {
                if (items->lpVtbl->GetValueAtIndex(items, i, &current) != S_OK)
                {
                        status = FUNC_STATUS_WBV_ERR;
                        goto cleanup;
                }

                if (items->lpVtbl->RemoveValueAtIndex(items, i) != S_OK)
                {
                        status = FUNC_STATUS_WBV_ERR;
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
 * @returns 0 if successful, else any other value
 */
static func_status_t
WebView2ContextMenuRequestEventHandlerInvoke(
        ICoreWebView2ContextMenuRequestedEventHandler *const this,
        ICoreWebView2 *const sender,
        ICoreWebView2ContextMenuRequestedEventArgs *const args)
{
        HRESULT status = FUNC_STATUS_OK;

        EVALEXPRCLEANUP(RemoveContextMenuItems(args));

        ICoreWebView2ContextMenuItemCollection *items = nullptr;
        if (args->lpVtbl->get_MenuItems(args, &items) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        UINT32 itemsCount;
        if (items->lpVtbl->get_Count(items, &itemsCount) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        WCHAR pathWcharPtr[BUFFSIZE];
        LPWSTR pathPtr = pathWcharPtr;
        if (sender->lpVtbl->get_Source(sender, &pathPtr) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        char pathCharPtr[BUFFSIZE];
        if (wcstombs(pathCharPtr, pathPtr, BUFFSIZE) < 0)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        ReplaceChars(pathCharPtr, widget_char_slash, widget_char_b_slash);
        char out[BUFFSIZE];
        if (!TrimStart(pathCharPtr, out, FILE_PREFIX_OFFSET))
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        const size_t hash = GetHashFromString(out, HASH_PRIME, MAX_WIDGETS);

        g_hWndSelectedHash = hash;
        ICoreWebView2Environment10 *environment =
                (ICoreWebView2Environment10 *)g_env;

        ICoreWebView2ContextMenuItem *moveBtnMenuItem = nullptr;
        EVALEXPRCLEANUP(AddContextMenuItem(environment,
                                           items,
                                           &itemsCount,
                                           &g_moveEventHandlerToken,
                                           &moveMenuSelectedHandler,
                                           LBL_CTX_MENU_MOVE,
                                           moveBtnMenuItem));

        ICoreWebView2ContextMenuItem *topMostBtnMenuItem = nullptr;
        EVALEXPRCLEANUP(AddContextMenuItem(environment,
                                           items,
                                           &itemsCount,
                                           &g_topMostEventHandlerToken,
                                           &topMostMenuSelectedHandler,
                                           LBL_CTX_MENU_TOP_MOST,
                                           topMostBtnMenuItem));

        ICoreWebView2ContextMenuItem *closeBtnMenuItem = nullptr;
        EVALEXPRCLEANUP(AddContextMenuItem(environment,
                                           items,
                                           &itemsCount,
                                           &g_closeEventHandlerToken,
                                           &closeMenuSelectedHandler,
                                           LBL_CTX_MENU_CLOSE,
                                           closeBtnMenuItem));

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
 * @returns The status of the operation upon return
 */
static func_status_t
AppendWidgetsToDOM(ICoreWebView2 *const webview)
{
        char app_dir[BUFFSIZE];
        if (ww_default_widgets_dir(app_dir))
        {
                return FUNC_STATUS_ENV_ERR;
        }

        size_t count = 0;
        char widgets[MAX_WIDGETS][BUFFSIZE];
        if ((count = ww_get_files_from_dir(app_dir, widgets, MAX_WIDGETS)) == 0)
        {
                return FUNC_STATUS_MEM_ERR;
        }

        size_t bytes = 0;
        const uint8_t offset = 3;
        const size_t maxLength = (MAX_WIDGETS * BUFFSIZE) +
                                 (MAX_WIDGETS * (BUFFSIZE + offset)) -
                                 (MAX_WIDGETS * BUFFSIZE);

        char files[maxLength];
        for (size_t i = 0; i < count; i++)
        {
                const size_t filenameLen = strlen(widgets[i]);
                if (filenameLen >= BUFFSIZE - offset)
                {
                        return FUNC_STATUS_MEM_ERR;
                }

                bytes += snprintf(&files[bytes], BUFFSIZE, "'%s',", widgets[i]);
        }
        files[bytes - 1] = '\0';

        if (bytes >= maxLength)
        {
                return FUNC_STATUS_MEM_ERR;
        }

        const char *const funcDef = "addWidgets(\"[%s]\")";
        char command[maxLength + strlen(funcDef)];
        if ((bytes = snprintf(command, maxLength, funcDef, files)) < 0)
        {
                return FUNC_STATUS_MEM_ERR;
        }

        wchar_t wideBuffCommand[maxLength];
        mbstowcs(wideBuffCommand, command, bytes);
        webview->lpVtbl->ExecuteScript(webview, wideBuffCommand, nullptr);

        return FUNC_STATUS_OK;
}

/**
 * @brief Loads the settings in-memory into the DOM
 * @param webview Webview core object
 * @returns The status of the operation upon return
 */
static func_status_t
AppendSettingsToDOM(ICoreWebView2 *const webview)
{
        char settings[BUFFSIZE];
        snprintf(settings,
                 BUFFSIZE,
                 "toggleSettings({\"isWidgetAutostartEnabled\":%s,"
                 "\"isWidgetFullscreenHideEnabled\":%s,"
                 "\"isOpenAppOnStartupEnabled\":%s})",
                 g_settings.isWidgetAutostartEnabled ? "true" : "false",
                 g_settings.isWidgetFullscreenHideEnabled ? "true" : "false",
                 g_settings.isOpenAppOnStartupEnabled ? "true" : "false");

        wchar_t command[BUFFSIZE];
        if (mbstowcs(command, settings, strlen(settings)) < 0)
        {
                return FUNC_STATUS_MEM_ERR;
        }

        if (webview->lpVtbl->ExecuteScript(webview, command, nullptr) != S_OK)
        {
                return FUNC_STATUS_ENV_ERR;
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
                g_settings.isOpenAppOnStartupEnabled =
                        !g_settings.isOpenAppOnStartupEnabled;
        }

        if (strcmp(src, "isWidgetFullscreenHideEnabled") == 0)
        {
                g_settings.isWidgetFullscreenHideEnabled =
                        !g_settings.isWidgetFullscreenHideEnabled;
        }

        if (strcmp(src, "isWidgetAutostartEnabled") == 0)
        {
                g_settings.isWidgetAutostartEnabled =
                        !g_settings.isWidgetAutostartEnabled;
        }

        EVALEXPR(SaveConfigurationToFile());
}

/**
 * @brief Handles incoming events from JavaScript
 * @param handler Received event handler
 * @param webview Context of the webview
 * @param args Arguments of the event handler
 * @returns The status of the operation upon return
 */
static func_status_t
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
                return FUNC_STATUS_ENV_ERR;
        }

        string_json_t eventIdStr;
        if (GetProperty(dataString, &eventIdStr, "eventId") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        string_json_t argsJson;
        if (GetProperty(dataString, &argsJson, "args") != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        // Each event has an associated id to trigger default functions. Some
        // may also have arguments with relevant information of what
        // instructions to perform
        int eventId;
        if (ConvertJsonToStandardType(eventIdStr, JSON_INT, &eventId) !=
            FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        char argument[BUFFSIZE];
        if (ConvertJsonToString(argsJson, argument) != FUNC_SUCCESS)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        switch (eventId)
        {
        case EVT_OPEN_DEFAULT_DIR:
                EVALEXPR(OpenDefaultDirectory());
                break;
        case EVT_GET_WGT_FILENAMES:
                EVALEXPR(AppendWidgetsToDOM(webview));
                EVALEXPR(AppendSettingsToDOM(webview));
                break;
        case EVT_OPEN_WGT_FILENAME:
        {
                char content[USHRT_MAX];
                EVALEXPR(OpenWidgetByFilename(
                        argument, nullptr, nullptr, nullptr, content));
                break;
        }
        case EVT_TOGGLE_SETTING:
                ToggleSettingByName(argument);
                break;
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
EnvironmentCompletedHandlerInvoke(const IUnknown *const this,
                                  const HRESULT errorCode,
                                  const ICoreWebView2Environment *const arg)
{
        g_envCreated = true;
        g_env = (ICoreWebView2Environment *)arg;

        const HWND hWnd = g_widgets[g_widgetCount].hWnd;
        if (g_env->lpVtbl->CreateCoreWebView2Controller(
                    g_env, hWnd, &controllerCompletedHandler) != S_OK)
        {
                return FUNC_STATUS_ENV_ERR;
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
ControllerCompletedHandlerInvoke(const IUnknown *const this,
                                 const HRESULT errorCode,
                                 const ICoreWebView2Controller *const arg)
{
        HRESULT status = FUNC_STATUS_OK;

        g_widgets[g_widgetCount].controller = (ICoreWebView2Controller *)arg;
        ICoreWebView2Controller *controller =
                g_widgets[g_widgetCount].controller;
        if (controller != nullptr)
        {
                if (controller->lpVtbl->get_CoreWebView2(
                            controller, &g_widgets[g_widgetCount].window) !=
                    S_OK)
                {
                        status = FUNC_STATUS_ENV_ERR;
                        goto cleanup;
                }

                controller->lpVtbl->AddRef(controller);
        }

        // Browser settings
        ICoreWebView2Settings *settings = nullptr;
        ICoreWebView2_11 *window =
                (ICoreWebView2_11 *)g_widgets[g_widgetCount].window;
        if (window->lpVtbl->get_Settings(window, &settings) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_IsScriptEnabled(settings, true) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_AreDefaultScriptDialogsEnabled(settings,
                                                                 true) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_IsWebMessageEnabled(settings, true) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_AreDevToolsEnabled(settings, true) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_AreDefaultContextMenusEnabled(settings,
                                                                true) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        if (settings->lpVtbl->put_IsStatusBarEnabled(settings, true) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        const HWND hWnd = g_widgets[g_widgetCount].hWnd;
        RECT bounds;
        if (!GetClientRect(hWnd, &bounds))
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        if (controller->lpVtbl->put_Bounds(controller, bounds) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
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
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        // Only children should have the context menu
        if (windowCtx.child == true)
        {
                if (window->lpVtbl->add_ContextMenuRequested(
                            window, &contextMenuEventHandler, &token) != S_OK)
                {
                        status = FUNC_STATUS_ENV_ERR;
                        goto cleanup;
                }
        }

        // Converting the URI to a widestr so that we can pass to Navigate(...)
        wchar_t path[BUFFSIZE];
        const size_t pathLen = strlen(windowCtx.filename);
        if (mbstowcs(path, windowCtx.filename, pathLen) < 0)
        {
                status = FUNC_STATUS_MEM_ERR;
                goto cleanup;
        }
        path[pathLen] = '\0';

        if (window->lpVtbl->Navigate(window, path) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
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
                return FUNC_STATUS_ENV_ERR;
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
                        EVALEXPR(SetWindowPosition(hWnd, width, height));
                        break;
                }

                size_t i = 0;
                do
                {
                        const HWND hWnd = g_widgets[i].hWnd;
                        RECT bounds;
                        if (!GetClientRect(hWnd, &bounds))
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

                        EVALEXPR(AddSystrayContextMenuItem(
                                hMenu, UID_SYSTRAY_EXIT, LBL_SYSTRAY_EXIT));
                        EVALEXPR(AddSystrayContextMenuItem(
                                hMenu, UID_SYSTRAY_CLOSE, LBL_SYSTRAY_CLOSE));

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
                        EVALEXPR(DestroyWidowByHWND(hWnd));
                        break;
                }
                case UID_SYSTRAY_CLOSE:
                {
                        EVALEXPR(DestroyWidgetWindows());
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
 * @returns The status of the operation upon return
 */
static func_status_t
SetWindowOpacity(const HWND hWnd, const double alpha)
{
        LONG style = GetWindowLong(hWnd, GWL_EXSTYLE);
        style |= WS_EX_LAYERED;

        if (SetWindowLong(hWnd, GWL_EXSTYLE, style) == 0)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        const byte byteAlpha = alpha * MAX_ALPHA;
        if (SetLayeredWindowAttributes(hWnd, 0, byteAlpha, LWA_ALPHA) == 0)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        return FUNC_STATUS_OK;
}

/**
 * @brief Set the border radius for the corners of the window
 * @param hWnd Handle to the window
 * @param context Window context of the widget
 * @returns The status of the operation upon return
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
                return FUNC_STATUS_ENV_ERR;
        }

        if (SetWindowRgn(hWnd, hRgn, true) == 0)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        DeleteObject(hRgn);

        return FUNC_STATUS_OK;
}

/**
 * @brief Hides the window from the taskbar
 * @param hWnd Handle to the window
 * @returns The status of the operation upon return
 */
static func_status_t
HideWindowFromTaskbar(const HWND hWnd)
{
        LONG style = GetWindowLong(hWnd, GWL_EXSTYLE);
        if (SetWindowLong(hWnd, GWL_EXSTYLE, style | WS_EX_TOOLWINDOW) == 0)
        {
                return FUNC_STATUS_ENV_ERR;
        }

        ShowWindow(hWnd, SW_SHOW);

        return FUNC_STATUS_OK;
}

/**
 * @brief Set window icon
 * @param hWnd Handle to the window
 * @param src Resource ico to set the icon to
 * @param hIcon Handle to the icon
 * @returns The status of the operation upon return
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
                return FUNC_STATUS_ENV_ERR;
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
 * @returns The status of the operation upon return
 */
static func_status_t
create_widget_window(ww_window_ctx *const context)
{
        func_status_t status = FUNC_STATUS_OK;

        // Children should not have prefixes
        if (context->child)
        {
                TrimStart(context->filename,
                          context->filename,
                          HANDLE_PREFIX_OFFSET);
        }

        // Saving the context of the main window to a global list
        if (g_widgetCount == 0)
        {
                memcpy(&g_widgets[g_widgetCount].context,
                       context,
                       sizeof(ww_window_ctx));
        }

        // Replacing '/' with '\' as it is the standard for windows
        ReplaceChars(context->filename, widget_char_slash, widget_char_b_slash);

        const size_t hash =
                GetHashFromString(context->filename, HASH_PRIME, MAX_WIDGETS);

        // Refuse to open duplicate widgets
        if (g_hWndTable[hash] != nullptr)
        {
                g_widgetCount--;
                status = FUNC_STATUS_USR_ERR;
                goto cleanup;
        }

        // Trick for the main window so that it doesn't appear in the taskbar.
        // Basically we create an invisible window and set it as the parent of
        // the real window.
        if (g_invisibleHwnd == nullptr)
        {
                WNDCLASS invisibleWc = {};
                invisibleWc.lpfnWndProc = WindowProc;
                invisibleWc.hInstance = g_hInstance;
                invisibleWc.lpszClassName = PROG_NAME;
                RegisterClass(&invisibleWc);

                g_invisibleHwnd = CreateWindowEx(0,
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
                ShowWindow(g_invisibleHwnd, SW_HIDE);
                UpdateWindow(g_invisibleHwnd);

                HANDLE hIcon;
                EVALEXPRCLEANUP(
                        SetWindowIcon(g_invisibleHwnd, FAVICON_PATH, &hIcon));

                // Creating the systemtray icon for this invisible window
                NOTIFYICONDATA nId;
                nId.cbSize = sizeof(nId);
                nId.hWnd = g_invisibleHwnd;
                nId.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
                nId.uID = 0;
                nId.uCallbackMessage = PROG_WM_ICONNOTIFY;
                nId.hIcon = hIcon;
                nId.uVersion = NOTIFYICON_VERSION_4;

                memcpy(nId.szTip, PROG_NAME, PROG_NAME_SIZE);

                Shell_NotifyIcon(NIM_ADD, &nId);
        }

        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = g_hInstance;
        wc.lpszClassName = PROG_NAME;
        RegisterClass(&wc);

        HWND *const hWnd = &g_widgets[g_widgetCount].hWnd;
        const size_t wsStyle =
                context->child ? WS_POPUP : WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        const HWND parentHwnd = context->child ? nullptr : g_invisibleHwnd;
        if ((*hWnd = CreateWindowEx(WS_EX_LAYERED,
                                    PROG_NAME,
                                    context->title,
                                    wsStyle,
                                    context->x,
                                    context->y,
                                    context->width,
                                    context->height,
                                    parentHwnd,
                                    nullptr,
                                    g_hInstance,
                                    nullptr)) == nullptr)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        g_hWndTable[hash] = *hWnd;
        ShowWindow(*hWnd, g_nCmdShow);

        HANDLE hIcon;
        EVALEXPRCLEANUP(SetWindowIcon(*hWnd, FAVICON_PATH, &hIcon));
        EVALEXPRCLEANUP(SetWindowBorderRadius(*hWnd, context));
        EVALEXPRCLEANUP(SetWindowOpacity(*hWnd, context->opacity));

        // Apply these visual changes only for children widgets
        if (context->child)
        {
                const bool topMost = context->top_most == 0;
                EVALEXPRCLEANUP(SetWindowTopMost(*hWnd, topMost));
                EVALEXPRCLEANUP(HideWindowFromTaskbar(*hWnd));
        }

        if (!UpdateWindow(*hWnd))
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

        if (!g_envCreated)
        {
                if (CreateCoreWebView2EnvironmentWithOptions(
                            nullptr,
                            nullptr,
                            nullptr,
                            &environmentCompletedHandler) != S_OK)
                {
                        status = FUNC_STATUS_ENV_ERR;
                        goto cleanup;
                }
                return true;
        }

        if (g_env->lpVtbl->CreateCoreWebView2Controller(
                    g_env, *hWnd, &controllerCompletedHandler) != S_OK)
        {
                status = FUNC_STATUS_ENV_ERR;
                goto cleanup;
        }

cleanup:
        if (status > FUNC_STATUS_USR_ERR)
        {
                CoUninitialize();
        }
        return status;
}

/*
 * @brief Initializing the main window of the application. It will be
 * responsible for spawning children widgets.
 *
 * @param hInstance instance of the application
 * @param context Context of the main window
 * @param widgets List of all widgets
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

        if ((g_eventLog = RegisterEventSourceA(nullptr, "WinWidgets")) ==
            nullptr)
        {
                fprintf(stderr, "Could not register event\n");
                return true;
        }

        if (FAILED(CoInitialize(nullptr)))
        {
                fprintf(stderr, "COM Failed to initialize\n");
                return true;
        }

        EVALEXPR(LoadConfigurationFromFile());
        EVALEXPR(create_widget_window(context));

        return event_loop();
}
