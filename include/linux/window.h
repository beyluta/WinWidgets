
#ifndef WINDOW_H
#define WINDOW_H

#include "utils.h"

typedef struct window_opts_t window_opts_t;

typedef enum : uint8_t
{
        WINDOW_MOUSE_PRESS_EVENT_LEFT = 1,
        WINDOW_MOUSE_PRESS_EVENT_MIDDLE = 2,
        WINDOW_MOUSE_PRESS_EVENT_RIGHT = 3,
} ww_window_mouse_press_event_t;

typedef enum : uint8_t
{
        WINDOW_CONTEXT_MENU_SELECTION_CLOSE = 1
} ww_window_context_menu_selection_t;

typedef struct
{
        window_opts_t *private;
        size_t width;
        size_t height;
        size_t x;
        size_t y;
        size_t guid;
        uint8_t opacity;
        uint8_t radius;
        bool show_title_bar;
        bool is_child;
} window_t;

window_t *
window_new(const window_t options,
           const char *const title,
           const size_t title_len,
           const size_t guid,
           void (*cb_window_realized)(window_t *));

void
window_destroy(window_t *self);

void
window_set_url(const window_t *const self,
               const string url,
               const size_t url_len);

void
window_set_transparency(window_t *const self, const double alpha);

void
window_show(window_t *self);

void
window_set_size(window_t *self, size_t width, size_t height);

void
window_set_position(window_t *self, size_t x, size_t y);

void
window_set_hide_from_taskbar(window_t *self, bool status);

void
window_set_hide_from_pager(window_t *self, bool status);

void
window_set_title(window_t *self, string title);

void *
window_get_webview(window_t *self);

void *
window_get_manager(window_t *self);

void *
window_get_window(window_t *self);

void
window_register_event_callback(window_t *self,
                               void *instance,
                               string event,
                               void (*cb)(void *, void *, void *),
                               void *data);

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

size_t
window_save_state(window_t *const self);

void
window_save_state_remove(window_t *const self);

#endif
