#include "filesystem.h"
#include "global.h"
#include "widget.h"
#include <WebView2.h>
#include <stdio.h>
#include <windows.h>
#include <json.h>

constexpr char CLASS_NAME[] = "WidgetClass";
typedef enum : int
{
        EVT_OPEN_DEFAULT_DIR,
        EVT_GET_WGT_FILENAMES,
        EVT_OPEN_WGT_FILENAME
} widget_events_t;

// ----------------------------------------------------------
// Global variables to control the state of the main window |
// ----------------------------------------------------------
static ICoreWebView2Environment *g_env = nullptr;
static HWND g_hWnds[MAX_WIDGETS] = {};
static HINSTANCE g_hInstance = nullptr;
static int g_nCmdShow = {};
static ICoreWebView2Controller *g_controllers[MAX_WIDGETS] = {};
static ICoreWebView2 *g_windows[MAX_WIDGETS] = {};
static size_t g_widgets = 0;
static bool g_envCreated = false;
static ULONG g_handlerRefCount = 0;

// ----------------------------------------------------------------
// Global variables to control the state of current opened widget |
// ----------------------------------------------------------------
static char g_tmplName[BUFFSIZE] = {};
static char g_tmplPath[BUFFSIZE] = {};

// ---------------------------------------------------------------------
// Forward declaration of function definitions that will be used later |
// ---------------------------------------------------------------------
static HRESULT
HandlerInvoke(IUnknown *this, HRESULT errorCode, ICoreWebView2Controller *arg);

static HRESULT
WebView2WebMessageReceivedEventHandlerInvoke(
        ICoreWebView2WebMessageReceivedEventHandler *handler,
        ICoreWebView2 *webview,
        ICoreWebView2WebMessageReceivedEventArgs *args);

static bool
OpenDefaultDirectory();

static bool
AppendWidgetToBrowserList(ICoreWebView2 *webview);

static bool
create_widget_window(ww_window_ctx *context);

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
        return EXIT_REASON_TERMINATED;
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

/**
 * @brief Handles incoming events from JavaScript
 * @param handler Received event handler
 * @param webview Context of the webview
 * @param args Arguments of the event handler
 */
static HRESULT
WebView2WebMessageReceivedEventHandlerInvoke(
        ICoreWebView2WebMessageReceivedEventHandler *handler,
        ICoreWebView2 *webview,
        ICoreWebView2WebMessageReceivedEventArgs *args)
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
                return EXIT_REASON_MEM_FAILURE;
        }

        char cMessage[BUFFSIZE];
        ret = WideCharToMultiByte(
                CP_UTF8, 0, wMessage, -1, cMessage, ret, nullptr, nullptr);
        if (ret == 0)
        {
                fprintf(stderr, "Failed to convert string to UTF-8\n");
                return EXIT_REASON_MEM_FAILURE;
        }

        // The result is returned as a JSON string, parse the string here
        string_json_t dataString;
        if (ConvertStringToJson(cMessage, &dataString) != FUNC_SUCCESS)
        {
                fprintf(stderr, "Error converting \"%s\" to JSON\n", cMessage);
                return EXIT_REASON_MEM_FAILURE;
        }

        string_json_t eventIdStr;
        if (GetProperty(dataString, &eventIdStr, "eventId") != FUNC_SUCCESS)
        {
                fprintf(stderr,
                        "Event ID could not be found in \"%s\"\n",
                        cMessage);
                return EXIT_REASON_MEM_FAILURE;
        }

        string_json_t argsStr;
        if (GetProperty(dataString, &argsStr, "args") != FUNC_SUCCESS)
        {
                fprintf(stderr,
                        "Event arguments could not be found in \"%s\"\n",
                        cMessage);
                return EXIT_REASON_MEM_FAILURE;
        }

        // Each event has an associated id to trigger default functions. Some
        // may also have arguments with relevant information of what
        // instructions to perform
        int eventId;
        if (ConvertJsonToStandardType(eventIdStr, JSON_INT, &eventId) !=
            FUNC_SUCCESS)
        {
                fprintf(stderr, "Error converting JSON to Integer type\n");
                return EXIT_REASON_MEM_FAILURE;
        }

        char arguments[BUFFSIZE];
        if (ConvertJsonToStandardType(argsStr, JSON_CHAR_ARR, &arguments) !=
            FUNC_SUCCESS)
        {
                fprintf(stderr, "Error converting JSON to char array type\n");
                return EXIT_REASON_MEM_FAILURE;
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
                ww_window_ctx context = {.width = 500,
                                         .height = 500,
                                         .x = 0,
                                         .y = 0,
                                         .index = 1,
                                         .child = true,
                                         .top_most = true,
                                         .opacity = 1,
                                         .radius = 0};
                memcpy(context.filename, arguments, argsStr.length);
                HWND hwnd;
                create_widget_window(&context);
                break;
        }

        return EXIT_REASON_TERMINATED;
}

static bool
AppendWidgetToBrowserList(ICoreWebView2 *webview)
{
        // Getting the default app directory
        char app_dir[BUFFSIZE];
        if (ww_default_widgets_dir(app_dir) == true)
        {
                fprintf(stderr, "Failed to get default app directory\n");
                return false;
        }

        // Getting all filenames from the directory
        char widgets[MAX_WIDGETS][BUFFSIZE];
        const size_t count =
                ww_get_files_from_dir(app_dir, widgets, MAX_WIDGETS);
        if (count <= 0)
        {
                fprintf(stderr, "Failed to read directory %s\n", app_dir);
                return false;
        }

        // Building a comma separated list with the list all files found
        size_t j = 0;
        const size_t len = (MAX_WIDGETS * BUFFSIZE) + (MAX_WIDGETS * 2) +
                           (MAX_WIDGETS - 1) + 2;
        char list[len];
        for (size_t i = 0; i < count; i++)
        {
                const size_t size = strlen(widgets[i]);
                char temp[size + 3];
                snprintf(temp, sizeof(temp) + 1, "'%s',", widgets[i]);
                memcpy(&list[j], temp, sizeof(temp));
                j = j + sizeof(temp);
        }
        list[j - 1] = '\0';

        // Build a JSON object out of the list
        const size_t list_len = strlen(list) + 4;
        char json[list_len];
        snprintf(json, list_len + 1, "\"[%s]\"", list);
        json[list_len] = '\0';

        // Building the correct command string
        char command[EXTBUFFSIZE];
        snprintf(command, EXTBUFFSIZE, "addWidgets(%s)", json);

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
                fprintf(stderr, "Failed to open folder to directory %s\n", dir);
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
HandlerInvoke(IUnknown *this, HRESULT errorCode, ICoreWebView2Controller *arg)
{
        // Initializing the environment if it isn't already initialized
        if (!g_envCreated)
        {
                g_envCreated = true;
                g_env = (ICoreWebView2Environment *)arg;
                g_env->lpVtbl->CreateCoreWebView2Controller(
                        g_env, g_hWnds[g_widgets], &completedHandler);
                return EXIT_REASON_TERMINATED;
        }

        g_controllers[g_widgets] = (ICoreWebView2Controller *)arg;
        ICoreWebView2Controller *controller = g_controllers[g_widgets];
        if (controller != nullptr)
        {
                controller->lpVtbl->get_CoreWebView2(controller,
                                                     &g_windows[g_widgets]);
                controller->lpVtbl->AddRef(controller);
        }

        // Browser settings
        ICoreWebView2Settings *settings;
        ICoreWebView2 *window = g_windows[g_widgets];
        window->lpVtbl->get_Settings(window, &settings);
        settings->lpVtbl->put_IsScriptEnabled(settings, true);
        settings->lpVtbl->put_AreDefaultScriptDialogsEnabled(settings, true);
        settings->lpVtbl->put_IsWebMessageEnabled(settings, true);
        settings->lpVtbl->put_AreDevToolsEnabled(settings, true);
        settings->lpVtbl->put_AreDefaultContextMenusEnabled(settings, true);
        settings->lpVtbl->put_IsStatusBarEnabled(settings, true);

        HWND hWnd = g_hWnds[g_widgets];
        RECT bounds;
        GetClientRect(hWnd, &bounds);
        controller->lpVtbl->put_Bounds(controller, bounds);

        // Convert char* to wide char for the navigate function to work
        wchar_t wTpmlPath[BUFFSIZE];
        MultiByteToWideChar(CP_ACP, 0, g_tmplPath, -1, wTpmlPath, BUFFSIZE);

        // Registering event handlers
        EventRegistrationToken token;
        window->lpVtbl->add_WebMessageReceived(
                window, &messageReceivedEventHandler, &token);

        // Navigating to the requested URL or file
        window->lpVtbl->Navigate(window, wTpmlPath);

        return EXIT_REASON_TERMINATED;
}

/**
 * @brief Callback function used by the window cration process of the default
 * messages
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
                PostQuitMessage(EXIT_REASON_TERMINATED);
                return 0;
        case WM_SIZE:
                if (g_controllers[g_widgets] != nullptr)
                {
                        RECT bounds;
                        GetClientRect(g_hWnds[g_widgets], &bounds);
                        g_controllers[g_widgets]->lpVtbl->put_Bounds(
                                g_controllers[g_widgets], bounds);
                }
                return EXIT_REASON_TERMINATED;
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

static bool
create_widget_window(ww_window_ctx *context)
{
        // Copying the title of the window to a local variable here
        size_t len = strlen(context->title);
        if (len >= BUFFSIZE)
        {
                fprintf(stderr, "Window name was bigger than expectd\n");
                CoUninitialize();
                return true;
        }
        memcpy(g_tmplName, context->title, len);
        g_tmplName[len] = '\0';

        // Copying the complete path to the wiget in the machine
        len = strlen(context->filename);
        if (len >= BUFFSIZE)
        {
                fprintf(stderr,
                        "Path to the specified file location was too big\n");
                return true;
        }
        memcpy(g_tmplPath, context->filename, len);
        g_tmplPath[len] = '\0';

        WNDCLASS wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = g_hInstance;
        wc.lpszClassName = CLASS_NAME;

        RegisterClass(&wc);

        g_hWnds[g_widgets] =
                CreateWindowEx(0,                   // Optional window styles.
                               CLASS_NAME,          // Window class
                               g_tmplName,          // Window text
                               WS_OVERLAPPEDWINDOW, // Window style
                               CW_USEDEFAULT,       // X Position
                               CW_USEDEFAULT,       // Y Position
                               CW_USEDEFAULT,       // Width
                               CW_USEDEFAULT,       // Height
                               nullptr,             // Parent window
                               nullptr,             // Menu
                               g_hInstance,         // Instance handle
                               nullptr // Additional application data
                );
        if (g_hWnds[g_widgets] == nullptr)
        {
                fprintf(stderr, "Failed to create window\n");
                CoUninitialize();
                return true;
        }

        ShowWindow(g_hWnds[g_widgets], g_nCmdShow);
        UpdateWindow(g_hWnds[g_widgets]);

        // Createing the environment if it hasn't already been created
        if (!g_envCreated)
        {
                CreateCoreWebView2EnvironmentWithOptions(
                        nullptr, nullptr, nullptr, &envHandler);
                return true;
        }

        // Just opening a new window
        g_env->lpVtbl->CreateCoreWebView2Controller(
                g_env, g_hWnds[g_widgets], &completedHandler);
        return true;
}

/*
 * @brief Initializing the main window of the application. It will be
 * responsible for spawning children widgets.
 * @returns The status code of the function at its conclusion
 */
bool
ww_init_main(HINSTANCE hInstance,
             int nCmdShow,
             ww_window_ctx *context,
             ww_widget_ctx *widgets)
{
        g_hInstance = hInstance;
        g_nCmdShow = nCmdShow;

        if (FAILED(CoInitialize(NULL)))
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
