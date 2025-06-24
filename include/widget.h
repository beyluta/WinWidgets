#ifndef WIDGET_H
#define WIDGET_H

#include "global.h"
#include <stddef.h>

/**
 * @brief Window context structure for widgets.
 * Holds application-specific context data for widget windows.
 */
typedef struct ww_window_ctx {
  size_t width;
  size_t height;
  size_t x;
  size_t y;
  BOOLEAN title_bar;
  BOOLEAN child;
  BOOLEAN top_most;
  char filename[BUFFSIZE];
  char title[BUFFSIZE];
  double opacity;
  double radius;
} ww_window_ctx;

/**
 * @brief Widget context structure for widgets.
 * Holds pointers to dynamic data.
 */
typedef struct ww_widget_ctx {
  ww_window_ctx window_context;
  const void *window;
} ww_widget_ctx;

/**
 * @brief Creates the main window of the application
 * @param context Window context
 * @param widgets A pointer to an array of widgets
 * @return Status of the function execution
 */
BOOLEAN ww_init_main(ww_window_ctx *context, ww_widget_ctx *widgets);

#endif
