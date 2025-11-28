// ======================= Purpose ==========================
//
// This file contains the Linux source code for the widget-
// manager and each individual widget that can be opened
// during runtime.
//
// ==========================================================
#include "widget.h"
#include "filesystem.h"
#include "global.h"
#include "utils.h"
#include "parser.h"
#include "json.h"

#include <libappindicator/app-indicator.h>

static constexpr char SIGNAL_DESTROY[] = "destroy";
static constexpr char SIGNAL_DRAW_WINDOW[] = "draw";
static constexpr char SIGNAL_ACTIVATE[] = "activate";
static constexpr char SIGNAL_CONFIGURE[] = "configure-event";
static constexpr char SIGNAL_BUTTON_PRESS[] = "button-press-event";

static constexpr char SIGNAL_GET_FILENAMES[] =
        "script-message-received::on_get_widget_filenames";
static constexpr char SIGNAL_OPEN_WIDGET[] =
        "script-message-received::on_open_widget_by_filename";
static constexpr char SIGNAL_OPEN_DIRECTORY[] =
        "script-message-received::on_open_default_directory";

static constexpr char LBL_EVT_GET_WIDGETS[] = "on_get_widget_filenames";
static constexpr char LBL_EVT_OPEN_WIDGET[] = "on_open_widget_by_filename";
static constexpr char LBL_EVT_OPEN_DIR[] = "on_open_default_directory";

static constexpr uint8_t MOUSE_CLICK_RIGHT = 3;

typedef struct
{
        const size_t width;
        const size_t height;
        const size_t x;
        const size_t y;
        char title[BUFFSIZE];
        const bool hideTitleBar;
        const bool topMost;
} window_style_t;

typedef struct
{
        AppIndicator *indicator;
        GtkWidget *menu;
        GtkWidget *exitBtn;
} system_tray_t;

static application_settings_t g_settings;
static ww_widget_ctx g_widgets[MAX_WIDGETS];
static system_tray_t g_tray = {.exitBtn = nullptr,
                               .indicator = nullptr,
                               .menu = nullptr};
static size_t g_widgetCount = 1;

/**
 * @brief Saves the main config of the application with information about the
 * currently opened widgets.
 * @returns true if successful, else false on failure
 */
static bool
save_main_config()
{
        if (g_widgetCount < 2)
        {
                return false;
        }

        size_t bytesWritten = 0;
        char session[JSONBUFFSIZE] = {};

        for (size_t i = 1; i < g_widgetCount; i++)
        {
                const ww_widget_ctx widget = g_widgets[i];
                const ww_window_ctx window = widget.window_context;
                const size_t pathLength = strlen(window.filename);

                if (pathLength >= BUFFSIZE)
                {
                        return false;
                }

                gint x, y;
                gtk_window_get_position(GTK_WINDOW(widget.window), &x, &y);

                if ((bytesWritten +=
                     snprintf(&session[bytesWritten],
                              lengthof(session),
                              "{\"path\":\"%s\",\"position\":\"%d, "
                              "%d\",\"alwaysOnTop\":%s},",
                              window.filename,
                              x,
                              y,
                              window.top_most ? "true" : "false")) >=
                    lengthof(session))
                {
                        return false;
                }
        }
        session[bytesWritten - 1] = '\0';

        char json[USHRT_MAX];
        if (snprintf(json,
                     lengthof(json),
                     "{\"version\":\"%s\",\"isWidgetAutostartEnabled\":%s,"
                     "\"isWidgetFullscreenHideEnabled\":%s,"
                     "\"isOpenAppOnStartupEnabled\":%s,"
                     "\"lastSessionWidgets\":[%s]}",
                     PROG_SEM_VER,
                     g_settings.widgetAutostart ? "true" : "false",
                     g_settings.fullscreenHide ? "true" : "false",
                     g_settings.appAutostart ? "true" : "false",
                     session) < 0)
        {
                return false;
        }

        char directory[BUFFSIZE];
        if (ww_default_widgets_dir(directory))
        {
                return false;
        }

        const size_t directoryLen = strlen(directory);
        const size_t cfgLen = strlen(PROG_CFG_NAME);
        strncat(directory, PROG_CFG_NAME, directoryLen + cfgLen);

        if (ww_write_to_file(directory, json, WRITE_OVERWRITE))
        {
                return false;
        }

        return true;
}

/**
 * @brief Calls a JavaScript function. First makes sure that the function is
 * defined before calling it.
 *
 * @param func Name of the function to call
 * @param arg arguments of the function
 * @param user_data Pointer to the WebKitWebView
 */
static void
call_js_function(const char *const func,
                 const char *const arg,
                 const gpointer user_data)
{
        char command[BUFFSIZE];
        const char format[] = "window.%s && window.%s(%s);";
        const size_t totalLen =
                strlen(func) * 2 + strlen(arg) + lengthof(format);
        if (totalLen >= lengthof(command))
        {
                return;
        }

        if (snprintf(command, totalLen, format, func, func, arg) < 0)
        {
                return;
        }

        webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(user_data),
                                            command,
                                            -1,
                                            nullptr,
                                            nullptr,
                                            nullptr,
                                            nullptr,
                                            nullptr);
}

/**
 * @brief Triggered when a gtk widget is resized or moved. This function is
 * currently being used to save the state of the application to the
 * configuration file.
 */
static void
on_window_configure(const GtkWindow *const)
{
        save_main_config();
}

/**
 * @brief Sends the path of all files to the JavaScript as an array
 * @param manager WebKit content manager
 * @param result Javascript response
 * @param user_data Pointer to the WebKitWebView
 */
static void
on_get_widget_filenames(WebKitUserContentManager *const,
                        WebKitJavascriptResult *const,
                        const gpointer user_data)
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
                        continue;
                }

                char content[USHRT_MAX];
                if (ww_get_file_content(widgets[i], content, USHRT_MAX))
                {
                        continue;
                }

                char appTitle[BUFFSIZE];
                if (!ww_begin_parsing(content,
                                      lengthof(content),
                                      TAG_APP_NAME,
                                      appTitle,
                                      lengthof(appTitle)))
                {
                        continue;
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

        call_js_function("addWidgets", command, user_data);
}

/**
 * @brief Removes the file:// prefix from the string
 * @param filename File with the prefix attached
 * @param dest String to save the new string to
 * @returns Status of the operation
 */
static bool
remove_file_prefix(const char *const filename, char *const dest)
{
        const size_t len = strlen(filename) - 7;
        if (strncpy(dest, &filename[7], len) == nullptr)
        {
                return true;
        }
        dest[len] = '\0';
        return false;
}

/**
 * @brief Puts the configuration information for a specific widget inside the
 * destination struct. All these configurations come directly from the HTML
 * file, for session information get it from the config file instead.
 *
 * @param filename Absolute path to the widget file
 * @param content Pointer to the variable that will hold the raw HTML content
 * @param contentSize Max length of the content variable
 * @param dest Widget window context that the props will be saved to
 * @returns true if successful, else false on failure
 */
static bool
get_widget_config(const char *const filename,
                  char *const content,
                  const size_t contentSize,
                  ww_window_ctx *const dest)
{
        char root_dir[BUFFSIZE];
        if (ww_get_root_dir(filename, root_dir) == true)
        {
                return false;
        }

        char absolutePath[BUFFSIZE];
        if (remove_file_prefix(filename, absolutePath))
        {
                return false;
        }

        if (ww_get_file_content(absolutePath, content, contentSize))
        {
                return false;
        }

        char buf[BUFFSIZE] = {};
        const size_t bufLen = lengthof(buf);
        if (ww_begin_parsing(content, contentSize, TAG_APP_NAME, buf, bufLen))
        {
                memcpy(dest->title, buf, bufLen);
                dest->title[bufLen - 1] = '\0';
        }

        if (ww_begin_parsing(content, contentSize, TAG_WIN_SIZE, buf, bufLen))
        {
                size_t width, height;
                const bool isSet = Get2DValue(buf, &width, &height);
                dest->width = isSet ? width : DEF_WIDTH;
                dest->height = isSet ? height : DEF_WIDTH;
        }

        if (ww_begin_parsing(
                    content, contentSize, TAG_WIN_LOCATION, buf, bufLen))
        {
                size_t xPos, yPos;
                const bool isSet = Get2DValue(buf, &xPos, &yPos);
                dest->x = isSet ? xPos : DEF_X;
                dest->y = isSet ? yPos : DEF_Y;
        }

        if (ww_begin_parsing(
                    content, contentSize, TAG_APP_TOPMOST, buf, bufLen))
        {
                dest->top_most = strcmp(buf, "true") == 0;
        }

        if (ww_begin_parsing(
                    content, contentSize, TAG_WIN_BORD_RAD, buf, bufLen))
        {
                dest->radius = strtod(buf, nullptr);
        }

        const size_t filename_len = strlen(filename);
        if (filename_len >= BUFFSIZE)
        {
                return false;
        }
        memcpy(dest->filename, filename, filename_len);
        dest->filename[filename_len] = '\0';

        return true;
}

/**
 * @brief Destroy the window of a widget and set the widget as closed
 * @param widget Pointer to the index of the child widget
 * @param data Pointer to the widget context struct
 */
static gboolean
on_child_destroy(const GtkWidget *const, const void *const data)
{
        ww_widget_ctx *context = (ww_widget_ctx *)data;
        const size_t index = context->window_context.index;
        g_widgets[index] = g_widgets[g_widgetCount--];
        return false;
}

/**
 * @brief Stops the event loop when the main window closes
 * @param widget Pointer to the GtkWidget
 * @param running State of the main event loop
 * @returns true on success, false on failure
 */
static gboolean
on_main_destroy(const GtkWidget *const, bool *const running)
{
        *running = false;
        return false;
}

/**
 * @brief Event that is called when the Widget window is drawn
 * @param widget Pointer to the GtkWidget
 * @param cr Cairo object with information about the rendering device
 * @param data User data
 * @param context Window context with information about the widget to be drawn
 */
static gboolean
on_window_draw(GtkWidget *widget, cairo_t *, gpointer data)
{
        ww_window_ctx *context = (ww_window_ctx *)data;
        cairo_surface_t *shape_surface = nullptr;
        cairo_t *shape_cr = nullptr;
        cairo_region_t *shape_region = nullptr;

        GtkAllocation allocation;
        gtk_widget_get_allocation(widget, &allocation);

        if ((shape_surface = cairo_image_surface_create(
                     CAIRO_FORMAT_A1, allocation.width, allocation.height)) ==
            nullptr)
        {
                return false;
        }

        if ((shape_cr = cairo_create(shape_surface)) == nullptr)
        {
                return false;
        }

        cairo_set_operator(shape_cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_rgba(shape_cr, 0, 0, 0, 0);
        cairo_paint(shape_cr);

        cairo_set_operator(shape_cr, CAIRO_OPERATOR_OVER);
        cairo_set_source_rgba(shape_cr, 0, 0, 0, 1);

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

        return false;
}

/**
 * @brief Sets various Gtk window style properties
 * @param window Gtk widget of the window
 * @param context Context containing all styles
 */
static void
set_window_style(GtkWidget *const window, const window_style_t context)
{
        GtkWindow *gWindow = GTK_WINDOW(window);
        gtk_window_set_title(gWindow, context.title);
        gtk_window_set_default_size(gWindow, context.width, context.height);
        gtk_window_set_decorated(gWindow, context.hideTitleBar);
        gtk_window_set_keep_above(gWindow, context.topMost);
        gtk_window_move(gWindow, context.x, context.y);
        gtk_window_set_skip_taskbar_hint(gWindow, true);
}

/**
 * @brief Creates a menu item and adds it to the menu
 * @param menu Pointer to the GtkWidget
 * @param label Name of the item
 * @param callback Which function to trigger once this menu item is clicked
 * @param data Pointer to the widget context
 */
static void
create_menu_item(const GtkWidget *const menu,
                 const char *const label,
                 const void *const callback,
                 void *const data)
{
        GtkWidget *item = gtk_menu_item_new_with_label(label);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_signal_connect(item, SIGNAL_ACTIVATE, G_CALLBACK(callback), data);
        gtk_widget_show(item);
}

/**
 * @brief Sets the widget to the always on top
 * @param item Menu item
 * @param data Widget context struct
 */
static void
on_top_most_clicked(const GtkMenuItem *const, const void *const data)
{
        const ww_widget_ctx *ctx = (const ww_widget_ctx *)data;
        const GtkWidget *window = ctx->window;
        bool *top_most = (bool *)&ctx->window_context.top_most;
        *top_most = !(*top_most);
        gtk_window_set_keep_above(GTK_WINDOW(window), *top_most);
        save_main_config();
}

/**
 * @brief Closes the Window event
 * @param item Menu item
 * @param data Widget context struct
 */
static void
on_close_clicked(const GtkMenuItem *const, const void *const data)
{
        const ww_widget_ctx *ctx = (ww_widget_ctx *)data;
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
on_button_press(const GtkWidget *const,
                const GdkEventButton *const event,
                void *const data)
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
 * @returns true if successful, else false
 */
static bool
create_widget(const ww_window_ctx context)
{
        if (g_widgetCount >= MAX_WIDGETS)
        {
                return false;
        }

        GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        WebKitWebView *webview = g_widgets[g_widgetCount].webview;
        webview = WEBKIT_WEB_VIEW(webkit_web_view_new());

        window_style_t style = {.width = context.width,
                                .height = context.height,
                                .x = context.x,
                                .y = context.y,
                                .topMost = context.top_most,
                                .hideTitleBar = context.title_bar};
        const size_t titleLen = strlen(context.title);
        memcpy(style.title, context.title, titleLen);
        style.title[titleLen] = '\0';

        set_window_style(window, style);

        ww_widget_ctx *widget_context = &g_widgets[g_widgetCount];
        ww_window_ctx *window_context = &widget_context->window_context;
        memcpy(&widget_context->window, &window, sizeof(GtkWidget *));
        memcpy(window_context, &context, sizeof(ww_window_ctx));

        g_signal_connect(window,
                         SIGNAL_DESTROY,
                         G_CALLBACK(on_child_destroy),
                         widget_context);

        g_signal_connect(window,
                         SIGNAL_DRAW_WINDOW,
                         G_CALLBACK(on_window_draw),
                         widget_context);

        g_signal_connect(window,
                         SIGNAL_CONFIGURE,
                         G_CALLBACK(on_window_configure),
                         window_context);

        g_signal_connect(webview,
                         SIGNAL_BUTTON_PRESS,
                         G_CALLBACK(on_button_press),
                         widget_context);

        webkit_web_view_load_uri(webview, context.filename);

        gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
        gtk_widget_add_events(GTK_WIDGET(webview), GDK_BUTTON_PRESS_MASK);
        gtk_widget_show_all(window);

        g_widgetCount++;

        return true;
}

/**
 * @brief Helper function that parses and adds widgets to the stack
 * @param src JSON string with all widgets inside of it
 * @returns `true` on success, else `false` on error
 */
static bool
parse_last_session_widgets(const string_json_t lastSessionWidgets)
{

        size_t start = 0;
        for (size_t i = 0; i < lastSessionWidgets.length; i++)
        {
                if (lastSessionWidgets.str[i] == '{' && start == 0)
                {
                        start = i;
                }

                if (lastSessionWidgets.str[i] == '}')
                {
                        char obj[JSONBUFFSIZE];
                        GetSubstring(lastSessionWidgets.str, obj, start, i);

                        string_json_t jsonObj;
                        if (ConvertStringToJson(obj, &jsonObj) != FUNC_SUCCESS)
                        {
                                return false;
                        }

                        string_json_t jsonPath;
                        if (GetProperty(jsonObj, &jsonPath, "path") !=
                            FUNC_SUCCESS)
                        {
                                return false;
                        }

                        char path[BUFFSIZE];
                        if (ConvertJsonToString(jsonPath, path) != FUNC_SUCCESS)
                        {
                                return false;
                        }

                        string_json_t jsonPosition;
                        if (GetProperty(jsonObj, &jsonPosition, "position") !=
                            FUNC_SUCCESS)
                        {
                                return false;
                        }

                        char position[BUFFSIZE];
                        if (ConvertJsonToString(jsonPosition, position) !=
                            FUNC_SUCCESS)
                        {
                                return false;
                        }

                        size_t x, y;
                        if (!Get2DValue(position, &x, &y))
                        {
                                return false;
                        }

                        string_json_t jsonTopMost;
                        if (GetProperty(jsonObj, &jsonTopMost, "alwaysOnTop") !=
                            FUNC_SUCCESS)
                        {
                                return false;
                        }

                        char topMost[BUFFSIZE];
                        if (ConvertJsonToString(jsonTopMost, topMost) !=
                            FUNC_SUCCESS)
                        {
                                return false;
                        }

                        char content[USHRT_MAX];
                        ww_widget_ctx *widget = &g_widgets[g_widgetCount];
                        ww_window_ctx *window = &widget->window_context;
                        if (!get_widget_config(
                                    path, content, USHRT_MAX, window))
                        {
                                return false;
                        }

                        window->x = x;
                        window->y = y;
                        window->top_most = strcmp(topMost, "true") == 0;

                        if (!create_widget(*window))
                        {
                                return false;
                        }

                        start = 0;
                }
        }

        return true;
}

/**
 * @brief Applies the main config of the application. Used for opening widgets
 * that were not closed between sessions.
 * @returns true if successful, else false on failure
 */
static bool
apply_main_config()
{
        char absolutePath[BUFFSIZE];
        if (ww_default_widgets_dir(absolutePath))
        {
                return false;
        }
        strcat(absolutePath, PROG_CFG_NAME);

        if (access(absolutePath, F_OK) != 0)
        {
                return true;
        }

        char fileContent[JSONBUFFSIZE];
        if (ww_get_file_content(
                    absolutePath, fileContent, lengthof(fileContent)))
        {
                return false;
        }

        string_json_t jsonContent;
        if (ConvertStringToJson(fileContent, &jsonContent) != FUNC_SUCCESS)
        {
                return false;
        }

        string_json_t lastSessionWidgets;
        if (GetProperty(jsonContent,
                        &lastSessionWidgets,
                        "lastSessionWidgets") != FUNC_SUCCESS)
        {
                return false;
        }

        string_json_t isWidgetAutostartEnabledJson;
        if (GetProperty(jsonContent,
                        &isWidgetAutostartEnabledJson,
                        "isWidgetAutostartEnabled") != FUNC_SUCCESS)
        {
                return false;
        }

        char isWidgetAutostartEnabled[JSONBUFFSIZE];
        if (ConvertJsonToString(isWidgetAutostartEnabledJson,
                                isWidgetAutostartEnabled) != FUNC_SUCCESS)
        {
                return false;
        }

        string_json_t isOpenAppOnStartupEnabledJson;
        if (GetProperty(jsonContent,
                        &isOpenAppOnStartupEnabledJson,
                        "isOpenAppOnStartupEnabled") != FUNC_SUCCESS)
        {
                return false;
        }

        char isOpenAppOnStartupEnabled[JSONBUFFSIZE];
        if (ConvertJsonToString(isOpenAppOnStartupEnabledJson,
                                isOpenAppOnStartupEnabled) != FUNC_SUCCESS)
        {
                return false;
        }

        string_json_t isWidgetFullscreenHideEnabledJson;
        if (GetProperty(jsonContent,
                        &isWidgetFullscreenHideEnabledJson,
                        "isWidgetFullscreenHideEnabled") != FUNC_SUCCESS)
        {
                return false;
        }

        char isWidgetFullscreenHideEnabled[JSONBUFFSIZE];
        if (ConvertJsonToString(isWidgetFullscreenHideEnabledJson,
                                isWidgetFullscreenHideEnabled) != FUNC_SUCCESS)
        {
                return false;
        }

        return parse_last_session_widgets(lastSessionWidgets);
}

/**
 * @brief JavaScript calls this function to open a new widget
 * @param manager Content manger from Webkit
 * @param result JavaScript result of the operation
 * @param user_data Pointer to an array of widgets
 */
static void
on_open_widget_by_filename(const WebKitUserContentManager *const,
                           WebKitJavascriptResult *const result)

{
        JSCValue *jValue = nullptr;
        char *cValue = nullptr;

        if ((jValue = webkit_javascript_result_get_js_value(result)) == nullptr)
        {
                goto cleanup;
        }

        if (jsc_value_is_string(jValue) <= 0)
        {
                goto cleanup;
        }

        if ((cValue = jsc_value_to_string(jValue)) == nullptr)
        {
                goto cleanup;
        }

        const size_t len = strlen(cValue);
        if (len >= BUFFSIZE)
        {
                goto cleanup;
        }

        char filename[BUFFSIZE];
        memcpy(filename, cValue, len);
        filename[len] = '\0';

        ww_widget_ctx *widget = &g_widgets[g_widgetCount];
        ww_window_ctx *window = &widget->window_context;

        char content[USHRT_MAX];
        if (!get_widget_config(filename, content, lengthof(content), window))
        {
                goto cleanup;
        }

        if (!create_widget(*window))
        {
                goto cleanup;
        }

cleanup:
        if (cValue != nullptr)
        {
                g_free(cValue);
        }
}

/**
 * @brief JavaScript calls this function to open the default directory
 * @param manager Content manger from Webkit
 * @param result JavaScript result of the operation
 * @param user_data Pointer to custom user data
 */
static void
on_open_default_directory(const WebKitUserContentManager *const,
                          const WebKitJavascriptResult *const,
                          void *const)
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
 * @brief Used when the system tray icon's exit option is pressed.
 * @param data Pointer to the state of the event loop
 */
static void
on_application_exit(const GtkMenuItem *const, void *const data)
{
        bool *running = (bool *)data;
        *running = false;
}

/**
 * @brief Closes all open widgets of the application
 */
static void
on_application_close_widgets()
{
        for (size_t i = 1; i < g_widgetCount; i++)
        {
                const ww_widget_ctx widget = g_widgets[i];
                gtk_window_close(GTK_WINDOW(widget.window));
        }
}

/**
 * @brief Initializes the system tray icon for the application.
 * @param running State of the main event loop
 */
static void
init_system_tray(bool *const running)
{
        if ((g_tray.indicator = app_indicator_new(
                     PROG_NAME,
                     "/home/beyluta/Projects/WinWidgets/assets/imgs/"
                     "icon.png",
                     APP_INDICATOR_CATEGORY_APPLICATION_STATUS)) == nullptr)
        {
                return;
        }

        app_indicator_set_status(g_tray.indicator, APP_INDICATOR_STATUS_ACTIVE);
        app_indicator_set_icon(
                g_tray.indicator,
                "/home/beyluta/Projects/WinWidgets/assets/imgs/icon.png");

        if ((g_tray.menu = gtk_menu_new()) == nullptr)
        {
                return;
        }

        create_menu_item(
                g_tray.menu, LBL_SYSTRAY_EXIT, on_application_exit, running);
        create_menu_item(g_tray.menu,
                         LBL_SYSTRAY_CLOSE,
                         on_application_close_widgets,
                         nullptr);

        app_indicator_set_menu(g_tray.indicator, GTK_MENU(g_tray.menu));

        gtk_widget_show_all(g_tray.menu);
}

/**
 * @brief Used to destroy the resources for the system tray
 */
static void
destroy_system_tray()
{
        if (g_tray.exitBtn != nullptr)
        {
                gtk_widget_destroy(g_tray.exitBtn);
        }

        if (g_tray.menu != nullptr)
        {
                gtk_widget_destroy(g_tray.menu);
        }

        if (g_tray.indicator != nullptr)
        {
                g_object_unref(g_tray.indicator);
        }
}

/**
 * @brief Main event loop of the application
 * @param running True if the event loop should continue to run
 * @returns true if successful, else false
 */
static bool
event_loop(const bool *const running)
{
        while (*running)
        {
                if (!gtk_main_iteration_do(true))
                {
                        return false;
                }
        }

        return true;
}

bool
ww_init_main(const ww_window_ctx context)
{
        bool running = true;
        bool status = false;

        gtk_init(nullptr, nullptr);

        init_system_tray(&running);

        for (size_t i = 0; i < lengthof(g_widgets); i++)
        {
                g_widgets[i].window_context.index = i;
        }

        GtkWidget *window = nullptr;
        if ((window = gtk_window_new(GTK_WINDOW_TOPLEVEL)) == nullptr)
        {
                status = true;
                goto cleanup;
        }

        window_style_t style = {.width = context.width,
                                .height = context.height,
                                .x = context.x,
                                .y = context.y,
                                .hideTitleBar = true,
                                .topMost = context.top_most};
        const size_t titleLen = strlen(context.title);
        memcpy(style.title, context.title, titleLen);
        style.title[titleLen] = '\0';

        set_window_style(window, style);

        WebKitWebView *webview = nullptr;
        if ((webview = WEBKIT_WEB_VIEW(webkit_web_view_new())) == nullptr)
        {
                status = true;
                goto cleanup;
        }

        WebKitUserContentManager *manager = nullptr;
        if ((manager = webkit_web_view_get_user_content_manager(webview)) ==
            nullptr)
        {
                status = true;
                goto cleanup;
        }

        if (!webkit_user_content_manager_register_script_message_handler(
                    manager, LBL_EVT_GET_WIDGETS))
        {
                status = true;
                goto cleanup;
        }

        if (!webkit_user_content_manager_register_script_message_handler(
                    manager, LBL_EVT_OPEN_WIDGET))
        {
                status = true;
                goto cleanup;
        }

        if (!webkit_user_content_manager_register_script_message_handler(
                    manager, LBL_EVT_OPEN_DIR))
        {
                status = true;
                goto cleanup;
        }

        g_signal_connect(manager,
                         SIGNAL_GET_FILENAMES,
                         G_CALLBACK(on_get_widget_filenames),
                         webview);
        g_signal_connect(manager,
                         SIGNAL_OPEN_WIDGET,
                         G_CALLBACK(on_open_widget_by_filename),
                         g_widgets);
        g_signal_connect(manager,
                         SIGNAL_OPEN_DIRECTORY,
                         G_CALLBACK(on_open_default_directory),
                         nullptr);

        g_signal_connect(
                window, SIGNAL_DESTROY, G_CALLBACK(on_main_destroy), &running);

        webkit_web_view_load_uri(webview, context.filename);
        gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
        gtk_widget_show_all(window);

        if (!apply_main_config())
        {
                status = false;
                goto cleanup;
        }

        if (!event_loop(&running))
        {
                status = true;
                goto cleanup;
        }

cleanup:
        destroy_system_tray();

        return status;
}
