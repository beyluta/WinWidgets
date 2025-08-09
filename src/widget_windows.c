#include "global.h"
#include "widget.h"
#include <minwindef.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <winscard.h>
#include <WebView2.h>

constexpr char CLASS_NAME[] = "WidgetClass";

// ----------------------------------------------------------
// Global variables to control the state of the main window |
// ----------------------------------------------------------
static HWND hWndParent = nullptr;
static ICoreWebView2Controller *webviewController = nullptr;
static ICoreWebView2 *webviewWindow = nullptr;
static bool bEnvCreated = false;
static ULONG HandlerRefCount = 0;

// ----------------------------------------------------------------
// Global variables to control the state of current opened widget |
// ----------------------------------------------------------------
static char tmplName[BUFFSIZE] = {};
static char tmplPath[BUFFSIZE] = {};

// --------------------------------------------------
// Vtable functions for creating the default window |
// --------------------------------------------------
static ULONG HandlerAddRef(IUnknown *) { return ++HandlerRefCount; }
static ULONG HandlerRelease(IUnknown *) { return --HandlerRefCount; }
static HRESULT HandlerQueryInterface(IUnknown *this, const IID *riid,
                                     void **ppvObject) {
  *ppvObject = this;
  HandlerAddRef(this);
  return EXIT_REASON_TERMINATED;
}

static HRESULT HandlerInvoke(IUnknown *this, HRESULT errorCode,
                             ICoreWebView2Controller *arg);
ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl
    completedHandlerVtbl = {.AddRef = (void *)HandlerAddRef,
                            .Release = (void *)HandlerRelease,
                            .QueryInterface = (void *)HandlerQueryInterface,
                            .Invoke = (void *)HandlerInvoke};
ICoreWebView2CreateCoreWebView2ControllerCompletedHandler completedHandler = {
    .lpVtbl = &completedHandlerVtbl};

static HRESULT HandlerInvoke(IUnknown *this, HRESULT errorCode,
                             ICoreWebView2Controller *arg) {
  if (!bEnvCreated) {
    bEnvCreated = true;

    ICoreWebView2Environment *env = (ICoreWebView2Environment *)arg;
    env->lpVtbl->CreateCoreWebView2Controller(env, hWndParent,
                                              &completedHandler);
    return EXIT_REASON_TERMINATED;
  }

  ICoreWebView2Controller *controller = arg;
  if (controller != nullptr) {
    webviewController = controller;
    webviewController->lpVtbl->get_CoreWebView2(webviewController,
                                                &webviewWindow);
    webviewController->lpVtbl->AddRef(webviewController);
  }

  ICoreWebView2Settings *settings;
  webviewWindow->lpVtbl->get_Settings(webviewWindow, &settings);
  settings->lpVtbl->put_IsScriptEnabled(settings, true);
  settings->lpVtbl->put_AreDefaultScriptDialogsEnabled(settings, true);
  settings->lpVtbl->put_IsWebMessageEnabled(settings, true);
  settings->lpVtbl->put_AreDevToolsEnabled(settings, true);
  settings->lpVtbl->put_AreDefaultContextMenusEnabled(settings, true);
  settings->lpVtbl->put_IsStatusBarEnabled(settings, true);

  RECT bounds;
  GetClientRect(hWndParent, &bounds);
  webviewController->lpVtbl->put_Bounds(webviewController, bounds);
  webviewWindow->lpVtbl->Navigate(webviewWindow, L"https://google.com");
  return EXIT_REASON_TERMINATED;
}

// ---------------------------------------------------------------------
// Callback function used by the window cration process of the default |
// windows api to process messages.                                    |
// ---------------------------------------------------------------------
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_DESTROY:
    PostQuitMessage(EXIT_REASON_TERMINATED);
    return 0;
  case WM_SIZE:
    if (webviewController != nullptr) {
      RECT bounds;
      GetClientRect(hWndParent, &bounds);
      webviewController->lpVtbl->put_Bounds(webviewController, bounds);
    }
    return EXIT_REASON_TERMINATED;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// -----------------------------------------------------
// Event loop of the application.                      |
// -----------------------------------------------------
static bool event_loop() {
  MSG msg;
  BOOL bRet;
  while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
    // An error occured
    if (bRet == -1) {
      break;
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  return false;
}

// --------------------------------------------------------------------------
// Here we are linking the previously declared functions to the pointers in |
// the vtables.                                                             |
// --------------------------------------------------------------------------
ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl envHandlerVtbl =
    {.AddRef = (void *)HandlerAddRef,
     .Release = (void *)HandlerRelease,
     .QueryInterface = (void *)HandlerQueryInterface,
     .Invoke = (void *)HandlerInvoke};
ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler envHandler = {
    .lpVtbl = &envHandlerVtbl};

// --------------------------------------------------------------------------
// Initializing the main window of the application. It will be responsible  |
// for spawning children widgets.                                           |
// --------------------------------------------------------------------------
bool ww_init_main(HINSTANCE hInstance, int nCmdShow, ww_window_ctx *context,
                  ww_widget_ctx *widgets) {
  if (FAILED(CoInitialize(NULL))) {
    fprintf(stderr, "COM Failed to initialize\n");
    return true;
  }

  // Copying the title of the window to a local variable here
  size_t len = strlen(context->title);
  if (len >= BUFFSIZE) {
    fprintf(stderr, "Window name was bigger than expectd\n");
    CoUninitialize();
    return true;
  }
  memcpy(tmplName, context->title, len);
  tmplName[len] = '\0';

  // Copying the complete path to the wiget in the machine
  len = strlen(context->filename);
  if (len >= BUFFSIZE) {
    fprintf(stderr, "Path to the specified file location was too big\n");
    return true;
  }
  memcpy(tmplPath, context->filename, len);
  tmplPath[len] = '\0';
  printf("Path is: %s\n", tmplPath);

  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;

  RegisterClass(&wc);

  hWndParent = CreateWindowEx(0,                   // Optional window styles.
                              CLASS_NAME,          // Window class
                              tmplName,            // Window text
                              WS_OVERLAPPEDWINDOW, // Window style
                              CW_USEDEFAULT,       // X Position
                              CW_USEDEFAULT,       // Y Position
                              CW_USEDEFAULT,       // Width
                              CW_USEDEFAULT,       // Height
                              NULL,                // Parent window
                              NULL,                // Menu
                              hInstance,           // Instance handle
                              NULL // Additional application data
  );
  if (hWndParent == nullptr) {
    fprintf(stderr, "Failed to create window\n");
    CoUninitialize();
    return true;
  }

  ShowWindow(hWndParent, nCmdShow);
  UpdateWindow(hWndParent);

  CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
                                           &envHandler);
  return event_loop();
}
