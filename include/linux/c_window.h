
#ifndef C_WINDOW_H
#define C_WINDOW_H

#include "utils.h"

typedef struct window_t window_t;
typedef struct window_vtable_t window_vtable_t;
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
} ww_window_state_t;

struct window_vtable_t
{
        void (*destroy)(window_t *);
        void (*set_url)(window_t *, string);
        void (*show)(window_t *);
        void (*set_size)(window_t *, size_t, size_t);
        void (*set_position)(window_t *, size_t, size_t);
        void (*set_hide_from_taskbar)(window_t *, bool);
        void (*set_hide_from_pager)(window_t *, bool);
        void (*set_title)(window_t *, string);
        void (*set_state)(window_t *, ww_window_state_t);
        void *(*get_webview)(window_t *);
        void *(*get_manager)(window_t *);
        void *(*get_window)(window_t *);
        bool (*get_state)(window_t *, ww_window_state_t);
        void (*clear_state)(window_t *, ww_window_state_t);
        void (*register_event_callback)(window_t *,
                                        void *,
                                        string,
                                        void (*cb)(void *, void *, void *),
                                        void *);
        void (*register_event_mouse_motion)(window_t *,
                                            void (*cb)(void *,
                                                       const size_t,
                                                       const size_t));
        void (*register_event_mouse_press)(
                window_t *,
                void (*cb)(void *, const ww_window_mouse_press_event_t));
        void (*register_event_context_menu)(
                window_t *,
                void (*cb)(void *, const ww_window_context_menu_selection_t));
        void (*run_javascript)(window_t *, string);
        void (*add_child)(window_t *, window_t *);
        void (*destroy_chain)(window_t *);
};

struct window_t
{
        window_vtable_t *vtable;
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
};

window_t *
ww_window_new(window_t options);

#endif
