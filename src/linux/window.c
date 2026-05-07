// ======================= Purpose ==========================
//
// Responsible for the implementation of the window for the
// linux platform.
//
// ==========================================================
#include "widget.h"
#include "window.h"

// Private members

struct window_private_t
{
        string url;
        window_t *next;
        window_t *parent;
        GtkWidget *window;
        WebKitWebView *webview;
        WebKitUserContentManager *manager;
        void (*cb_mouse_button_press)(void *, const uint8_t);
        void (*cb_mouse_move)(void *, const size_t, const size_t);
        void (*cb_context_menu_open)(void *,
                                     const ww_window_context_menu_selection_t);
        ww_window_state_t state;
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
window_destroy_private_members(window_private_t *private)
{
        if (private->url != nullptr)
        {
                free(private->url);
                private->url = nullptr;
        }
}

static void
on_window_destroy(GtkWidget *window, gpointer data)
{
        window_t *self = (window_t *)data;

        if (self->title != nullptr)
        {
                free(self->title);
                self->title = nullptr;
        }

        if (self->private != nullptr)
        {
                window_destroy_private_members(self->private);
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

        if (self->is_child)
        {
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

                window_destroy(self);
        }
        else
        {
                gtk_main_quit();
        }
}

static gboolean
on_mouse_move(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
        window_t *self = (window_t *)data;
        const size_t x = (size_t)event->x_root - (self->width / 2);
        const size_t y = (size_t)event->y_root - (self->height / 2);
        self->private->cb_mouse_move(self, x, y);
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
        g_signal_connect(action, "activate", G_CALLBACK(cb), self);
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
                         "button-press-event",
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
                window, "motion-notify-event", G_CALLBACK(on_mouse_move), self);
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
        g_signal_connect(
                window, "context-menu", G_CALLBACK(on_context_menu_open), self);
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
window_show(window_t *self)
{
        window_set_title(self, self->title);
        window_set_size(self, self->width, self->height);
        window_set_position(self, self->x, self->y);
        window_set_hide_from_pager(self, self->is_child);
        window_set_hide_from_taskbar(self, !self->is_child);

        webkit_web_view_load_uri(self->private->webview, self->private->url);
        gtk_container_add(GTK_CONTAINER(self->private->window),
                          GTK_WIDGET(self->private->webview));

        g_signal_connect(self->private->window,
                         "destroy",
                         G_CALLBACK(destroy_window),
                         self);

        gtk_widget_show_all(self->private->window);

        if (self->is_child)
        {
                GtkWindow *window = GTK_WINDOW(self->private->window);
                gtk_window_set_decorated(window, FALSE);
                gtk_window_set_transient_for(
                        window,
                        GTK_WINDOW(self->private->parent->private->window));
        }
        else
        {
                gtk_main();
        }
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
}

void
window_set_topmost(window_t *self, bool state)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_set_keep_above(window, state);
        gtk_window_present(window);

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
window_set_state(window_t *self, ww_window_state_t state_flag)
{
        self->private->state = self->private->state | state_flag;
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
                         "destroy",
                         G_CALLBACK(on_window_destroy),
                         self);
        gtk_window_close(GTK_WINDOW(window));
}

void
window_set_url(window_t *self, string url)
{
        if (self->private->url != nullptr)
        {
                free(self->private->url);
                self->private->url = nullptr;
        }

        string temp_url = (string)malloc(sizeof(char) * (MAX_STR_SIZE + 1));
        if (temp_url == nullptr)
        {
                fprintf(stderr, "Memory allocation for temporary url failed\n");
                exit(1);
        }

        const size_t url_size = strlen(url);
        if (url_size >= MAX_STR_SIZE)
        {
                free(temp_url);
                fprintf(stderr,
                        "URL size is greater than allowed by the heap memory "
                        "region\n");
                exit(1);
        }

        strncpy(temp_url, url, url_size);
        temp_url[url_size] = '\0';

        self->private->url = temp_url;
}

window_t *
window_new(window_t options)
{
        window_t *window = nullptr;
        window_private_t *private = nullptr;
        string title = nullptr;

        if ((window = (window_t *)malloc(sizeof(window_t))) == nullptr)
        {
                fprintf(stderr, "Memory allocation for new window failed\n");
                goto cleanup;
        }

        if ((private = (window_private_t *)malloc(sizeof(window_private_t))) ==
            nullptr)
        {
                fprintf(stderr, "Memory allocation for private failed\n");
                goto cleanup;
        }

        if ((title = (string)malloc(sizeof(char) * (MAX_STR_SIZE + 1))) ==
            nullptr)
        {
                fprintf(stderr, "Memory allocation for title failed\n");
                goto cleanup;
        }

        const size_t title_size = strlen(options.title);
        if (title_size >= MAX_STR_SIZE)
        {
                fprintf(stderr, "Title size must be <= %d\n", MAX_STR_SIZE);
                goto cleanup;
        }

        *window = options;

        strncpy(title, options.title, title_size);
        title[title_size] = '\0';

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
        private->window = gtk_window;
        private->webview = webview;
        private->manager = manager;
        private->url = nullptr;
        private->next = nullptr;
        private->parent = nullptr;
        private->state = WINDOW_STATE_NONE;

        window->title = title;
        window->guid = window_generate_id();
        window->private = private;

        return window;

cleanup:
        if (title != nullptr)
        {
                free(title);
        }

        if (private != nullptr)
        {
                free(private);
        }

        if (window != nullptr)
        {
                free(window);
        }

        exit(1);
}
