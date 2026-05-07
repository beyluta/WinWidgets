
#ifndef WINDOW_H
#define WINDOW_H

#include "utils.h"

typedef struct window_private_t window_private_t;

typedef enum : uint8_t
{
        WINDOW_MOUSE_PRESS_EVENT_LEFT = 1,
        WINDOW_MOUSE_PRESS_EVENT_MIDDLE = 2,
        WINDOW_MOUSE_PRESS_EVENT_RIGHT = 3,
} ww_window_mouse_press_event_t;

typedef enum : uint8_t
{
        WINDOW_CONTEXT_MENU_SELECTION_MOVE,
        WINDOW_CONTEXT_MENU_SELECTION_TOPMOST,
        WINDOW_CONTEXT_MENU_SELECTION_CLOSE,
} ww_window_context_menu_selection_t;

typedef enum : uint8_t
{
        WINDOW_STATE_NONE = 1,
        WINDOW_STATE_MOVING = 2,
        WINDOW_STATE_TOPMOST = 4,
} ww_window_state_t;

typedef struct
{
        window_private_t *private;
        string title;
        size_t width;
        size_t height;
        size_t x;
        size_t y;
        size_t guid;
        uint8_t opacity;
        uint8_t radius;
        bool show_title_bar;
        bool is_child;
        bool is_top_most;
} window_t;

window_t *
window_new(window_t options);

void
window_destroy(window_t *self);

void
window_set_url(window_t *self, string url);

void
window_show(window_t *self);

void
window_set_size(window_t *self, size_t width, size_t height);

void
window_set_position(window_t *self, size_t x, size_t y);

void
window_set_topmost(window_t *self, bool state);

void
window_set_hide_from_taskbar(window_t *self, bool status);

void
window_set_hide_from_pager(window_t *self, bool status);

void
window_set_title(window_t *self, string title);

void
window_set_state(window_t *self, ww_window_state_t state);

void
window_clear_state(window_t *self, ww_window_state_t state);

void *
window_get_webview(window_t *self);

void *
window_get_manager(window_t *self);

void *
window_get_window(window_t *self);

bool
window_get_state(window_t *self, ww_window_state_t state);

void
window_register_event_callback(window_t *self,
                               void *instance,
                               string event,
                               void (*cb)(void *, void *, void *),
                               void *data);

void
window_register_event_mouse_motion(window_t *self,
                                   void (*cb)(void *,
                                              const size_t,
                                              const size_t));

void
window_register_event_mouse_press(
        window_t *self,
        void (*cb)(void *, const ww_window_mouse_press_event_t));

void
window_register_event_context_menu(
        window_t *self,
        void (*cb)(void *, const ww_window_context_menu_selection_t));

void
window_run_javascript(window_t *self, string script);

void
window_add_child(window_t *self, window_t *child);

void
window_destroy_chain(window_t *self);

#endif
