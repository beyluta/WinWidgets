#include "widget.h"
#include "filesystem.h"
#include "gdk/gdk.h"
#include "glib-object.h"
#include "global.h"
#include "utils.h"

#include <cairo/cairo.h>
#include <gtk/gtk.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <webkit2/webkit2.h>

static constexpr char LBL_EVT_GET_WIDGETS[] = "on_get_widget_filenames";
static constexpr char LBL_EVT_OPEN_WIDGET[] = "on_open_widget_by_filename";
static constexpr char LBL_EVT_OPEN_DIR[] = "on_open_default_directory";

static constexpr uint8_t EVT_LOOP_TICK_DELAY = 1;
static constexpr uint8_t MOUSE_CLICK_RIGHT = 3;

static size_t g_widgetCount = 1;
static size_t g_closedWidgets[MAX_WIDGETS] = {};

static bool
create_widget(ww_window_ctx *context, ww_widget_ctx *widgets);

static bool
get_widget_config(const char *filename,
                  char *const content,
                  const size_t contentSize,
                  ww_window_ctx *dest);

/**
 * @brief Applies the main config of the application. Used for opening widgets
 * that were not closed between sessions.
 * @param widgets Pointer to an array of widgets
 */
static bool
apply_main_config(ww_widget_ctx *)
{
        return false;
}

/**
 * @brief Saves the main config of the application with information about the
 * currently opened widgets.
 * @param widgets Pointer to an array of widgets
 */
static bool
save_main_config(const ww_widget_ctx *)
{
        return false;
}

/**
 * @brief Sets the window to use an RGBA visual if available, enabling
 * transparency and compositing effects. If the screen supports compositing and
 * an RGBA visual is available, it sets the window's visual accordingly. Also
 * marks the window as app-paintable to allow custom drawing with transparency.
 * @param window GtkWidget pointer to the window to configure
 */
static void
set_rgba_visuals(GtkWidget *window)
{
        GdkScreen *screen = gtk_widget_get_screen(window);
        GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
        if (visual != NULL && gdk_screen_is_composited(screen))
        {
                gtk_widget_set_visual(window, visual);
        }
        gtk_widget_set_app_paintable(window, TRUE);
}

/**
 * @brief Calls a JavaScript function
 * @param func Name of the function to call
 * @param arg arguments of the function
 * @param user_data Pointer to the WebKitWebView
 */
static void
call_js_function(const char *func, const char *arg, const gpointer user_data)
{
        const size_t len = (2 * strlen(func)) + strlen(arg) + 21;
        char command[len];
        snprintf(command,
                 len + 1,
                 "window.%s && window.%s(%s);",
                 func,
                 func,
                 arg);

        WebKitWebView *webview = WEBKIT_WEB_VIEW(user_data);
        webkit_web_view_evaluate_javascript(
                webview, command, -1, NULL, NULL, NULL, NULL, NULL);
}

/**
 * @brief Sends the path of all files to the JavaScript as an array
 * @param manager WebKit content manager
 * @param result Javascript response
 * @param user_data Pointer to the WebKitWebView
 */
static void
on_get_widget_filenames(WebKitUserContentManager *,
                        WebKitJavascriptResult *,
                        gpointer user_data)
{
        char app_dir[BUFFSIZE];
        if (ww_default_widgets_dir(app_dir) == true)
        {
                return;
        }

        char widgets[MAX_WIDGETS][BUFFSIZE];
        const size_t count =
                ww_get_files_from_dir(app_dir, widgets, MAX_WIDGETS);
        if (count <= 0)
        {
                return;
        }

        ssize_t bytes = 0;
        const uint8_t offset = 3;
        const ssize_t maxLength = (MAX_WIDGETS * BUFFSIZE) +
                                  (MAX_WIDGETS * (BUFFSIZE + offset)) -
                                  (MAX_WIDGETS * BUFFSIZE);

        char files[maxLength];
        for (size_t i = 0; i < count; i++)
        {
                const size_t filenameLen = strlen(widgets[i]);
                if (filenameLen >= BUFFSIZE - offset)
                {
                        return;
                }

                char content[USHRT_MAX];
                if (ww_get_file_content(widgets[i], content, USHRT_MAX))
                {
                        return;
                }

                char appTitle[BUFFSIZE];
                if (!GetMetaTagValue(content, TAG_APP_NAME, appTitle, BUFFSIZE))
                {
                        return;
                }

                bytes += snprintf(&files[bytes],
                                  BUFFSIZE,
                                  "{\"title\":\"%s\",\"path\":\"%s\"},",
                                  appTitle,
                                  widgets[i]);
        }
        files[bytes - 1] = '\0';

        if (bytes >= maxLength)
        {
                return;
        }

        const char format[] = "[%s]";
        char command[maxLength + (sizeof(format) / sizeof(format[0]))];
        if ((bytes = snprintf(command, maxLength, format, files)) < 0)
        {
                return;
        }

        // Calling the function from JavaScript
        call_js_function("addWidgets", command, user_data);
}

/**
 * @brief Removes the file:// prefix from the string
 * @param filename File with the prefix attached
 * @param dest String to save the new string to
 * @returns Status of the operation
 */
static bool
remove_file_prefix(const char *filename, char *dest)
{
        const size_t len = strlen(filename) - 7;
        if (strncpy(dest, &filename[7], len) == NULL)
        {
                return true;
        }
        dest[len] = '\0';
        return false;
}

/**
 * @brief Gets the configuration file for a widget
 * @param filename Absolute path to the widget file
 * @param dest Widget window context that the props will be saved to
 * @returns Status of the operation
 */
static bool
get_widget_config(const char *filename,
                  char *const content,
                  const size_t contentSize,
                  ww_window_ctx *dest)
{
        char root_dir[BUFFSIZE];
        if (ww_get_root_dir(filename, root_dir) == true)
        {
                return true;
        }

        char absolutePath[BUFFSIZE];
        if (remove_file_prefix(filename, absolutePath))
        {
                return true;
        }

        if (ww_get_file_content(absolutePath, content, contentSize))
        {
                return true;
        }

        char buf[BUFFSIZE] = {};
        const size_t bufLen = lengthof(buf);
        if (GetMetaTagValue(content, TAG_APP_NAME, buf, bufLen))
        {
                memcpy(dest->title, buf, bufLen);
                dest->title[bufLen - 1] = '\0';
        }

        if (GetMetaTagValue(content, TAG_WIN_SIZE, buf, bufLen))
        {
                size_t width, height;
                const bool isSet = Get2DValue(buf, &width, &height);
                dest->width = isSet ? width : DEF_WIDTH;
                dest->height = isSet ? height : DEF_WIDTH;
        }

        if (GetMetaTagValue(content, TAG_WIN_LOCATION, buf, bufLen))
        {
                size_t xPos, yPos;
                const bool isSet = Get2DValue(buf, &xPos, &yPos);
                dest->x = isSet ? xPos : DEF_X;
                dest->y = isSet ? yPos : DEF_Y;
        }

        if (GetMetaTagValue(content, TAG_APP_TOPMOST, buf, bufLen))
        {
                dest->top_most = strcmp(buf, "true") == 0;
        }

        if (GetMetaTagValue(content, TAG_WIN_BORD_RAD, buf, bufLen))
        {
                dest->radius = strtod(buf, nullptr);
        }

        const size_t filename_len = strlen(filename);
        if (filename_len >= BUFFSIZE)
        {
                return true;
        }
        memcpy(dest->filename, filename, filename_len);
        dest->filename[filename_len] = '\0';

        return false;
}

/**
 * @brief Saves the window context to the config file
 * @param window Window context object
 * @returns Status of the operation
 */
static bool
save_widget_config(const ww_window_ctx *)
{
        return false;
}

/**
 * Object that stores a pointer to the array of all widgets along with the index
 * of which widget to destroy. This is only used when widgets need to be
 * destroyed.
 */
typedef struct widget_destroy_obj
{
        ww_widget_ctx *widgets; // All widgets
        size_t index;           // Index of this specific widget
        bool *running;          // Only the main window has this
} widget_destroy_obj;

/**
 * @brief Destroy the window of a widget and set the widget as closed
 * @param widget Pointer to the index of the child widget
 * @param data Pointer to the widget context struct
 */
static gboolean
on_child_destroy(GtkWidget *, void *data)
{
        ww_widget_ctx *context = (ww_widget_ctx *)data;
        const size_t index = context->window_context.index;
        g_closedWidgets[index] = 1; // Set widget as closed
        return false;
}

/**
 * @brief Stops the event loop when the main window closes
 * @param widget Pointer to the GtkWidget
 * @param data Pointer to the widget destroy struct
 */
static gboolean
on_main_destroy(GtkWidget *, const widget_destroy_obj *destroy_data)
{
        const widget_destroy_obj *destroy_obj =
                (const widget_destroy_obj *)destroy_data;

        if (save_main_config(destroy_obj->widgets))
        {
                return false;
        }

        bool *running = (bool *)destroy_obj->running;
        *running = true;
        return false;
}

/**
 * @brief Event that is called when the Widget window is drawn
 * @param widget Pointer to the GtkWidget
 * @param cr Cairo object with information about the rendering device
 * @param context Window context with information about the widget to be drawn
 */
static void
on_window_draw(GtkWidget *widget, cairo_t *, const ww_window_ctx *context)
{
        cairo_surface_t *shape_surface = nullptr;
        cairo_t *shape_cr = nullptr;
        cairo_region_t *shape_region = nullptr;

        // Bounds of the gtk window
        GtkAllocation allocation;
        gtk_widget_get_allocation(widget, &allocation);

        // Initializing cairo objects
        if ((shape_surface = cairo_image_surface_create(
                     CAIRO_FORMAT_A1, allocation.width, allocation.height)) ==
            nullptr)
        {
                return;
        }

        if ((shape_cr = cairo_create(shape_surface)) == nullptr)
        {
                return;
        }

        // Setting the properties of the shape
        cairo_set_operator(shape_cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(shape_cr, 0, 0, 0, 0);
        cairo_paint(shape_cr);

        // Setting the properties of the inner shape content
        cairo_set_operator(shape_cr, CAIRO_OPERATOR_OVER);
        cairo_set_source_rgba(shape_cr, 0, 0, 0, 1);

        // Setting the roundness
        const double max_radius =
                MIN(allocation.width, allocation.height) / 2.0;
        const double border_radius = context->radius;
        const double radius =
                border_radius > max_radius ? max_radius : border_radius;
        const double x = 0, y = 0, w = allocation.width, h = allocation.height;
        cairo_new_sub_path(shape_cr);
        cairo_arc(shape_cr, x + w - radius, y + radius, radius, -G_PI / 2, 0);
        cairo_arc(
                shape_cr, x + w - radius, y + h - radius, radius, 0, G_PI / 2);
        cairo_arc(shape_cr, x + radius, y + h - radius, radius, G_PI / 2, G_PI);
        cairo_arc(shape_cr, x + radius, y + radius, radius, G_PI, 3 * G_PI / 2);
        cairo_close_path(shape_cr);
        cairo_fill(shape_cr);

        // Creating and applying the shape region
        if ((shape_region = gdk_cairo_region_create_from_surface(
                     shape_surface)) == nullptr)
        {
                goto cleanup;
        }

        gtk_widget_shape_combine_region(widget, shape_region);

cleanup:
        if (shape_cr != nullptr)
        {
                cairo_destroy(shape_cr);
        }

        if (shape_surface != nullptr)
        {
                cairo_surface_destroy(shape_surface);
        }

        if (shape_region != nullptr)
        {
                cairo_region_destroy(shape_region);
        }
        return;
}

/**
 * @brief Sets various Gtk window properties passed by window context
 * @param context Window context containing all properties
 * @param window Pointer to the GtkWidget
 */
static bool
set_window_style(ww_window_ctx *context, GtkWidget *window)
{
        gtk_window_set_title(GTK_WINDOW(window), context->title);
        gtk_window_set_default_size(
                GTK_WINDOW(window), context->width, context->height);
        gtk_window_set_decorated(GTK_WINDOW(window),
                                 context->title_bar == false);
        gtk_window_move(GTK_WINDOW(window), context->x, context->y);

        set_rgba_visuals(window); // Options below require this
        gtk_window_set_keep_above(GTK_WINDOW(window),
                                  context->top_most == false);
        return false;
}

/**
 * @brief Creates a menu item and adds it to the menu
 * @param menu Pointer to the GtkWidget
 * @param label Name of the item
 * @param callback Which function to trigger once this menu item is clicked
 * @param data Pointer to the widget context
 */
static void
create_menu_item(GtkWidget *menu, const char *label, void *callback, void *data)
{
        GtkWidget *item = gtk_menu_item_new_with_label(label);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_signal_connect(item, "activate", G_CALLBACK(callback), data);
        gtk_widget_show(item);
}

/**
 * @brief Sets the widget to the always on top
 * @param item Menu item
 * @param data Widget context struct
 */
static void
on_top_most_clicked(GtkMenuItem *, void *data)
{
        const ww_widget_ctx *ctx = (const ww_widget_ctx *)data;
        const GtkWidget *window = ctx->window;
        bool *top_most = (bool *)&ctx->window_context.top_most;
        *top_most = ctx->window_context.top_most == false ? true : false;
        gtk_window_set_keep_above(GTK_WINDOW(window), *top_most == false);
}

/**
 * @brief Closes the Window event
 * @param item Menu item
 * @param data Widget context struct
 */
static void
on_close_clicked(GtkMenuItem *, void *data)
{
        const ww_widget_ctx *ctx = (const ww_widget_ctx *)data;
        const GtkWidget *window = ctx->window;
        gtk_window_close(GTK_WINDOW(window));
}

/**
 * @brief Creates the options when right clicking on Widgets
 * @param window Pointer to the GtkWidget
 * @param event Information about the event triggered
 * @param data Pointer to the widget context
 */
static gboolean
on_button_press(GtkWidget *, GdkEventButton *event, void *data)
{
        if (event->type == GDK_BUTTON_PRESS &&
            event->button == MOUSE_CLICK_RIGHT)
        {
                GtkWidget *menu = gtk_menu_new();
                create_menu_item(menu, "Top Most", on_top_most_clicked, data);
                create_menu_item(menu, "Close Widget", on_close_clicked, data);
                gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
                return TRUE;
        }
        return FALSE;
}

/**
 * @brief Creates a single widget based on the context object provided
 * @param context Properties of the widget window
 * @param widgets Pointer to an array of widgets
 */
static bool
create_widget(ww_window_ctx *context, ww_widget_ctx *widgets)
{
        if (g_widgetCount >= MAX_WIDGETS)
        {
                return true;
        }

        GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        if (set_window_style(context, window) == true)
        {
                return true;
        }

        WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
        ww_widget_ctx *widget_context = &widgets[g_widgetCount];
        widget_context->window = window;
        widget_context->window_context.width = context->width;
        widget_context->window_context.height = context->height;
        widget_context->window_context.x = context->x;
        widget_context->window_context.y = context->y;
        widget_context->window_context.top_most = context->top_most;
        widget_context->window_context.radius = context->radius;

        memcpy(widget_context->window_context.title, context->title, BUFFSIZE);
        widget_context->window_context.title[BUFFSIZE - 1] = '\0';

        memcpy(widget_context->window_context.filename,
               context->filename,
               BUFFSIZE);
        widget_context->window_context.filename[BUFFSIZE - 1] = '\0';

        g_signal_connect(window,
                         "destroy",
                         G_CALLBACK(on_child_destroy),
                         widget_context);
        g_signal_connect(window,
                         "draw",
                         G_CALLBACK(on_window_draw),
                         (ww_window_ctx *)context);

        gtk_widget_add_events(GTK_WIDGET(webview), GDK_BUTTON_PRESS_MASK);

        g_signal_connect(webview,
                         "button-press-event",
                         G_CALLBACK(on_button_press),
                         widget_context);

        webkit_web_view_load_uri(webview, context->filename);
        gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
        gtk_widget_show_all(window);
        g_widgetCount++;

        return false;
}

/**
 * @brief JavaScript calls this function to open a new widget
 * @param manager Content manger from Webkit
 * @param result JavaScript result of the operation
 * @param user_data Pointer to an array of widgets
 */
static void
on_open_widget_by_filename(WebKitUserContentManager *,
                           WebKitJavascriptResult *result,
                           ww_widget_ctx *widgets)
{
        JSCValue *value = nullptr;
        void *jsc_value = nullptr;
        char *content = nullptr;

        if ((value = webkit_javascript_result_get_js_value(result)) == nullptr)
        {
                goto cleanup;
        }

        if (jsc_value_is_string(value) <= 0)
        {
                goto cleanup;
        }

        if ((jsc_value = jsc_value_to_string(value)) == nullptr)
        {
                goto cleanup;
        }

        const size_t len = strlen(jsc_value);
        if (len >= BUFFSIZE)
        {
                goto cleanup;
        }

        char filename[BUFFSIZE];
        memcpy(filename, jsc_value, len);
        filename[len] = '\0';

        ww_widget_ctx *widget = &widgets[g_widgetCount];
        ww_window_ctx window = widget->window_context;

        content = (char *)malloc(sizeof(char) * USHRT_MAX);
        if (get_widget_config(filename, content, USHRT_MAX, &window) == true)
        {
                goto cleanup;
        }

        create_widget(&window, widgets);

cleanup:
        if (content != nullptr)
        {
                free(content);
        }

        if (jsc_value != nullptr)
        {
                g_free(jsc_value);
        }
}

/**
 * @brief JavaScript calls this function to open the default directory
 * @param manager Content manger from Webkit
 * @param result JavaScript result of the operation
 * @param user_data Pointer to custom user data
 */
static void
on_open_default_directory(WebKitUserContentManager *,
                          WebKitJavascriptResult *,
                          void *)
{
        char dir[BUFFSIZE];
        if (ww_default_widgets_dir(dir))
        {
                return;
        }

        if (ww_open_folder(dir))
        {
                return;
        }
}

/**
 * @brief Main event loop of the application
 * @param widgets Pointer to an array of widgets
 * @param running Pointer to the curent state of the event loop
 */
static bool
event_loop(ww_widget_ctx *widgets, bool *running)
{
        size_t next_tick = time(NULL) + EVT_LOOP_TICK_DELAY;
        while (*running == false)
        {
                if (!gtk_main_iteration_do(TRUE))
                {
                        return true;
                }

                const size_t now = time(NULL);
                if (now < next_tick)
                {
                        continue; // Skipping if it isn't time to update
                }
                next_tick = now + EVT_LOOP_TICK_DELAY;

                for (size_t i = 1; i < g_widgetCount; i++)
                {
                        if (g_closedWidgets[i] != 0)
                        {
                                continue; // Skipping closed widgets or if time
                                          // hasn't reached
                        }

                        const ww_widget_ctx *child = &widgets[i];
                        const GtkWidget *gtk_window =
                                (GtkWidget *)child->window;
                        const ww_window_ctx window = child->window_context;

                        size_t x = 0, y = 0, w = 0, h = 0;
                        gtk_window_get_position(
                                GTK_WINDOW(gtk_window), (int *)&x, (int *)&y);
                        gtk_window_get_size(
                                GTK_WINDOW(gtk_window), (int *)&w, (int *)&h);
                        size_t *ptr_x = (size_t *)&window.x;
                        size_t *ptr_y = (size_t *)&window.y;
                        size_t *ptr_w = (size_t *)&window.width;
                        size_t *ptr_h = (size_t *)&window.height;
                        *ptr_x = x;
                        *ptr_y = y;
                        *ptr_w = w;
                        *ptr_h = h;

                        if (save_widget_config(&window))
                        {
                                return true;
                        }
                }
        }
        return false;
}

bool
ww_init_main(ww_window_ctx *context, ww_widget_ctx *widgets)
{
        gtk_init(NULL, NULL);

        GtkWidget *window = nullptr;
        if ((window = gtk_window_new(GTK_WINDOW_TOPLEVEL)) == nullptr)
        {
                return true;
        }

        if (set_window_style(context, window) == true)
        {
                return true;
        }

        WebKitWebView *webview = nullptr;
        if ((webview = WEBKIT_WEB_VIEW(webkit_web_view_new())) == nullptr)
        {
                return true;
        }

        WebKitUserContentManager *manager = nullptr;
        if ((manager = webkit_web_view_get_user_content_manager(webview)) ==
            nullptr)
        {
                return true;
        }

        if (!webkit_user_content_manager_register_script_message_handler(
                    manager, LBL_EVT_GET_WIDGETS))
        {
                return true;
        }

        if (!webkit_user_content_manager_register_script_message_handler(
                    manager, LBL_EVT_OPEN_WIDGET))
        {
                return true;
        }

        if (!webkit_user_content_manager_register_script_message_handler(
                    manager, LBL_EVT_OPEN_DIR))
        {
                return true;
        }

        g_signal_connect(manager,
                         "script-message-received::on_get_widget_filenames",
                         G_CALLBACK(on_get_widget_filenames),
                         webview);
        g_signal_connect(manager,
                         "script-message-received::on_open_widget_by_filename",
                         G_CALLBACK(on_open_widget_by_filename),
                         widgets);
        g_signal_connect(manager,
                         "script-message-received::on_open_default_directory",
                         G_CALLBACK(on_open_default_directory),
                         NULL);

        static bool running = false;
        widget_destroy_obj destroy_obj = {.running = &running,
                                          .widgets = widgets};
        g_signal_connect(
                window, "destroy", G_CALLBACK(on_main_destroy), &destroy_obj);

        webkit_web_view_load_uri(webview, context->filename);
        gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
        gtk_widget_show_all(window);

        if (apply_main_config(widgets))
        {
                return true;
        }

        if (event_loop(widgets, &running))
        {
                return true;
        }

        return false;
}
