#include "global.h"
#include "widget.h"
#include <minwindef.h>
#include <stdio.h>
#include <windows.h>
#include <winscard.h>
#include <WebView2.h>

constexpr char CLASS_NAME[] = "WidgetClass";

static HWND hWnd = nullptr;
static ICoreWebView2Controller *webviewController = nullptr;
static ICoreWebView2 *webviewWindow = nullptr;
static bool bEnvCreated = false;
static ULONG HandlerRefCount = 0;

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
    env->lpVtbl->CreateCoreWebView2Controller(env, hWnd, &completedHandler);
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
  GetClientRect(hWnd, &bounds);
  webviewController->lpVtbl->put_Bounds(webviewController, bounds);
  webviewWindow->lpVtbl->Navigate(webviewWindow, L"https://google.com");
  return EXIT_REASON_TERMINATED;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_DESTROY:
    PostQuitMessage(EXIT_REASON_TERMINATED);
    return 0;
  case WM_SIZE:
    if (webviewController != nullptr) {
      RECT bounds;
      GetClientRect(hWnd, &bounds);
      webviewController->lpVtbl->put_Bounds(webviewController, bounds);
    }
    return EXIT_REASON_TERMINATED;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

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

ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl envHandlerVtbl =
    {.AddRef = (void *)HandlerAddRef,
     .Release = (void *)HandlerRelease,
     .QueryInterface = (void *)HandlerQueryInterface,
     .Invoke = (void *)HandlerInvoke};

ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler envHandler = {
    .lpVtbl = &envHandlerVtbl};

bool ww_init_main(HINSTANCE hInstance, int nCmdShow, ww_window_ctx *context,
                  ww_widget_ctx *widgets) {
  if (FAILED(CoInitialize(NULL))) {
    fprintf(stderr, "COM Failed to initialize\n");
    return true;
  }

  char WINDOW_NAME[BUFFSIZE];
  const size_t len = strlen(context->title);
  if (len >= BUFFSIZE) {
    fprintf(stderr, "Window name was bigger than expectd\n");
    CoUninitialize();
    return true;
  }
  memcpy(WINDOW_NAME, context->title, len);
  WINDOW_NAME[len] = '\0';

  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;

  RegisterClass(&wc);

  hWnd =
      CreateWindowEx(0,                   // Optional window styles.
                     CLASS_NAME,          // Window class
                     WINDOW_NAME,         // Window text
                     WS_OVERLAPPEDWINDOW, // Window style
                     // Size and position
                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                     NULL,      // Parent window
                     NULL,      // Menu
                     hInstance, // Instance handle
                     NULL       // Additional application data
      );
  if (hWnd == nullptr) {
    fprintf(stderr, "Failed to create window\n");
    CoUninitialize();
    return true;
  }

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
                                           &envHandler);
  return event_loop();
}
