#include "widget.h"
#include "filesystem.h"
#include "gdk/gdk.h"
#include "global.h"
#include <cairo/cairo.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <webkit2/webkit2.h>

#define TICK_DELAY 1
#define MAX_HTML_LEN 4096
#define CFG_SUFFIX ".cfg"
#define CLICK_RIGHT 3

static void set_rgba_visuals(GtkWidget *window) {
  GdkScreen *screen = gtk_widget_get_screen(window);
  GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
  if (visual != NULL && gdk_screen_is_composited(screen)) {
    gtk_widget_set_visual(window, visual);
  }
  gtk_widget_set_app_paintable(window, TRUE);
}

static void set_window_opacity(GtkWidget *window, const double opacity) {
  gtk_widget_set_opacity(window, opacity);
}

static void call_js_function(const char *func, const char *arg,
                             const gpointer user_data) {
  // Building the javascript command to execute
  const size_t len = (2 * strlen(func)) + strlen(arg) + 21;
  char command[len];
  snprintf(command, len + 1, "window.%s && window.%s(%s);", func, func, arg);
  command[len] = '\0';

  // Running the command
  WebKitWebView *webview = WEBKIT_WEB_VIEW(user_data);
  webkit_web_view_evaluate_javascript(webview, command, -1, NULL, NULL, NULL,
                                      NULL, NULL);
}

static void on_get_widget_filenames(WebKitUserContentManager *manager,
                                    WebKitJavascriptResult *result,
                                    gpointer user_data) {
  // Getting the default app directory
  char app_dir[BUFFSIZE];
  if (ww_default_widgets_dir(app_dir) == BOOLEAN_FALSE) {
    fprintf(stderr, "Failed to get default app directory\n");
    return;
  }

  // Getting all filenames from the directory
  char widgets[MAX_WIDGETS][BUFFSIZE];
  const size_t count = ww_get_files_from_dir(app_dir, widgets, MAX_WIDGETS);
  if (count <= 0) {
    fprintf(stderr, "Failed to read directory %s\n", app_dir);
    return;
  }

  // Building a comma separated list with the list all files found
  size_t j = 0;
  const size_t len =
      (MAX_WIDGETS * BUFFSIZE) + (MAX_WIDGETS * 2) + (MAX_WIDGETS - 1) + 2;
  char list[len];
  for (size_t i = 0; i < count; i++) {
    const size_t size = strlen(widgets[i]);
    char temp[size + 3];
    snprintf(temp, sizeof(temp) + 1, "'%s',", widgets[i]);
    memcpy(&list[j], temp, sizeof(temp));
    j = j + sizeof(temp);
  }
  list[j - 1] = '\0';

  // Build a JSON object out of the list
  const size_t list_len = strlen(list) + 4;
  char json[list_len];
  snprintf(json, list_len + 1, "\"[%s]\"", list);
  json[list_len] = '\0';

  // Calling the function from JavaScript
  call_js_function("addWidgets", json, user_data);
}

/**
 * @brief Removes the file:// prefix from the string
 * @param filename File with the prefix attached
 * @param dest String to save the new string to
 * @returns Status of the operation
 */
static BOOLEAN remove_file_prefix(const char *filename, char *dest) {
  const size_t len = strlen(filename) - 7;
  if (strncpy(dest, &filename[7], len) == NULL) {
    fprintf(stderr, "Failed to remove the prefix from the filename: %s\n",
            filename);
    return BOOLEAN_FALSE;
  }
  dest[len] = '\0';
  return BOOLEAN_TRUE;
}

/**
 * @brief Get a target value from the provided config file
 * @param src Complete config file content
 * @param target Target attribute to get the value from
 * @param dest String to save the value to
 */
static BOOLEAN get_widget_config_attribute(const char *src, const char *target,
                                           char *dest) {
  const size_t len = strlen(src);
  const size_t target_len = strlen(target);
  BOOLEAN value_found = BOOLEAN_FALSE;
  int start = -1, end = -1;
  for (size_t i = 0, k = 0; i < len; i++) {
    if (src[i] == target[k] && value_found == BOOLEAN_FALSE) {
      k++;
      if (k >= target_len) {
        value_found = BOOLEAN_TRUE;
      }
      continue;
    }

    if (value_found == BOOLEAN_TRUE) {
      if (src[i] == '=' && start < 0 && i + 1 < len) {
        start = i + 1;
      } else if (src[i] == '\n' && end < 0) {
        end = i;
      }

      if (start > 0 && end > 0) {
        const size_t size = end - start;
        if (strncpy(dest, &src[start], size) == NULL) {
          fprintf(stderr, "Failed to get value of attribute %s\n", target);
          return BOOLEAN_FALSE;
        }
        dest[size] = '\0';
        return BOOLEAN_TRUE;
      }
      continue;
    }

    k = 0;
  }
  return BOOLEAN_FALSE;
}

/**
 * @brief Gets the configuration file for a widget
 * @param filename Absolute path to the widget file
 * @param dest Widget window context that the props will be saved to
 * @returns Status of the operation
 */
static BOOLEAN get_widget_config(const char *filename, ww_window_ctx *dest) {
  char root_dir[BUFFSIZE];
  if (ww_get_root_dir(filename, root_dir) == BOOLEAN_FALSE) {
    fprintf(stderr, "Could not get root directory of file %s\n", filename);
    return BOOLEAN_FALSE;
  }

  char *file = (char *)malloc(sizeof(char) * BUFFSIZE);
  if (file == NULL) {
    fprintf(stderr, "Failed to allocate memory for file name\n");
    return BOOLEAN_FALSE;
  }

  if (ww_get_filename_from_absolute_path(filename, file) == BOOLEAN_FALSE) {
    fprintf(stderr, "Could not get file name\n");
    free(file);
    return BOOLEAN_FALSE;
  }

  char config_path[BUFFSIZE];
  const size_t len = strlen(root_dir) + strlen(file) + strlen(CFG_SUFFIX);
  if (snprintf(config_path, len + 1, "%s%s%s", root_dir, file, CFG_SUFFIX) <
      0) {
    fprintf(stderr, "Could not get the config file path\n");
    free(file);
    return BOOLEAN_FALSE;
  }
  config_path[len] = '\0';
  free(file);
  file = NULL;

  char config_path_no_prefix[BUFFSIZE];
  if (remove_file_prefix(config_path, config_path_no_prefix) == BOOLEAN_FALSE) {
    fprintf(stderr, "Could not remove prefix from file %s\n", config_path);
    free(file);
    return BOOLEAN_FALSE;
  }

  char config_content[BUFFSIZE];
  memset(config_content, 0, BUFFSIZE);
  if (ww_get_file_content(config_path_no_prefix, config_content, BUFFSIZE) ==
      BOOLEAN_FALSE) {
    fprintf(stderr, "Could not get the content from the config file: %s\n",
            config_path);
  }

  // Getting the properties from the config files
  // ---------------------------------------------------
  char *title = (char *)malloc(sizeof(char) * BUFFSIZE);
  if (title == NULL) {
    fprintf(stderr, "Failed to allocate memory for title\n");
    return BOOLEAN_FALSE;
  }

  if (get_widget_config_attribute(config_content, "title", title) ==
      BOOLEAN_FALSE) {
    strncpy(title, "Child", sizeof(title) - 1);
    title[sizeof(title) - 1] = '\0';
  }

  char width[BUFFSIZE];
  if (get_widget_config_attribute(config_content, "width", width) ==
      BOOLEAN_FALSE) {
    strncpy(width, "800", sizeof(width) - 1);
    width[sizeof(width) - 1] = '\0';
  }

  char height[BUFFSIZE];
  if (get_widget_config_attribute(config_content, "height", height) ==
      BOOLEAN_FALSE) {
    strncpy(height, "600", sizeof(height) - 1);
    height[sizeof(height) - 1] = '\0';
  }

  char x[BUFFSIZE];
  if (get_widget_config_attribute(config_content, "x", x) == BOOLEAN_FALSE) {
    strncpy(x, "0", sizeof(x) - 1);
    x[sizeof(x) - 1] = '\0';
  }

  char y[BUFFSIZE];
  if (get_widget_config_attribute(config_content, "y", y) == BOOLEAN_FALSE) {
    strncpy(y, "0", sizeof(y) - 1);
    y[sizeof(y) - 1] = '\0';
  }

  char title_bar[BUFFSIZE];
  if (get_widget_config_attribute(config_content, "title_bar", title_bar) ==
      BOOLEAN_FALSE) {
    strncpy(title_bar, "true", sizeof(title_bar) - 1);
    title_bar[sizeof(title_bar) - 1] = '\0';
  }

  char top_most[BUFFSIZE];
  if (get_widget_config_attribute(config_content, "top_most", top_most) ==
      BOOLEAN_FALSE) {
    strncpy(top_most, "false", sizeof(top_most) - 1);
    top_most[sizeof(top_most) - 1] = '\0';
  }

  char opacity[BUFFSIZE];
  if (get_widget_config_attribute(config_content, "opacity", opacity) ==
      BOOLEAN_FALSE) {
    strncpy(opacity, "1", sizeof(opacity) - 1);
    opacity[sizeof(opacity) - 1] = '\0';
  }

  char radius[BUFFSIZE];
  if (get_widget_config_attribute(config_content, "radius", radius) ==
      BOOLEAN_FALSE) {
    strncpy(radius, "0", sizeof(radius) - 1);
    radius[sizeof(radius) - 1] = '\0';
  }

  const ww_window_ctx window = {
      .width = (size_t)strtol(width, NULL, 10),
      .height = (size_t)strtol(height, NULL, 10),
      .x = (size_t)strtol(x, NULL, 10),
      .y = (size_t)strtol(y, NULL, 10),
      .title_bar =
          (strcmp(title_bar, "true") == 0) ? BOOLEAN_FALSE : BOOLEAN_TRUE,
      .child = BOOLEAN_TRUE,
      .top_most =
          (strcmp(top_most, "true") == 0) ? BOOLEAN_TRUE : BOOLEAN_FALSE,
      .filename = (char *)filename,
      .title = title,
      .opacity = (double)strtod(opacity, NULL),
      .radius = (double)strtod(radius, NULL)};
  memcpy(dest, &window, sizeof(ww_window_ctx));

  return BOOLEAN_TRUE;
}

/**
 * @brief Saves the window context to the config file
 * @param window Window context object
 * @returns Status of the operation
 */
static BOOLEAN save_widget_config(const ww_window_ctx *window) {
  char filename_without_prefix[BUFFSIZE];
  if (remove_file_prefix(window->filename, filename_without_prefix) ==
      BOOLEAN_FALSE) {
    fprintf(stderr, "Could not remove file prefix from: %s\n",
            window->filename);
    return BOOLEAN_FALSE;
  }

  char filename_with_suffix[BUFFSIZE];
  const size_t len = strlen(filename_without_prefix) + strlen(CFG_SUFFIX);
  if (snprintf(filename_with_suffix, len + 1, "%s%s", filename_without_prefix,
               CFG_SUFFIX) < 0) {

    fprintf(stderr, "Failed to add the configuration suffix to %s\n",
            filename_with_suffix);
    return BOOLEAN_FALSE;
  }
  filename_with_suffix[len] = '\0';

  char content[BUFFSIZE];
  snprintf(content, BUFFSIZE,
           "title=%s\n"
           "width=%zu\n"
           "height=%zu\n"
           "x=%zu\n"
           "y=%zu\n"
           "title_bar=%s\n"
           "top_most=%s\n"
           "opacity=%f\n"
           "radius=%f\n",
           window->title, window->width, window->height, window->x, window->y,
           window->title_bar == BOOLEAN_FALSE ? "true" : "false",
           window->top_most == BOOLEAN_TRUE ? "true" : "false", window->opacity,
           window->radius);

  if (ww_write_to_file(filename_with_suffix, content) == BOOLEAN_FALSE) {
    fprintf(stderr, "Failed to write content to file\n");
    return BOOLEAN_FALSE;
  }

  return BOOLEAN_TRUE;
}

typedef struct widget_destroy_obj {
  ww_widget_ctx **widgets;
  size_t index;
} widget_destroy_obj;

static gboolean on_child_destroy(GtkWidget *widget, void *data) {
  const widget_destroy_obj *obj = (const widget_destroy_obj *)data;

  // Not worth setting all these to NULL
  free((void *)(obj->widgets[obj->index])->window_context->title);
  free((void *)(obj->widgets[obj->index])->window_context->filename);
  free((void *)(*obj->widgets[obj->index]).window_context);
  free((void *)obj->widgets[obj->index]);
  obj->widgets[obj->index] = NULL; // This one is needed though

  free(data);
  data = NULL;
  return false;
}

static gboolean on_main_destroy(GtkWidget *widget, void *data) {
  BOOLEAN *running = (BOOLEAN *)data;
  *running = BOOLEAN_FALSE;
  return false;
}

static void on_window_draw(GtkWidget *widget, cairo_t *cr,
                           const ww_window_ctx *context) {
  // Bounds of the gtk window
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);

  // Initializing cairo objects
  cairo_surface_t *shape_surface = cairo_image_surface_create(
      CAIRO_FORMAT_A1, allocation.width, allocation.height);
  if (shape_surface == NULL) {
    fprintf(stderr, "Failed to initialize a cairo surface\n");
    return;
  }

  cairo_t *shape_cr = cairo_create(shape_surface);
  if (shape_cr == NULL) {
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
  const double max_radius = MIN(allocation.width, allocation.height) / 2.0;
  const double border_radius = context->radius;
  const double radius = border_radius > max_radius ? max_radius : border_radius;
  const double x = 0, y = 0, w = allocation.width, h = allocation.height;
  cairo_new_sub_path(shape_cr);
  cairo_arc(shape_cr, x + w - radius, y + radius, radius, -G_PI / 2, 0);
  cairo_arc(shape_cr, x + w - radius, y + h - radius, radius, 0, G_PI / 2);
  cairo_arc(shape_cr, x + radius, y + h - radius, radius, G_PI / 2, G_PI);
  cairo_arc(shape_cr, x + radius, y + radius, radius, G_PI, 3 * G_PI / 2);
  cairo_close_path(shape_cr);
  cairo_fill(shape_cr);

  // Creating and applying the shape region
  cairo_region_t *shape_region =
      gdk_cairo_region_create_from_surface(shape_surface);
  if (shape_region == NULL) {
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

static BOOLEAN set_window_style(ww_window_ctx *context, GtkWidget *window) {
  gtk_window_set_title(GTK_WINDOW(window), context->title);
  gtk_window_set_default_size(GTK_WINDOW(window), context->width,
                              context->height);
  gtk_window_set_decorated(GTK_WINDOW(window),
                           context->title_bar == BOOLEAN_TRUE);
  gtk_window_move(GTK_WINDOW(window), context->x, context->y);

  set_rgba_visuals(window); // Options below require this
  set_window_opacity(window, context->opacity);
  gtk_window_set_keep_above(GTK_WINDOW(window),
                            context->top_most == BOOLEAN_TRUE);
  return BOOLEAN_TRUE;
}

static void create_menu_item(GtkWidget *menu, const char *label, void *callback,
                             void *data) {
  GtkWidget *item = gtk_menu_item_new_with_label(label);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  g_signal_connect(item, "activate", G_CALLBACK(callback), data);
  gtk_widget_show(item);
}

static void on_top_most_clicked(GtkMenuItem *item, void *data) {
  const ww_widget_ctx *ctx = (const ww_widget_ctx *)data;
  const GtkWidget *window = ctx->window;
  BOOLEAN *top_most = (BOOLEAN *)&ctx->window_context->top_most;
  *top_most = ctx->window_context->top_most == BOOLEAN_TRUE ? BOOLEAN_FALSE
                                                            : BOOLEAN_TRUE;
  gtk_window_set_keep_above(GTK_WINDOW(window), *top_most == BOOLEAN_TRUE);
}

static void on_close_clicked(GtkMenuItem *item, void *data) {
  const ww_widget_ctx *ctx = (const ww_widget_ctx *)data;
  const GtkWidget *window = ctx->window;
  gtk_window_close(GTK_WINDOW(window));
}

static gboolean on_button_press(GtkWidget *window, GdkEventButton *event,
                                void *data) {
  if (event->type == GDK_BUTTON_PRESS && event->button == CLICK_RIGHT) {
    GtkWidget *menu = gtk_menu_new();
    create_menu_item(menu, "Top Most", on_top_most_clicked, data);
    create_menu_item(menu, "Close Widget", on_close_clicked, data);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
    return TRUE;
  }
  return FALSE;
}

static BOOLEAN create_widget(ww_window_ctx *context, ww_widget_ctx **widgets) {
  // Checking if we can create more widgets
  static size_t widget_index = 0;
  if (widget_index >= MAX_WIDGETS) {
    fprintf(stderr, "Failed to add widget to list. Widget limit exceeded.\n");
    return BOOLEAN_FALSE;
  }

  // Setting GTK window options
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (set_window_style(context, window) == BOOLEAN_FALSE) {
    fprintf(stderr, "Failed to set window default style\n");
    return BOOLEAN_FALSE;
  }

  // Creating the WebKit browser
  WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());

  ww_widget_ctx *widget_context =
      (ww_widget_ctx *)malloc(sizeof(ww_widget_ctx));
  if (widget_context == NULL) {
    fprintf(stderr, "Failed to allocate memory for widget context\n");
    return BOOLEAN_FALSE;
  }

  widget_context->window_context =
      (ww_window_ctx *)malloc(sizeof(ww_window_ctx));
  if (widget_context->window_context == NULL) {
    fprintf(stderr, "Failed to allocate memory for window context\n");
    free(widget_context);
    return BOOLEAN_FALSE;
  }

  widget_context->window = window;

  memcpy((void *)widget_context->window_context, context,
         sizeof(ww_window_ctx));

  widgets[widget_index] = widget_context;

  // Hooking up scripts and event handlers
  g_signal_connect(window, "draw", G_CALLBACK(on_window_draw),
                   (ww_window_ctx *)context);
  widget_destroy_obj *destroy_obj =
      (widget_destroy_obj *)malloc(sizeof(widget_destroy_obj));
  destroy_obj->widgets = widgets;
  destroy_obj->index = widget_index;
  g_signal_connect(window, "destroy", G_CALLBACK(on_child_destroy),
                   destroy_obj);
  gtk_widget_add_events(GTK_WIDGET(webview), GDK_BUTTON_PRESS_MASK);
  g_signal_connect(webview, "button-press-event", G_CALLBACK(on_button_press),
                   widget_context);

  // Configuring the WebKit Browser
  webkit_web_view_load_uri(webview, context->filename);
  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
  gtk_widget_show_all(window);
  widget_index++;

  return BOOLEAN_TRUE;
}

static void on_open_widget_by_filename(WebKitUserContentManager *manager,
                                       WebKitJavascriptResult *result,
                                       gpointer user_data) {
  // Get the value passed from the JavaScript
  JSCValue *value = webkit_javascript_result_get_js_value(result);
  if (jsc_value_is_string(value) <= 0) {
    fprintf(
        stderr,
        "Failed to open widget by filename. Argument type was incorrect.\n");
    return;
  }
  const char *jsc_value = jsc_value_to_string(value);
  const char *filename =
      (const char *)malloc(sizeof(const char) * strlen(jsc_value));
  strncpy((void *)filename, jsc_value, strlen(jsc_value));

  // Getting the configuration into a struct
  ww_window_ctx window = {0};
  if (get_widget_config(filename, &window) == BOOLEAN_FALSE) {
    fprintf(stderr, "Could not load existing configuration\n");
  }

  ww_widget_ctx **widget_ctx = (ww_widget_ctx **)user_data;
  create_widget(&window, widget_ctx);
}

static BOOLEAN event_loop(ww_widget_ctx **widgets, BOOLEAN *running) {
  size_t next_tick = time(NULL) + TICK_DELAY;
  while (*running == BOOLEAN_TRUE) {
    gtk_main_iteration_do(TRUE);

    for (size_t i = 0; i < MAX_WIDGETS; i++) {
      if (widgets[i] == NULL) {
        continue;
      }

      // Getting the context for window and widget
      const ww_widget_ctx *child = widgets[i];
      const GtkWidget *gtk_window = (GtkWidget *)child->window;
      const ww_window_ctx *window = child->window_context;

      // Values that need to be updated in the config file
      size_t x = 0, y = 0, w = 0, h = 0;
      gtk_window_get_position(GTK_WINDOW(gtk_window), (int *)&x, (int *)&y);
      gtk_window_get_size(GTK_WINDOW(gtk_window), (int *)&w, (int *)&h);
      size_t *ptr_x = (size_t *)&window->x;
      size_t *ptr_y = (size_t *)&window->y;
      size_t *ptr_w = (size_t *)&window->width;
      size_t *ptr_h = (size_t *)&window->height;
      *ptr_x = x;
      *ptr_y = y;
      *ptr_w = w;
      *ptr_h = h;

      // Saving the widgets to the config file if time has elapsed
      const size_t now = time(NULL);
      if (now < next_tick) {
        continue;
      }
      next_tick = now + TICK_DELAY;

      if (save_widget_config(window) == BOOLEAN_FALSE) {
        fprintf(stderr, "Failed to save new widget position and scale\n");
      }
    }
  }
  return BOOLEAN_TRUE;
}

BOOLEAN ww_init_main(ww_window_ctx *context, ww_widget_ctx **widgets) {
  // Initializing GTK
  gtk_init(NULL, NULL);

  // Setting GTK window options
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  if (set_window_style(context, window) == BOOLEAN_FALSE) {
    fprintf(stderr, "Failed to set window default style\n");
    return BOOLEAN_FALSE;
  }

  // Creating the WebKit browser
  WebKitWebView *webview = WEBKIT_WEB_VIEW(webkit_web_view_new());

  // Hooking up scripts and event handlers
  WebKitUserContentManager *manager =
      webkit_web_view_get_user_content_manager(webview);
  webkit_user_content_manager_register_script_message_handler(
      manager, "on_get_widget_filenames");
  webkit_user_content_manager_register_script_message_handler(
      manager, "on_open_widget_by_filename");
  g_signal_connect(manager, "script-message-received::on_get_widget_filenames",
                   G_CALLBACK(on_get_widget_filenames), webview);
  g_signal_connect(manager,
                   "script-message-received::on_open_widget_by_filename",
                   G_CALLBACK(on_open_widget_by_filename), (void *)widgets);

  static BOOLEAN running = BOOLEAN_TRUE;
  g_signal_connect(window, "destroy", G_CALLBACK(on_main_destroy), &running);

  // Configuring the WebKit Browser
  webkit_web_view_load_uri(webview, context->filename);
  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(webview));
  gtk_widget_show_all(window);

  // Event loop
  if (event_loop(widgets, &running) == BOOLEAN_FALSE) {
    fprintf(stderr, "Could not start start event loop\n");
    return BOOLEAN_FALSE;
  }

  return BOOLEAN_TRUE;
}
