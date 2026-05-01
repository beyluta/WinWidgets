// ======================= Purpose ==========================
//
// Responsible for the implementation of the window class for
// the linux platform.
//
// ==========================================================
#include "gdk/gdk.h"
#include "glib.h"
#include "widget.h"
#include "glib-object.h"
#include "gtk/gtk.h"
#include "c_window.h"
#include "webkit/WebKitUserContentManager.h"
#include <stdint.h>

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
};

static size_t
c_window_generate_guid()
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
c_window_destroy_private_members(window_private_t *private)
{
        if (private->url != nullptr)
        {
                free(private->url);
                private->url = nullptr;
        }
}

static void
c_window_destroy(window_t *self)
{
        if (self->title != nullptr)
        {
                free(self->title);
                self->title = nullptr;
        }

        if (self->private != nullptr)
        {
                c_window_destroy_private_members(self->private);
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
c_set_url(window_t *self, string url)
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
                fprintf(stderr, "URL size is greater than allowed by the heap memory region\n");
                exit(1);
        }

        strncpy(temp_url, url, url_size);
        temp_url[url_size] = '\0';

        self->private->url = temp_url;
}

static void
c_destroy_window(void *, void *data)
{
        window_t *self = (window_t *)data;

        if (self->is_child)
        {
                window_t *prev = nullptr;

                for (window_t *node = self->private->parent; node != nullptr; node = node->private->next)
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

                self->vtable->destroy(self);
        }
        else
        {
                gtk_main_quit();
        }
}

static void
c_show(window_t *self)
{
        self->vtable->set_title(self, self->title);
        self->vtable->set_size(self, self->width, self->height);
        self->vtable->set_position(self, self->x, self->y);
        self->vtable->set_hide_from_pager(self, self->is_child);
        self->vtable->set_hide_from_taskbar(self, !self->is_child);

        webkit_web_view_load_uri(self->private->webview, self->private->url);
        gtk_container_add(GTK_CONTAINER(self->private->window), GTK_WIDGET(self->private->webview));

        g_signal_connect(self->private->window, "destroy", G_CALLBACK(c_destroy_window), self);

        gtk_widget_show_all(self->private->window);

        if (self->is_child)
        {
                gtk_window_set_transient_for(GTK_WINDOW(self->private->window), GTK_WINDOW(self->private->parent->private->window));
        }
        else
        {
                gtk_main();
        }
}

static void
c_set_size(window_t *self, size_t width, size_t height)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_set_default_size(window, width, height);
}

static void
c_set_position(window_t *self, size_t x, size_t y)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_move(window, x, y);
}

static void
c_set_hide_from_taskbar(window_t *self, bool status)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_set_skip_taskbar_hint(window, status);
}

static void
c_set_hide_from_pager(window_t *self, bool status)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_set_skip_pager_hint(window, status);
}

static void
c_set_title(window_t *self, string title)
{
        GtkWindow *window = GTK_WINDOW(self->private->window);
        gtk_window_set_title(window, title);
}

static void *
c_get_webview(window_t *self)
{
        return self->private->webview;
}

static void *
c_get_manager(window_t *self)
{
        return self->private->manager;
}

static void *
c_get_window(window_t *self)
{
        return self->private->window;
}

static void
c_register_event_callback(window_t *self, void *instance, string event, void (*cb)(void *, void *, void *), void *data)
{
        const string prefix = "script-message-received::";
        const size_t totalLen = strlen(prefix) + strlen(event);
        if (totalLen >= MAX_STR_SIZE)
        {
                return;
        }

        char buffer[MAX_STR_SIZE + 1];
        snprintf(buffer, totalLen + 1, "%s%s", prefix, event);

        webkit_user_content_manager_register_script_message_handler(self->private->manager, event);

        g_signal_connect(instance, buffer, G_CALLBACK(cb), data);
}

static void
c_register_event_mouse_motion(window_t *self, void (*cb)(void *))
{
        GtkWidget *window = self->private->window;
        gtk_widget_add_events(window, GDK_POINTER_MOTION_MASK);
        g_signal_connect(window, "motion-notify-event", G_CALLBACK(cb), self);
}

static gboolean
c_on_mouse_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
        window_t *self = (window_t *)data;
        self->private->cb_mouse_button_press(self, event->button);
        return FALSE;
}

static void
c_register_event_mouse_press(window_t *self, void (*cb)(void *, const ww_window_mouse_press_event_t))
{
        GtkWidget *window = GTK_WIDGET(self->private->webview);
        gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
        self->private->cb_mouse_button_press = cb;
        g_signal_connect(window, "button-press-event", G_CALLBACK(c_on_mouse_button_press), self);
        printf("Event registered\n");
}

static void
c_run_javascript(window_t *self, string script)
{
        webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(self->private->webview),
                                            script,
                                            -1,
                                            nullptr,
                                            nullptr,
                                            nullptr,
                                            nullptr,
                                            nullptr);
}

static void
c_add_child(window_t *self, window_t *child)
{
        window_t *prev = nullptr;
        for (window_t *node = self; node != nullptr; node = node->private->next)
        {
                prev = node;
        }
        prev->private->next = child;
        child->private->parent = self;
}

static void
c_destroy_chain(window_t *self)
{
        window_t *next = nullptr;
        for (window_t *node = self; node != nullptr; node = next)
        {
                next = node->private->next;
                node->vtable->destroy(node);
        }
}

static window_vtable_t c_window_vtable = {
        .destroy = c_window_destroy,
        .set_url = c_set_url,
        .show = c_show,
        .set_size = c_set_size,
        .set_position = c_set_position,
        .set_hide_from_taskbar = c_set_hide_from_taskbar,
        .set_hide_from_pager = c_set_hide_from_pager,
        .set_title = c_set_title,
        .get_webview = c_get_webview,
        .get_manager = c_get_manager,
        .get_window = c_get_window,
        .register_event_callback = c_register_event_callback,
        .register_event_mouse_motion = c_register_event_mouse_motion,
        .register_event_mouse_press = c_register_event_mouse_press,
        .run_javascript = c_run_javascript,
        .add_child = c_add_child,
        .destroy_chain = c_destroy_chain};

// Public members

window_t *
ww_window_new(window_t options)
{
        window_t *window = (window_t *)malloc(sizeof(window_t));
        if (window == nullptr)
        {
                fprintf(stderr, "Memory allocation for new window instance failed\n");
                exit(1);
        }

        window_private_t *private = (window_private_t *)malloc(sizeof(window_private_t));
        if (private == nullptr)
        {
                free(window);
                fprintf(stderr, "Private member region of window could not be instantiated\n");
                exit(1);
        }

        string title = (string)malloc(sizeof(char) * (MAX_STR_SIZE + 1));
        if (title == nullptr)
        {
                free(private);
                free(window);
                fprintf(stderr, "Memory allocation for window title has failed\n");
                exit(1);
        }

        const size_t title_size = strlen(options.title);
        if (title_size >= MAX_STR_SIZE)
        {
                free(title);
                free(private);
                free(window);
                fprintf(stderr, "Title name is greater or lesser than allowed by the heap memory region\n");
                exit(1);
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

        uint8_t window_type = options.is_child ? GTK_WINDOW_POPUP : GTK_WINDOW_TOPLEVEL;
        GtkWidget *gtk_window = gtk_window_new(window_type);
        WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
        WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(webview);
        private->window = gtk_window;
        private->webview = webview;
        private->manager = manager;
        private->url = nullptr;
        private->next = nullptr;
        private->parent = nullptr;

        window->title = title;
        window->guid = c_window_generate_guid();
        window->private = private;
        window->vtable = &c_window_vtable;

        return window;
}
