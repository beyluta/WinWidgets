// ======================= Purpose ==========================
//
// Entry point of the Linux codebase of the application.
// Reponsible for the creation of the native host and events.
//
// ==========================================================
#include "c_window.h"
#include "parser.h"
#include "widget.h"
#include "filesystem.h"
#include "string-builder.h"

typedef enum : uint8_t
{
        PARSE_TYPE_STRING,
        PARSE_TYPE_UINT8,
        PARSE_TYPE_BOOLEAN,
} parse_type_t;

static void
parse_and_get_2d_value(const string html,
                       const size_t html_length,
                       const string tag,
                       size_t *a,
                       size_t *b)
{
        string8_t temp_string;
        if (!ww_begin_parsing(
                    html, html_length, tag, temp_string, sizeof(temp_string)))
        {
                return;
        }

        Get2DValue(temp_string, a, b);
}

static bool
parse_and_get_value(const string html,
                    const size_t html_length,
                    const string tag,
                    const parse_type_t type,
                    void *pointer)
{
        string8_t temp_string;
        if (!ww_begin_parsing(
                    html, html_length, tag, temp_string, sizeof(temp_string)))
        {
                return false;
        }

        switch (type)
        {
        case PARSE_TYPE_STRING:
        {
                string value = (string)pointer;
                const size_t length = strlen(temp_string);
                strncpy(value, temp_string, length);
                value[length] = '\0';
                break;
        }
        case PARSE_TYPE_UINT8:
        {
                if (!isStringDigit(temp_string, sizeof(temp_string)))
                {
                        return false;
                }

                uint8_t *value = (uint8_t *)pointer;
                *value = strtod(temp_string, nullptr);
                break;
        }
        case PARSE_TYPE_BOOLEAN:
        {
                bool *value = (bool *)pointer;
                *value = strcmp(temp_string, "true") == 0;
                break;
        }
        }

        return true;
}

static void
on_mouse_move(void *const data)
{
        // window_t *self = (window_t *)data;
        // gint x, y;
        // GetMousePosition(&x, &y);
        // printf("Mouse position is %d and %d\n", x, y);
        // printf("Mouse moved!\n");
}

static void
on_mouse_button_press(void *const data,
                      const ww_window_mouse_press_event_t code)
{
        window_t *self = (window_t *)data;

        switch (code)
        {
        default:
        case WINDOW_MOUSE_PRESS_EVENT_RIGHT:
        {
                break;
        }
        }

        // gint x, y;
        // GetMousePosition(&x, &y);
        // printf("Mouse position is %d and %d\n", x, y);
        // printf("Mouse moved!\n");
}

static void
on_context_menu_item_selected(void *const data,
                              const ww_window_context_menu_selection_t code)
{
        window_t *self = (window_t *)data;

        switch (code)
        {
        default:
        case WINDOW_CONTEXT_MENU_SELECTION_MOVE:
        {
                break;
        }
        case WINDOW_CONTEXT_MENU_SELECTION_TOPMOST:
        {
                break;
        }
        case WINDOW_CONTEXT_MENU_SELECTION_CLOSE:
        {
                self->vtable->destroy(self);
                break;
        }
        }
}

static window_t *
window_child_new(window_t *const parent,
                 const string url,
                 const string html,
                 const size_t html_length)
{
        string8_t application_title;
        if (!parse_and_get_value(html,
                                 html_length,
                                 (string)TAG_APP_NAME,
                                 PARSE_TYPE_STRING,
                                 application_title))
        {
                return nullptr;
        }

        size_t width = DEF_WIDTH;
        size_t height = DEF_HEIGHT;
        parse_and_get_2d_value(
                html, html_length, (string)TAG_WIN_SIZE, &width, &height);

        size_t x = DEF_X;
        size_t y = DEF_Y;
        parse_and_get_2d_value(
                html, html_length, (string)TAG_WIN_LOCATION, &x, &y);

        size_t opacity = DEF_OPACITY;
        parse_and_get_value(html,
                            html_length,
                            (string)TAG_WIN_OPACITY,
                            PARSE_TYPE_UINT8,
                            &opacity);

        size_t radius = DEF_RADIUS;
        parse_and_get_value(html,
                            html_length,
                            (string)TAG_WIN_BORD_RAD,
                            PARSE_TYPE_UINT8,
                            &radius);

        bool show_title_bar = DEF_SHOW_TITLE_BAR;
        parse_and_get_value(html,
                            html_length,
                            (string)TAG_SHOW_TITLE_BAR,
                            PARSE_TYPE_BOOLEAN,
                            &show_title_bar);

        bool is_top_most = DEF_TOPMOST;
        parse_and_get_value(html,
                            html_length,
                            (string)TAG_APP_TOPMOST,
                            PARSE_TYPE_BOOLEAN,
                            &is_top_most);

        window_t *child = nullptr;
        if ((child = ww_window_new((window_t){.title = application_title,
                                              .width = width,
                                              .height = height,
                                              .x = x,
                                              .y = y,
                                              .opacity = opacity,
                                              .radius = radius,
                                              .show_title_bar = show_title_bar,
                                              .is_child = true,
                                              .is_top_most = is_top_most})) ==
            nullptr)
        {
                return nullptr;
        }

        child->vtable->set_url(child, url);
        child->vtable->register_event_mouse_motion(child, on_mouse_move);
        child->vtable->register_event_mouse_press(child, on_mouse_button_press);
        child->vtable->register_event_context_menu(
                child, on_context_menu_item_selected);

        parent->vtable->add_child(parent, child);

        return child;
}

static void
on_widget_container_clicked(void *, void *webkit_data, void *user_data)
{
        WebKitJavascriptResult *result = (WebKitJavascriptResult *)webkit_data;
        JSCValue *js_string = nullptr;
        if ((js_string = webkit_javascript_result_get_js_value(result)) ==
            nullptr)
        {
                return;
        }

        if (jsc_value_is_string(js_string) <= 0)
        {
                return;
        }

        string temp_string = nullptr;
        if ((temp_string = jsc_value_to_string(js_string)) == nullptr)
        {
                return;
        }

        string_builder_t *file_path = string_builder_new(temp_string);
        if (file_path == nullptr)
        {
                return;
        }

        string_builder_t *temp_file_path =
                string_builder_slice(file_path, 0, 7);
        if (temp_file_path == nullptr)
        {
                goto cleanup;
        }

        string16_t html_raw_content;
        if (ww_get_file_content(temp_file_path->data,
                                html_raw_content,
                                sizeof(html_raw_content)))
        {
                goto cleanup;
        }

        window_t *self = (window_t *)user_data;
        window_t *child = window_child_new(self,
                                           file_path->data,
                                           html_raw_content,
                                           sizeof(html_raw_content));
        if (child == nullptr)
        {
                goto cleanup;
        }

        child->vtable->show(child);

cleanup:
        if (temp_file_path != nullptr)
        {
                string_builder_free(temp_file_path);
        }

        if (file_path != nullptr)
        {
                string_builder_free(file_path);
        }
}

static void
on_document_object_model_loaded(void *, void *, void *data)
{
        string12_t default_dir;
        if (ww_default_widgets_dir(default_dir))
        {
                return;
        }

        ww_file_t *file = ww_get_all_files_from_directory(default_dir);
        for (ww_file_t *current_file = file; current_file != nullptr;
             current_file = current_file->next)
        {
                string16_t file_content;
                string12_t absolute_file;
                snprintf(absolute_file,
                         MAX_STR_SIZE,
                         "%s/%s",
                         default_dir,
                         current_file->name);
                if (ww_get_file_content(
                            absolute_file, file_content, MAX_FILE_SIZE))
                {
                        fprintf(stderr,
                                "Content of the HTML file could not be loaded "
                                "into buffer\n");
                        continue;
                }

                string12_t file_title;
                if (!ww_begin_parsing(file_content,
                                      sizeof(file_content),
                                      TAG_APP_NAME,
                                      file_title,
                                      sizeof(file_title)))
                {
                        fprintf(stderr,
                                "Failed to get title from HTML content\n");
                        continue;
                }

                string16_t func_args;
                snprintf(func_args,
                         MAX_STR_SIZE * 2,
                         "window.addWidget && window.addWidget(\"%s\", \"%s\")",
                         file_title,
                         absolute_file);

                window_t *self = (window_t *)data;
                self->vtable->run_javascript(self, func_args);
        }

        ww_free_all_files_from_directory(file);
}

int
main()
{
        setenv("WEBKIT_DISABLE_COMPOSITING_MODE", "1", true);
        setenv("GDK_BACKEND", "x11", true);

        string12_t temp;
        if (ww_default_widgets_dir(temp) == true)
        {
                fprintf(stderr,
                        "Failed to create the default widgets directory\n");
                return EXIT_REASON_IO_FAILURE;
        }

        string12_t html;
        if (ww_default_index_html(html) == true)
        {
                fprintf(stderr, "Failed to get the default index webpage\n");
                return EXIT_REASON_IO_FAILURE;
        }

        window_t *main_window = ww_window_new((window_t){.title = "Main window",
                                                         .width = 1000,
                                                         .height = 1000,
                                                         .x = 0,
                                                         .y = 0,
                                                         .opacity = 1,
                                                         .radius = 0,
                                                         .show_title_bar = true,
                                                         .is_child = false,
                                                         .is_top_most = true});

        main_window->vtable->set_url(main_window, html);

        main_window->vtable->register_event_callback(
                main_window,
                main_window->vtable->get_manager(main_window),
                "on_get_widget_filenames",
                on_document_object_model_loaded,
                main_window);

        main_window->vtable->register_event_callback(
                main_window,
                main_window->vtable->get_manager(main_window),
                "on_open_widget_by_filename",
                on_widget_container_clicked,
                main_window);

        main_window->vtable->show(main_window);
        main_window->vtable->destroy(main_window);

        return EXIT_REASON_TERMINATED;
}
