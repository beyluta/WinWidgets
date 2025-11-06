#ifndef WIDGET_H
#define WIDGET_H

#include "global.h"
#include <stddef.h>
#include <stdint.h>

#if _WIN32
#include <minwindef.h>
#endif

constexpr char TAG_APP_NAME[] = "applicationTitle";
constexpr char TAG_APP_TOPMOST[] = "topMost";
constexpr char TAG_WIN_SIZE[] = "windowSize";
constexpr char TAG_WIN_LOCATION[] = "windowLocation";
constexpr char TAG_WIN_PREV[] = "previewSize";
constexpr char TAG_WIN_BORD_RAD[] = "windowBorderRadius";
constexpr char TAG_WIN_OPACITY[] = "windowOpacity";

constexpr uint16_t DEF_WIDTH = 500;
constexpr uint16_t DEF_HEIGHT = 500;
constexpr uint16_t DEF_PREV_WIDTH = DEF_WIDTH;
constexpr uint16_t DEF_PREV_HEIGHT = DEF_HEIGHT;
constexpr uint16_t DEF_X = 0;
constexpr uint16_t DEF_Y = 0;
constexpr uint16_t DEF_OPACITY = 1;
constexpr uint16_t DEF_RADIUS = 0;
constexpr bool DEF_CHILD = true;
constexpr bool DEF_TOPMOST = false;

/**
 * @brief Window context structure for widgets.
 * Holds application-specific context data for widget windows.
 */
typedef struct ww_window_ctx
{
        size_t width;
        size_t height;
        size_t prevWidth;
        size_t prevHeight;
        size_t x;
        size_t y;
        size_t index;
        bool title_bar;
        bool child;
        bool top_most;
        char filename[BUFFSIZE];
        char title[BUFFSIZE];
        double opacity;
        double radius;
} ww_window_ctx;

/**
 * @brief Widget context structure for widgets.
 * Holds pointers to dynamic data.
 */
typedef struct ww_widget_ctx
{
        ww_window_ctx window_context;
        const void *window;
} ww_widget_ctx;

/**
 * @brief Creates the main window of the application
 * @param context Window context
 * @param widgets A pointer to an array of widgets
 * @return Status of the function execution
 */
#if __linux__
bool
ww_init_main(ww_window_ctx *context, ww_widget_ctx *widgets);
#elif _WIN32
bool
ww_init_main(HINSTANCE hInstance,
             int nCmdShow,
             ww_window_ctx *context,
             ww_widget_ctx *widgets);
#endif

#endif
