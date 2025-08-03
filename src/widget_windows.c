#include "global.h"
#include "widget.h"
#include <minwindef.h>
#include <stdio.h>
#include <windows.h>
#include <winscard.h>

constexpr char CLASS_NAME[] = "WidgetClass";

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_DESTROY:
    PostQuitMessage(EXIT_REASON_TERMINATED);
    return 0;
  case WM_SIZE:
    if (!lParam) {
      fprintf(stderr, "Additional message information missing\n");
      return 0;
    }

    HWND hBrowser = FindWindowExA(hwnd, nullptr, "Shell Embedding", nullptr);
    if (hBrowser == nullptr) {
      fprintf(stderr, "Handle to the browser was not found\n");
      return EXIT_REASON_TERMINATED;
    }
    SetWindowPos(hBrowser, nullptr, 0, 0, LOWORD(lParam), HIWORD(lParam),
                 SWP_NOZORDER);

    return EXIT_REASON_TERMINATED;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static bool event_loop() {
  MSG msg = {};
  while (GetMessage(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return false;
}

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

  HWND hwnd =
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
  if (hwnd == nullptr) {
    fprintf(stderr, "Failed to create window\n");
    CoUninitialize();
    return true;
  }

  ShowWindow(hwnd, nCmdShow);

  return event_loop();
}
