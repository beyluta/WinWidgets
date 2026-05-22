// ======================= Purpose ==========================
//
// Responsible for the implementation of the window for the
// linux platform.
//
// ==========================================================
#include "widget.h"
#include "window.h"
#include "filesystem.h"

// Private members

static constexpr char SIGNAL_DESTROY[] = "destroy";
static constexpr char SIGNAL_REALIZE[] = "realize";
static constexpr char SIGNAL_ACTIVATE[] = "activate";
static constexpr char SIGNAL_BTN_PRESS[] = "button-press-event";
static constexpr char SIGNAL_MOUSE_MOVE[] = "motion-notify-event";
static constexpr char SIGNAL_CTX_MENU[] = "context-menu";
static constexpr char YAML_FILE_SUFFIX[] = ".yaml";

struct window_opts_t
{
        window_t *next;
        window_t *parent;
        GtkWidget *window;
        WebKitWebView *webview;
        WebKitUserContentManager *manager;
        void (*cb_window_realized)(window_t *);
        void (*cb_mouse_button_press)(void *, const uint8_t);
        void (*cb_mouse_move)(void *, const size_t, const size_t);
        void (*cb_mouse_move_end)(window_t *);
        void (*cb_mouse_top_most_changed)(window_t *);
        void (*cb_context_menu_open)(void *,
                                     const ww_window_context_menu_selection_t);
        ww_window_state_t state;
        string8_t title;
        string12_t url;
};

static size_t
window_generate_id()
{
        static bool initialized = false;
        if (!initialized)
        {
                srand(time(nullptr));
                initialized = true;
        }

        return rand();
}

static void
on_window_destroy(GtkWidget *window, gpointer data)
{
        window_t *self = (window_t *)data;

        if (self->private != nullptr)
        {
                free(self->private);
                self->private = nullptr;
        }

        if (self != nullptr)
        {
                free(self);
                self = nullptr;
        }
}

static void
destroy_window(void *, void *data)
{
        window_t *self = (window_t *)data;

        if (!self->is_child)
        {
                gtk_main_quit();
                return;
        }

        window_t *prev = nullptr;

        for (window_t *node = self->private->parent; node != nullptr;
             node = node->private->next)
        {

                if (self->guid != node->guid)
                {
                        prev = node;
                        continue;
                }

                if (prev == nullptr)
                {
                        break;
                }

                prev->private->next = self->private->next;

                break;
        }

        window_save_state_remove(self);

        window_destroy(self);
}

static void
on_window_realized(void *, void *data)
{
        window_t *self = (window_t *)data;

        if (self->private->cb_window_realized == nullptr)
        {
                return;
        }

        self->private->cb_window_realized(self);
}

static gboolean
on_mouse_move(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
        window_t *self = (window_t *)data;

        double x = event->x_root - ((double)self->width / 2);
        if (x < 0)
        {
                x = 0;
        }

        double y = event->y_root - ((double)self->height / 2);
        if (y < 0)
        {
                y = 0;
        }

        self->private->cb_mouse_move(self, (size_t)x, (size_t)y);

        return FALSE;
}

static gboolean
on_mouse_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
        window_t *self = (window_t *)data;
        self->private->cb_mouse_button_press(self, event->button);
        return FALSE;
}

static gboolean
on_context_menu_item_move_selected(GAction *action,
                                   GVariant *target,
                                   gpointer data)
{
        window_t *self = (window_t *)data;
        self->private->cb_context_menu_open(self,
                                            WINDOW_CONTEXT_MENU_SELECTION_MOVE);
        return FALSE;
}

static gboolean
on_context_menu_item_topmost_selected(GAction *action,
                                      GVariant *target,
                                      gpointer data)
{
        window_t *self = (window_t *)data;
        self->private->cb_context_menu_open(
                self, WINDOW_CONTEXT_MENU_SELECTION_TOPMOST);
        return FALSE;
}

static gboolean
on_context_menu_item_close_selected(GAction *action,
                                    GVariant *target,
                                    gpointer data)
{
        window_t *self = (window_t *)data;
        self->private->cb_context_menu_open(
                self, WINDOW_CONTEXT_MENU_SELECTION_CLOSE);
        return FALSE;
}

static GSimpleAction *
create_and_append_menu_item(const string label,
                            const ww_window_context_menu_selection_t,
                            WebKitContextMenu *context_menu,
                            window_t *const self,
                            gboolean (*cb)(GAction *, GVariant *, gpointer))
{

        GSimpleAction *action = g_simple_action_new(label, NULL);
        WebKitContextMenuItem *item = webkit_context_menu_item_new_from_gaction(
                (GAction *)action, label, NULL);
        webkit_context_menu_append(context_menu, item);
        g_signal_connect(action, SIGNAL_ACTIVATE, G_CALLBACK(cb), self);
        return action;
}

static gboolean
on_context_menu_open(WebKitWebView *webview,
                     WebKitContextMenu *context_menu,
                     GdkEvent *event,
                     WebKitHitTestResult *hit_test_result,
                     gpointer user_data)
{
        window_t *self = (window_t *)user_data;

        webkit_context_menu_remove_all(context_menu);

        GSimpleAction *action_move =
                create_and_append_menu_item("Move",
                                            WINDOW_CONTEXT_MENU_SELECTION_MOVE,
                                            context_menu,
                                            self,
                                            on_context_menu_item_move_selected);

        GSimpleAction *action_topmost = create_and_append_menu_item(
                "Toggle Topmost",
                WINDOW_CONTEXT_MENU_SELECTION_TOPMOST,
                context_menu,
                self,
                on_context_menu_item_topmost_selected);

        GSimpleAction *action_close = create_and_append_menu_item(
                "Close",
                WINDOW_CONTEXT_MENU_SELECTION_CLOSE,
                context_menu,
                self,
                on_context_menu_item_close_selected);

        g_object_unref(action_move);
        g_object_unref(action_topmost);
        g_object_unref(action_close);

        return FALSE;
}

// Public members

void
window_register_event_mouse_press(
        window_t *self,
        void (*cb)(void *, const ww_window_mouse_press_event_t))
{
        GtkWidget *window = GTK_WIDGET(self->private->webview);
        gtk_widget_add_events(window,
                              GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
        self->private->cb_mouse_button_press = cb;
        g_signal_connect(window,
                         SIGNAL_BTN_PRESS,
                         G_CALLBACK(on_mouse_button_press),
                         self);
}

void
window_register_event_mouse_motion(window_t *self,
                                   void (*cb)(void *,
                                              const size_t,
                                              const size_t))
{
        GtkWidget *window = self->private->window;
        gtk_widget_add_events(window, GDK_POINTER_MOTION_MASK);
        self->private->cb_mouse_move = cb;
        g_signal_connect(
                window, SIGNAL_MOUSE_MOVE, G_CALLBACK(on_mouse_move), self);
}

void
window_register_event_context_menu(
        window_t *self,
        void (*cb)(void *, const ww_window_context_menu_selection_t))
{
        GtkWidget *window = GTK_WIDGET(self->private->webview);
        gtk_widget_add_events(window,
                              GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
        self->private->cb_context_menu_open = cb;
        g_signal_connect(window,
                         SIGNAL_CTX_MENU,
                         G_CALLBACK(on_context_menu_open),
                         self);
}

void
window_run_javascript(window_t *self, string script)
{
        webkit_web_view_evaluate_javascript(
                WEBKIT_WEB_VIEW(self->private->webview),
                script,
                -1,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr);
}

void
window_add_child(window_t *self, window_t *child)
{
        window_t *prev = nullptr;
        for (window_t *node = self; node != nullptr; node = node->private->next)
        {
                prev = node;
        }
        prev->private->next = child;
        child->private->parent = self;
}

void
window_destroy_chain(window_t *self)
{
        window_t *next = nullptr;
        for (window_t *node = self; node != nullptr; node = next)
        {
                next = node->private->next;
                window_destroy(node);
        }
}

void
window_set_transparency(window_t *const self, const double alpha)
{
        WebKitWebView *webview = self->private->webview;
        GtkWidget *widget = self->private->window;
        GtkWindow *window = GTK_WINDOW(widget);

        GdkScreen *screen = gtk_widget_get_screen(widget);
        GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
        if (visual == nullptr || !gdk_screen_is_composited(screen))
        {
                return;
        }

        gtk_widget_set_visual(widget, visual);
        gtk_widget_set_app_paintable(widget, true);

        GdkRGBA rgba = {0, 0, 0, alpha};
        webkit_web_view_set_background_color(webview, &rgba);
}

void
window_show(window_t *self)
{
        window_opts_t *opts = self->private;

        window_set_title(self, opts->title);
        window_set_size(self, self->width, self->height);
        window_set_position(self, self->x, self->y);
        window_set_hide_from_pager(self, self->is_child);
        window_set_hide_from_taskbar(self, !self->is_child);

        webkit_web_view_load_uri(opts->webview, opts->url);
        gtk_container_add(GTK_CONTAINER(opts->window),
                          GTK_WIDGET(opts->webview));

        g_signal_connect(
                opts->window, SIGNAL_DESTROY, G_CALLBACK(destroy_window), self);

        if (!self->is_child)
        {
                g_signal_connect(opts->window,
                                 SIGNAL_REALIZE,
                                 G_CALLBACK(on_window_realized),
                                 self);

                gtk_widget_show_all(opts->window);

                gtk_main();

                return;
        }
        else
        {
                window_set_transparency(self, 0);
                gtk_widget_show_all(opts->window);
        }

        GtkWindow *window = GTK_WINDOW(opts->window);
        gtk_window_set_decorated(window, FALSE);
}

void
window_set_size(window_t *self, size_t width, size_t height)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_set_default_size(window, width, height);
}

void
window_set_position(window_t *self, size_t x, size_t y)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_move(window, x, y);
        self->x = x;
        self->y = y;
}

void
window_set_topmost(window_t *self, bool state)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_set_keep_above(window, state);

        if (state)
        {
                window_set_state(self, WINDOW_STATE_TOPMOST);
                return;
        }

        window_clear_state(self, WINDOW_STATE_TOPMOST);
}

void
window_set_hide_from_taskbar(window_t *self, bool status)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_set_skip_taskbar_hint(window, status);
}

void
window_set_hide_from_pager(window_t *self, bool status)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_set_skip_pager_hint(window, status);
}

void
window_set_title(window_t *self, string title)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_set_title(window, title);
}

void
window_set_state(window_t *self, ww_window_state_t state)
{
        self->private->state = self->private->state | state;

        switch (state)
        {
        default:
        {
                break;
        }
        case WINDOW_STATE_TOPMOST:
        {
                if (self->private->cb_mouse_top_most_changed == nullptr)
                {
                        return;
                }

                self->private->cb_mouse_top_most_changed(self);

                break;
        }
        }
}

void *
window_get_webview(window_t *self)
{
        return self->private->webview;
}

void *
window_get_manager(window_t *self)
{
        return self->private->manager;
}

void *
window_get_window(window_t *self)
{
        return self->private->window;
}

bool
window_get_state(window_t *self, ww_window_state_t state)
{
        return (self->private->state & state) == state;
}

void
window_clear_state(window_t *self, ww_window_state_t state)
{
        self->private->state = self->private->state & (~state);

        switch (state)
        {
        default:
        {
                break;
        }
        case WINDOW_STATE_MOVING:
        {
                if (self->private->cb_mouse_move_end == nullptr)
                {
                        return;
                }

                self->private->cb_mouse_move_end(self);

                break;
        }
        case WINDOW_STATE_TOPMOST:
        {
                if (self->private->cb_mouse_top_most_changed == nullptr)
                {
                        return;
                }

                self->private->cb_mouse_top_most_changed(self);

                break;
        }
        }
}

void
window_register_event_callback(window_t *self,
                               void *instance,
                               string event,
                               void (*cb)(void *, void *, void *),
                               void *data)
{
        const string prefix = "script-message-received::";
        const size_t totalLen = strlen(prefix) + strlen(event);
        if (totalLen >= MAX_STR_SIZE)
        {
                return;
        }

        char buffer[MAX_STR_SIZE + 1];
        snprintf(buffer, totalLen + 1, "%s%s", prefix, event);

        webkit_user_content_manager_register_script_message_handler(
                self->private->manager, event);

        g_signal_connect(instance, buffer, G_CALLBACK(cb), data);
}

void
window_destroy(window_t *self)
{
        GtkWidget *window = self->private->window;
        g_signal_connect(self->private->window,
                         SIGNAL_DESTROY,
                         G_CALLBACK(on_window_destroy),
                         self);
        gtk_window_close(GTK_WINDOW(window));
}

size_t
window_save_state(window_t *const self)
{
        char buffer[MAX_FILE_SIZE];
        ssize_t n = snprintf(buffer,
                             sizeof(buffer) - 1,
                             "x: %zu\n"
                             "y: %zu\n"
                             "url: %s\n"
                             "guid: %zu\n"
                             "top_most: %b\n",
                             self->x,
                             self->y,
                             self->private->url,
                             self->guid,
                             window_get_state(self, WINDOW_STATE_TOPMOST));
        if (n < 0)
        {
                return 0;
        }

        char fb[PATH_MAX];
        if ((n = ww_default_widgets_dir(fb, sizeof(fb) - 1)) == 0)
        {
                return 0;
        }

        if ((n = snprintf(&fb[n], sizeof(fb) - n, "/%zu.yaml", self->guid)) < 0)
        {
                return 0;
        }

        if (ww_write_to_file(fb, buffer, WRITE_OVERWRITE))
        {
                return 0;
        }

        return n;
}

void
window_save_state_remove(window_t *const self)
{
        char fb[PATH_MAX];
        size_t n = ww_default_widgets_dir(fb, sizeof(fb) - 1);
        if (n == 0)
        {
                return;
        }

        if ((n = snprintf(&fb[n], sizeof(fb) - n, "/%zu.yaml", self->guid)) < 0)
        {
                return;
        }

        if (remove(fb) != 0)
        {
                return;
        }
}

void
window_register_event_mouse_motion_end(window_t *const self,
                                       void (*cb)(window_t *))
{
        self->private->cb_mouse_move_end = cb;
}

void
window_register_event_top_most_changed(window_t *const self,
                                       void (*cb)(window_t *))
{
        self->private->cb_mouse_top_most_changed = cb;
}

void
window_set_url(const window_t *const self,
               const string url,
               const size_t url_len)
{
        if (url_len >= sizeof(self->private->url))
        {
                fprintf(stderr, "URL size is greater than allowed\n");
                exit(1);
        }

        const string url_ptr = self->private->url;
        strncpy(url_ptr, url, url_len);
        url_ptr[url_len] = '\0';
}

window_t *
window_new(const window_t options,
           const char *const title,
           const size_t title_len,
           const size_t guid,
           void (*cb_window_realized)(window_t *))
{
        window_t *window = nullptr;
        window_opts_t *opts = nullptr;

        if ((window = (window_t *)malloc(sizeof(window_t))) == nullptr)
        {
                fprintf(stderr, "Memory allocation for new window failed\n");
                goto cleanup;
        }

        if ((opts = (window_opts_t *)malloc(sizeof(window_opts_t))) == nullptr)
        {
                fprintf(stderr, "Memory allocation for window_opts_t failed\n");
                goto cleanup;
        }

        if (title_len >= sizeof(opts->title))
        {
                fprintf(stderr, "Title size was greater than supported\n");
                goto cleanup;
        }

        *window = options;

        string title_ptr = opts->title;
        strncpy(title_ptr, title, title_len);
        title_ptr[title_len] = '\0';

        static bool gtk_initialized = false;
        if (!gtk_initialized)
        {
                gtk_init(nullptr, nullptr);
                gtk_initialized = true;
        }

        GtkWidget *gtk_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
        WebKitUserContentManager *manager =
                webkit_web_view_get_user_content_manager(webview);
        opts->window = gtk_window;
        opts->webview = webview;
        opts->manager = manager;
        opts->state = WINDOW_STATE_NONE;
        opts->next = nullptr;
        opts->parent = nullptr;
        opts->cb_window_realized = cb_window_realized;
        opts->cb_mouse_move_end = nullptr;
        opts->cb_mouse_move = nullptr;
        opts->cb_mouse_top_most_changed = nullptr;
        opts->cb_mouse_button_press = nullptr;
        opts->cb_context_menu_open = nullptr;

        window->guid = guid > 0 ? guid : window_generate_id();
        window->private = opts;

        return window;

cleanup:
        if (opts != nullptr)
        {
                free(opts);
        }

        if (window != nullptr)
        {
                free(window);
        }

        exit(1);
}
