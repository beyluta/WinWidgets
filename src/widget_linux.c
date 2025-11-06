#include "widget.h"
#include "filesystem.h"
#include "gdk/gdk.h"
#include "global.h"
#include "utils.h"

#include <cairo/cairo.h>
#include <gtk/gtk.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <webkit2/webkit2.h>

constexpr int TICK_DELAY = 1;          // Seconds between event loop updates
constexpr int CLICK_RIGHT = 3;         // Code for right-clicking
constexpr char CFG_SUFFIX[] = ".cfg";  // Prefix for default children widgets
constexpr char CFG_MAIN[] = "app.cfg"; // Name of the default config file

// Global variables
static size_t open_widgets = 1;                 // Number of opened widgets
static size_t closed_widgets[MAX_WIDGETS] = {}; // Number of closed widgets

// Forward declaration of functions
static bool
create_widget(ww_window_ctx *context, ww_widget_ctx *widgets);
static bool
get_widget_config(const char *filename, ww_window_ctx *dest);

/**
 * @brief Applies the main config of the application. Used for opening widgets
 * that were not closed between sessions.
 * @param widgets Pointer to an array of widgets
 */
static bool
apply_main_config(ww_widget_ctx *widgets)
{
        char default_dir[BUFFSIZE];
        if (ww_default_widgets_dir(default_dir) == true)
        {
                fprintf(stderr, "Failed to get default widgets dir\n");
                return true;
        }

        char app_cfg[BUFFSIZE];
        const size_t len = strlen(CFG_MAIN) + strlen(default_dir) +
                           2; // +2 for '\0' and '/'
        if (snprintf(app_cfg, len, "%s/%s", default_dir, CFG_MAIN) < 0)
        {
                fprintf(stderr, "Failed to construct app config path\n");
                return true;
        }
        app_cfg[len] = '\0';

        if (access(app_cfg, F_OK) != 0)
        {
                fprintf(stderr,
                        "Failed to apply config; %s does not exist\n",
                        app_cfg);
                return true;
        }

        char content[EXTBUFFSIZE];
        if (ww_get_file_content(app_cfg, content, EXTBUFFSIZE) == true)
        {
                fprintf(stderr,
                        "Failed to get content of the main app "
                        "configuration.\n");
                return true;
        }

        const size_t content_len = strlen(content);
        size_t start = 0;
        char filename[BUFFSIZE];
        for (size_t i = 0; i < content_len; i++)
        {
                if (content[i] == '\n')
                {
                        const size_t filename_len = i - start;
                        strncpy(filename, &content[start], filename_len);
                        filename[filename_len] = '\0';

                        ww_window_ctx window = widgets->window_context;
                        if (get_widget_config(filename, &window) == true)
                        {
                                fprintf(stderr,
                                        "Could not load existing "
                                        "configuration\n");
                                return true;
                        }

                        if (create_widget(&window, widgets) == true)
                        {
                                fprintf(stderr,
                                        "Failed to open previously opened "
                                        "Widget: %s\n",
                                        filename);
                                return true;
                        }

                        start = i + 1;
                }
        }

        return false;
}

/**
 * @brief Saves the main config of the application with information about the
 * currently opened widgets.
 * @param widgets Pointer to an array of widgets
 */
static bool
save_main_config(const ww_widget_ctx *widgets)
{
        char default_dir[BUFFSIZE];
        if (ww_default_widgets_dir(default_dir) == true)
        {
                fprintf(stderr, "Failed to get default widgets dir\n");
                return true;
        }

        char app_cfg[BUFFSIZE];
        const size_t len = strlen(CFG_MAIN) + strlen(default_dir) +
                           2; // +2 for '\0' and '/'
        if (snprintf(app_cfg, len, "%s/%s", default_dir, CFG_MAIN) < 0)
        {
                fprintf(stderr, "Failed to construct app config path\n");
                return true;
        }
        app_cfg[len] = '\0';

        if (ww_write_to_file(app_cfg, "", WRITE_OVERWRITE) == true)
        {
                fprintf(stderr, "Failed to clear the contents of the file\n");
                return true;
        }

        for (size_t i = 1; i < open_widgets; i++)
        {
                if (closed_widgets[i] != 0)
                {
                        continue; // Skipping closed widgets
                }

                const ww_window_ctx window = widgets[i].window_context;
                const char *filename = window.filename;
                const size_t len = strlen(filename) + 2; // +2 for \n and \0
                char concatenated_str[BUFFSIZE];
                if (snprintf(concatenated_str, len, "%s\n", filename) < 0)
                {
                        fprintf(stderr,
                                "Failed to concatenate title and filename\n");
                        return true;
                }
                concatenated_str[len] = '\0';

                if (ww_write_to_file(app_cfg, concatenated_str, WRITE_APPEND) ==
                    true)
                {
                        fprintf(stderr, "Failed to append filename to file\n");
                        return true;
                }
        }
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
 * @brief Sets the opacity of the current Gtk window
 * @param window Pointer to the GtkWidget
 * @param opacity Opacity value of the window 0 - 1
 */
static void
set_window_opacity(GtkWidget *window, const double opacity)
{
        gtk_widget_set_opacity(window, opacity);
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
        // Building the javascript command to execute
        const size_t len = (2 * strlen(func)) + strlen(arg) + 21;
        char command[len];
        snprintf(command,
                 len + 1,
                 "window.%s && window.%s(%s);",
                 func,
                 func,
                 arg);
        command[len] = '\0';

        // Running the command
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
        // Getting the default app directory
        char app_dir[BUFFSIZE];
        if (ww_default_widgets_dir(app_dir) == true)
        {
                fprintf(stderr, "Failed to get default app directory\n");
                return;
        }

        // Getting all filenames from the directory
        char widgets[MAX_WIDGETS][BUFFSIZE];
        const size_t count =
                ww_get_files_from_dir(app_dir, widgets, MAX_WIDGETS);
        if (count <= 0)
        {
                fprintf(stderr, "Failed to read directory %s\n", app_dir);
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
                fprintf(stderr,
                        "Failed to remove the prefix from the filename: %s\n",
                        filename);
                return true;
        }
        dest[len] = '\0';
        return false;
}

/**
 * @brief Get a target value from the provided config file
 * @param src Complete config file content
 * @param target Target attribute to get the value from
 * @param dest String to save the value to
 */
static bool
get_widget_config_attribute(const char *src, const char *target, char *dest)
{
        const size_t len = strlen(src);
        const size_t target_len = strlen(target);
        bool value_found = true;
        int start = -1, end = -1;
        for (size_t i = 0, k = 0; i < len; i++)
        {
                if (src[i] == target[k] && value_found == true)
                {
                        k++;
                        if (k >= target_len)
                        {
                                value_found = false;
                        }
                        continue;
                }

                if (value_found == false)
                {
                        if (src[i] == '=' && start < 0 && i + 1 < len)
                        {
                                start = i + 1;
                        }
                        else if (src[i] == '\n' && end < 0)
                        {
                                end = i;
                        }

                        if (start > 0 && end > 0)
                        {
                                const size_t size = end - start;
                                if (strncpy(dest, &src[start], size) == NULL)
                                {
                                        fprintf(stderr,
                                                "Failed to get value of "
                                                "attribute %s\n",
                                                target);
                                        return true;
                                }
                                dest[size] = '\0';
                                return false;
                        }
                        continue;
                }

                k = 0;
        }
        return true;
}

/**
 * @brief Gets the configuration file for a widget
 * @param filename Absolute path to the widget file
 * @param dest Widget window context that the props will be saved to
 * @returns Status of the operation
 */
static bool
get_widget_config(const char *filename, ww_window_ctx *dest)
{
        char root_dir[BUFFSIZE];
        if (ww_get_root_dir(filename, root_dir) == true)
        {
                fprintf(stderr,
                        "Could not get root directory of file %s\n",
                        filename);
                return true;
        }

        char file[BUFFSIZE];
        if (ww_get_filename_from_absolute_path(filename, file) == true)
        {
                fprintf(stderr, "Could not get file name\n");
                return true;
        }

        char config_path[BUFFSIZE];
        const size_t len = strlen(root_dir) + strlen(file) + strlen(CFG_SUFFIX);
        if (snprintf(config_path,
                     len + 1,
                     "%s%s%s",
                     root_dir,
                     file,
                     CFG_SUFFIX) < 0)
        {
                fprintf(stderr, "Could not get the config file path\n");
                return true;
        }
        config_path[len] = '\0';

        char config_path_no_prefix[BUFFSIZE];
        if (remove_file_prefix(config_path, config_path_no_prefix) == true)
        {
                fprintf(stderr,
                        "Could not remove prefix from file %s\n",
                        config_path);
                return true;
        }

        char config_content[BUFFSIZE];
        memset(config_content, 0, BUFFSIZE);
        if (ww_get_file_content(
                    config_path_no_prefix, config_content, BUFFSIZE) == true)
        {
                fprintf(stderr,
                        "Could not get the content from the config file: %s\n",
                        config_path);
        }

        // Getting the properties from the config files
        // ---------------------------------------------------
        char title[BUFFSIZE];
        if (get_widget_config_attribute(config_content, "title", title) == true)
        {
                strncpy(title, "Child", sizeof(title) - 1);
                title[sizeof(title) - 1] = '\0';
        }

        char width[BUFFSIZE];
        if (get_widget_config_attribute(config_content, "width", width) == true)
        {
                strncpy(width, "800", sizeof(width) - 1);
                width[sizeof(width) - 1] = '\0';
        }

        char height[BUFFSIZE];
        if (get_widget_config_attribute(config_content, "height", height) ==
            true)
        {
                strncpy(height, "600", sizeof(height) - 1);
                height[sizeof(height) - 1] = '\0';
        }

        char x[BUFFSIZE];
        if (get_widget_config_attribute(config_content, "x", x) == true)
        {
                strncpy(x, "0", sizeof(x) - 1);
                x[sizeof(x) - 1] = '\0';
        }

        char y[BUFFSIZE];
        if (get_widget_config_attribute(config_content, "y", y) == true)
        {
                strncpy(y, "0", sizeof(y) - 1);
                y[sizeof(y) - 1] = '\0';
        }

        char title_bar[BUFFSIZE];
        if (get_widget_config_attribute(
                    config_content, "title_bar", title_bar) == true)
        {
                strncpy(title_bar, "true", sizeof(title_bar) - 1);
                title_bar[sizeof(title_bar) - 1] = '\0';
        }

        char top_most[BUFFSIZE];
        if (get_widget_config_attribute(config_content, "top_most", top_most) ==
            true)
        {
                strncpy(top_most, "false", sizeof(top_most) - 1);
                top_most[sizeof(top_most) - 1] = '\0';
        }

        char opacity[BUFFSIZE];
        if (get_widget_config_attribute(config_content, "opacity", opacity) ==
            true)
        {
                strncpy(opacity, "1", sizeof(opacity) - 1);
                opacity[sizeof(opacity) - 1] = '\0';
        }

        char radius[BUFFSIZE];
        if (get_widget_config_attribute(config_content, "radius", radius) ==
            true)
        {
                strncpy(radius, "0", sizeof(radius) - 1);
                radius[sizeof(radius) - 1] = '\0';
        }

        // Copying primitive values
        dest->width = (size_t)strtol(width, NULL, 10);
        dest->height = (size_t)strtol(height, NULL, 10);
        dest->x = (size_t)strtol(x, NULL, 10);
        dest->y = (size_t)strtol(y, NULL, 10);
        dest->title_bar = (strcmp(title_bar, "true") == 0) ? true : false;
        dest->child = false;
        dest->top_most = (strcmp(top_most, "true") == 0) ? false : true;
        dest->opacity = (double)strtod(opacity, NULL);
        dest->radius = (double)strtod(radius, NULL);

        // Copying the memories of filename and title
        const size_t title_len = strlen(title);
        if (title_len >= BUFFSIZE)
        {
                fprintf(stderr, "Length of title was bigger than expected\n");
                return true;
        }
        memcpy(dest->title, title, title_len);
        dest->title[title_len] = '\0';

        const size_t filename_len = strlen(filename);
        if (filename_len >= BUFFSIZE)
        {
                fprintf(stderr,
                        "Length of filename was bigger than expected\n");
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
save_widget_config(const ww_window_ctx *window)
{
        char filename_without_prefix[BUFFSIZE];
        if (remove_file_prefix(window->filename, filename_without_prefix) ==
            true)
        {
                fprintf(stderr,
                        "Could not remove file prefix from: %s\n",
                        window->filename);
                return true;
        }

        char filename_with_suffix[BUFFSIZE];
        const size_t len = strlen(filename_without_prefix) + strlen(CFG_SUFFIX);
        if (snprintf(filename_with_suffix,
                     len + 1,
                     "%s%s",
                     filename_without_prefix,
                     CFG_SUFFIX) < 0)
        {

                fprintf(stderr,
                        "Failed to add the configuration suffix to %s\n",
                        filename_with_suffix);
                return true;
        }
        filename_with_suffix[len] = '\0';

        char content[EXTBUFFSIZE];
        const ssize_t size =
                snprintf(content,
                         EXTBUFFSIZE,
                         "title=%s\n"
                         "width=%zu\n"
                         "height=%zu\n"
                         "x=%zu\n"
                         "y=%zu\n"
                         "title_bar=%s\n"
                         "top_most=%s\n"
                         "opacity=%f\n"
                         "radius=%f\n",
                         window->title,
                         window->width,
                         window->height,
                         window->x,
                         window->y,
                         window->title_bar == true ? "true" : "false",
                         window->top_most == false ? "true" : "false",
                         window->opacity,
                         window->radius);
        if (size < 0)
        {
                fprintf(stderr,
                        "Failed to construct content of the save file\n");
                return true;
        }

        if (ww_write_to_file(filename_with_suffix, content, WRITE_OVERWRITE) ==
            true)
        {
                fprintf(stderr, "Failed to write content to file\n");
                return true;
        }

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
        closed_widgets[index] = 1; // Set widget as closed
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

        // Saving the state of all opened widgets before closing
        save_main_config(destroy_obj->widgets);

        // Stopping the event loop
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
        // Bounds of the gtk window
        GtkAllocation allocation;
        gtk_widget_get_allocation(widget, &allocation);

        // Initializing cairo objects
        cairo_surface_t *shape_surface = cairo_image_surface_create(
                CAIRO_FORMAT_A1, allocation.width, allocation.height);
        if (shape_surface == NULL)
        {
                fprintf(stderr, "Failed to initialize a cairo surface\n");
                return;
        }

        cairo_t *shape_cr = cairo_create(shape_surface);
        if (shape_cr == NULL)
        {
                cairo_surface_destroy(shape_surface);
                fprintf(stderr, "Failed to create the surface of the shape\n");
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
        cairo_region_t *shape_region =
                gdk_cairo_region_create_from_surface(shape_surface);
        if (shape_region == NULL)
        {
                cairo_destroy(shape_cr);
                cairo_surface_destroy(shape_surface);
                fprintf(stderr, "Failed to create a shape region\n");
                return;
        }
        gtk_widget_shape_combine_region(widget, shape_region);

        // Freeing memory
        cairo_destroy(shape_cr);
        cairo_surface_destroy(shape_surface);
        cairo_region_destroy(shape_region);
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
        set_window_opacity(window, context->opacity);
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
        if (event->type == GDK_BUTTON_PRESS && event->button == CLICK_RIGHT)
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
        // Checking if we can create more widgets
        if (open_widgets >= MAX_WIDGETS)
        {
                fprintf(stderr,
                        "Failed to add widget to list. Widget limit "
                        "exceeded.\n");
                return true;
        }

        // Setting GTK window options
        GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        if (set_window_style(context, window) == true)
        {
                fprintf(stderr, "Failed to set window default style\n");
                return true;
        }

        // Creating the WebKit browser
        WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());

        ww_widget_ctx *widget_context = &widgets[open_widgets];
        widget_context->window = window;

        // Copying the data from the context into the widget entry
        widget_context->window_context.width = context->width;
        widget_context->window_context.height = context->height;
        widget_context->window_context.x = context->x;
        widget_context->window_context.y = context->y;
        widget_context->window_context.title_bar = context->title_bar;
        widget_context->window_context.child = context->child;
        widget_context->window_context.top_most = context->top_most;
        widget_context->window_context.opacity = context->opacity;
        widget_context->window_context.radius = context->radius;

        // Copying pointers into the widget entry
        strncpy(widget_context->window_context.title,
                context->title,
                BUFFSIZE - 1);
        widget_context->window_context.title[BUFFSIZE - 1] = '\0';
        strncpy(widget_context->window_context.filename,
                context->filename,
                BUFFSIZE - 1);
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

        // Configuring the WebKit Browser
        webkit_web_view_load_uri(webview, context->filename);
        gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
        gtk_widget_show_all(window);
        open_widgets++;

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
        // Get the value passed from the JavaScript
        JSCValue *value = webkit_javascript_result_get_js_value(result);
        if (jsc_value_is_string(value) <= 0)
        {
                fprintf(stderr,
                        "Failed to open widget by filename. Argument type was "
                        "incorrect.\n");
                return;
        }

        // Making a copy of JSC value that we can use without worries
        char filename[BUFFSIZE];
        void *jsc_value = jsc_value_to_string(value);
        const size_t len = strlen(jsc_value);
        if (len >= BUFFSIZE)
        {
                fprintf(stderr,
                        "Value of JavaScript value was bigger than expected\n");
                return;
        }
        memcpy(filename, jsc_value, len);
        filename[len] = '\0';
        g_free(jsc_value);

        // Getting the configuration into a struct
        ww_widget_ctx *widget = &widgets[open_widgets];
        ww_window_ctx window = widget->window_context;
        if (get_widget_config(filename, &window) == true)
        {
                fprintf(stderr, "Could not load existing configuration\n");
        }

        create_widget(&window, widgets);
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
        if (ww_default_widgets_dir(dir) == true)
        {
                fprintf(stderr,
                        "Failed to create the default widgets directory\n");
                return;
        }

        if (ww_open_folder(dir) == true)
        {
                fprintf(stderr, "Failed to open folder to directory %s\n", dir);
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
        size_t next_tick = time(NULL) + TICK_DELAY;
        while (*running == false)
        {
                gtk_main_iteration_do(TRUE);

                const size_t now = time(NULL);
                if (now < next_tick)
                {
                        continue; // Skipping if it isn't time to update
                }
                next_tick = now + TICK_DELAY;

                for (size_t i = 1; i < open_widgets; i++)
                {
                        if (closed_widgets[i] != 0)
                        {
                                continue; // Skipping closed widgets or if time
                                          // hasn't reached
                        }

                        // Getting the context for window and widget
                        const ww_widget_ctx *child = &widgets[i];
                        const GtkWidget *gtk_window =
                                (GtkWidget *)child->window;
                        const ww_window_ctx window = child->window_context;

                        // Values that need to be updated in the config file
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

                        if (save_widget_config(&window) == true)
                        {
                                fprintf(stderr,
                                        "Failed to save new widget position "
                                        "and scale\n");
                        }
                }
        }
        return false;
}

bool
ww_init_main(ww_window_ctx *context, ww_widget_ctx *widgets)
{
        // Initializing GTK
        gtk_init(NULL, NULL);

        // Setting GTK window options
        GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        if (set_window_style(context, window) == true)
        {
                fprintf(stderr, "Failed to set window default style\n");
                return true;
        }

        // Creating the WebKit browser
        WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());

        // Registering the script handler for the different JS functions
        WebKitUserContentManager *manager =
                webkit_web_view_get_user_content_manager(webview);
        webkit_user_content_manager_register_script_message_handler(
                manager, "on_get_widget_filenames");
        webkit_user_content_manager_register_script_message_handler(
                manager, "on_open_widget_by_filename");
        webkit_user_content_manager_register_script_message_handler(
                manager, "on_open_default_directory");

        // Connecting the events to their respective functions
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

        // Configuring the WebKit Browser
        webkit_web_view_load_uri(webview, context->filename);
        gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
        gtk_widget_show_all(window);

        // Load the main app configuration
        if (apply_main_config(widgets) == true)
        {
                fprintf(stderr, "Failed to apply main app configuration\n");
        }

        // Event loop
        if (event_loop(widgets, &running) == true)
        {
                fprintf(stderr, "Could not start start event loop\n");
                return true;
        }

        return false;
}
