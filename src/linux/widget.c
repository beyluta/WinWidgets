// ======================= Purpose ==========================
//
// Entry point of the Linux codebase of the application.
// Reponsible for the creation of the native host and events.
//
// ==========================================================
#include "agent.h"
#include "utils.h"
#include "window.h"
#include "parser.h"
#include "widget.h"
#include "filesystem.h"
#include "cyaml.h"
#include <pthread.h>
#include <stdint.h>

// Private members

static constexpr char YAML_FILE_SUFFIX[] = ".yaml";

enum parse_type : uint8_t
{
        PARSE_TYPE_STRING,
        PARSE_TYPE_UINT8,
        PARSE_TYPE_BOOLEAN,
};

enum dynamic_event : uint8_t
{
        DYNAMIC_EVENT_ON_GET_WIDGET_FILENAMES,
        DYNAMIC_EVENT_ON_OPEN_WIDGET_BY_FILENAME,
        DYNAMIC_EVENT_ON_OPEN_DEFAULT_DIRECTORY,
        DYNAMIC_EVENT_ON_AGENT_USER_PROMPT_RECEIVED,
        DYNAMIC_EVEN_ON_AGENT_UNLOAD_FROM_MEMORY,
};

struct agent_send_prompt_options
{
        window_t *self;
        string prompt;
};

typedef enum parse_type parse_type_t;
typedef enum dynamic_event dynamic_event_t;
typedef struct agent_send_prompt_options agent_send_prompt_options_t;

static string
dynamic_event_get_str(dynamic_event_t code)
{
        switch (code)
        {
        default:
        case DYNAMIC_EVENT_ON_GET_WIDGET_FILENAMES:
                return "on_get_widget_filenames";
        case DYNAMIC_EVENT_ON_OPEN_WIDGET_BY_FILENAME:
                return "on_open_widget_by_filename";
        case DYNAMIC_EVENT_ON_OPEN_DEFAULT_DIRECTORY:
                return "on_open_default_directory";
        case DYNAMIC_EVENT_ON_AGENT_USER_PROMPT_RECEIVED:
                return "on_agent_user_prompt_received";
        case DYNAMIC_EVEN_ON_AGENT_UNLOAD_FROM_MEMORY:
                return "on_agent_unload_from_memory";
        }
}

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
                memcpy(value, temp_string, length);
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
on_context_menu_item_selected(void *const data,
                              const ww_window_context_menu_selection_t code)
{
        window_t *self = (window_t *)data;

        switch (code)
        {
        case WINDOW_CONTEXT_MENU_SELECTION_CLOSE:
        {
                window_destroy(self);
                break;
        }
        }
}

static window_t *
window_child_new(window_t *const parent,
                 const string url,
                 const string_t html,
                 const size_t guid,
                 size_t x,
                 size_t y)
{
        string8_t application_title;
        if (!parse_and_get_value(html.data,
                                 html.length,
                                 (string)TAG_APP_NAME,
                                 PARSE_TYPE_STRING,
                                 application_title))
        {
                return nullptr;
        }

        size_t width = DEF_WIDTH;
        size_t height = DEF_HEIGHT;
        parse_and_get_2d_value(
                html.data, html.length, (string)TAG_WIN_SIZE, &width, &height);

        if (x == 0 && y == 0)
        {
                x = DEF_X;
                y = DEF_Y;
                parse_and_get_2d_value(html.data,
                                       html.length,
                                       (string)TAG_WIN_LOCATION,
                                       &x,
                                       &y);
        }

        size_t opacity = DEF_OPACITY;
        parse_and_get_value(html.data,
                            html.length,
                            (string)TAG_WIN_OPACITY,
                            PARSE_TYPE_UINT8,
                            &opacity);

        size_t radius = DEF_RADIUS;
        parse_and_get_value(html.data,
                            html.length,
                            (string)TAG_WIN_BORD_RAD,
                            PARSE_TYPE_UINT8,
                            &radius);

        bool show_title_bar = DEF_SHOW_TITLE_BAR;
        parse_and_get_value(html.data,
                            html.length,
                            (string)TAG_SHOW_TITLE_BAR,
                            PARSE_TYPE_BOOLEAN,
                            &show_title_bar);

        window_t opts = {.width = width,
                         .height = height,
                         .x = x,
                         .y = y,
                         .opacity = opacity,
                         .radius = radius,
                         .show_title_bar = show_title_bar,
                         .is_child = true};

        window_t *child = window_new(opts,
                                     application_title,
                                     strlen(application_title),
                                     guid,
                                     nullptr);

        if (child == nullptr)
        {
                return nullptr;
        }

        window_set_url(child, url, strlen(url));
        window_set_position(child, x, y);

        window_register_event_context_menu(child,
                                           on_context_menu_item_selected);

        window_add_child(parent, child);

        return child;
}

static void
on_widget_container_clicked(void *, void *webkit_data, void *user_data)
{
        WebKitJavascriptResult *result = (WebKitJavascriptResult *)webkit_data;
        JSCValue *js_string = nullptr;
        string temp_file_path = nullptr;

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

        size_t bytes = strlen(temp_string);
        string file_path = nullptr;
        if ((file_path = (string)malloc(sizeof(char) * (bytes + 1))) == nullptr)
        {
                goto cleanup;
        }

        memcpy(file_path, temp_string, bytes);
        file_path[bytes] = '\0';

        bytes = bytes - 7;
        if ((temp_file_path = (string)malloc(sizeof(char) * (bytes + 1))) ==
            nullptr)
        {
                goto cleanup;
        }

        memcpy(temp_file_path, &file_path[7], bytes);
        temp_file_path[bytes] = '\0';

        string16_t html_raw_content;
        if (ww_get_file_content(temp_file_path,
                                html_raw_content,
                                sizeof(html_raw_content)) == 0)
        {
                goto cleanup;
        }

        window_t *self = (window_t *)user_data;

        window_t *child =
                window_child_new(self,
                                 file_path,
                                 (string_t){.data = html_raw_content,
                                            .length = sizeof(html_raw_content)},
                                 0,
                                 0,
                                 0);

        if (child == nullptr)
        {
                goto cleanup;
        }

        window_show(child);

cleanup:
        if (temp_file_path != nullptr)
        {
                free(temp_file_path);
        }

        if (file_path != nullptr)
        {
                free(file_path);
        }
}

static void
on_document_object_model_loaded(void *, void *, void *data)
{
        ww_file_t *fp = nullptr;

        char dir[PATH_MAX];
        if (ww_default_widgets_dir(dir, sizeof(dir) - 1) == 0)
        {
                goto cleanup;
        }

        fp = ww_get_all_files_from_directory(dir, FILE_FILTER_HTML);
        if (fp == nullptr)
        {
                goto cleanup;
        }

        for (ww_file_t *current_file = fp; current_file != nullptr;
             current_file = current_file->next)
        {
                char fb[PATH_MAX];
                ssize_t bytes = snprintf(
                        fb, sizeof(fb) - 1, "%s/%s", dir, current_file->name);
                if (bytes < 0)
                {
                        continue;
                }

                char buff[MAX_FILE_SIZE];
                if (ww_get_file_content(fb, buff, sizeof(buff) - 1) == 0)
                {
                        fprintf(stderr, "Buffer too small for HTML content\n");
                        continue;
                }

                char file_title[BUFFSIZE];
                if (!ww_begin_parsing(buff,
                                      sizeof(buff),
                                      TAG_APP_NAME,
                                      file_title,
                                      sizeof(file_title) - 1))
                {
                        fprintf(stderr, "Failed to get title from HTML\n");
                        continue;
                }

                string16_t func_args;
                snprintf(func_args,
                         MAX_STR_SIZE * 2,
                         "window.addWidget && window.addWidget(\"%s\", \"%s\")",
                         file_title,
                         fb);

                window_t *self = (window_t *)data;
                window_run_javascript(self, func_args);
        }

cleanup:
        if (fp != nullptr)
        {
                ww_free_all_files_from_directory(fp);
        }
}

static void
on_open_default_directory(void *, void *, void *)
{
        char dir[PATH_MAX];
        if (ww_default_widgets_dir(dir, sizeof(dir) - 1) == 0)
        {
                return;
        }

        if (ww_open_folder(dir))
        {
                return;
        }
}

static ssize_t
build_js_command(string dest,
                 const size_t max,
                 const string func,
                 const string param)
{
        const size_t size = !dest ? 0 : max + 1;
        return snprintf(dest,
                        size,
                        "window.%s && window.%s(\"%s\")",
                        func,
                        func,
                        param);
}

static void *
on_agent_prompt_processing(void *data)
{
        agent_send_prompt_options_t *options =
                (agent_send_prompt_options_t *)data;

        agent_llama_t *agent_llama = nullptr;
        window_get_agent_instance(options->self, &agent_llama);
        if (!agent_llama)
        {
                agent_llama_t *new_agent = window_llama_init();
                window_set_agent_instance(options->self, new_agent);
                if (!new_agent)
                {
                        fprintf(stderr, "Failed to create agent instance\n");
                        pthread_exit(nullptr);
                }
        }

        string response =
                window_agent_send_prompt(options->self, options->prompt);
        if (!response)
        {
                fprintf(stderr, "Failed to get a response from the agent\n");
                free(options->prompt);
                pthread_exit(nullptr);
        }

        string no_think = agent_tool_strip_think_response(response);
        if (no_think)
        {
                free(response);
                response = no_think;
        }

        string safe_response = nullptr;
        string text = agent_tool_strip_codeblock_response(response);
        if (text)
        {
                safe_response = EscapeJavaScriptParamString(text);
                free(text);
        }
        else
        {
                safe_response = EscapeJavaScriptParamString(response);
        }

        if (!safe_response)
        {
                fprintf(stderr, "Failed to escape string from double quotes\n");
                free(response);
                free(options->prompt);
                pthread_exit(nullptr);
        }

        const ssize_t len =
                build_js_command(nullptr, 0, "submitChat", safe_response);
        string command = (string)malloc(sizeof(char) * (len + 1));
        if (!command)
        {
                fprintf(stderr, "Failed to allocate memory for the command\n");
                free(safe_response);
                free(response);
                free(options->prompt);
                pthread_exit(nullptr);
        }

        build_js_command(command, len, "submitChat", safe_response);
        window_run_javascript(options->self, command);

        string codeblock = agent_tool_codeblock_scan(response);
        if (codeblock)
        {
                string safe_codeblock = EscapeJavaScriptParamString(codeblock);
                if (!safe_codeblock)
                {
                        free(codeblock);
                        free(options->prompt);
                        pthread_exit(nullptr);
                }

                const ssize_t len = build_js_command(
                        nullptr, 0, "addHtmlMessage", safe_codeblock);

                string buf = malloc(sizeof(char) * (len + 1));
                if (!buf)
                {
                        free(safe_codeblock);
                        free(codeblock);
                        free(options->prompt);
                        pthread_exit(nullptr);
                }

                build_js_command(buf, len, "addHtmlMessage", safe_codeblock);
                window_run_javascript(options->self, buf);

                free(safe_codeblock);
                free(buf);
                free(codeblock);
        }

        free(command);
        free(safe_response);
        free(response);
        free(options->prompt);
        free(options);
        return nullptr;
}

static void
on_agent_user_prompt_received(void *, void *webkit_data, void *user_data)
{
        WebKitJavascriptResult *result = (WebKitJavascriptResult *)webkit_data;
        JSCValue *jsstr = webkit_javascript_result_get_js_value(result);
        if (!jsstr)
        {
                fprintf(stderr, "Failed to get a value from JavaScript\n");
                return;
        }

        if (jsc_value_is_string(jsstr) <= 0)
        {
                fprintf(stderr, "Value received is not a string\n");
                return;
        }

        string cstr = nullptr;
        if ((cstr = jsc_value_to_string(jsstr)) == nullptr)
        {
                fprintf(stderr, "No user prompt received from JavaScript\n");
                return;
        }

        string prompt = strdup(cstr);
        if (!prompt)
        {
                fprintf(stderr, "Failed to duplicate prompt string\n");
                return;
        }

        agent_send_prompt_options_t *prompt_options =
                (agent_send_prompt_options_t *)malloc(
                        sizeof(agent_send_prompt_options_t));
        if (!prompt_options)
        {
                fprintf(stderr, "Failed to create prompt options\n");
                free(prompt);
                return;
        }

        window_t *self = (window_t *)user_data;
        prompt_options->self = self;
        prompt_options->prompt = prompt;

        pthread_t thread;
        pthread_create(
                &thread, nullptr, on_agent_prompt_processing, prompt_options);
        pthread_detach(thread);
}

static void
on_agent_unload_from_memory(void *, void *webkit_data, void *user_data)
{
        window_t *self = (window_t *)user_data;
        window_free_agent_instance(self);
}

static void
on_window_realized(window_t *self)
{
        ww_file_t *file = nullptr;
        yaml_s *yaml = nullptr;

        char dir[PATH_MAX];
        ssize_t bytes = ww_default_widgets_dir(dir, sizeof(dir) - 1);
        if (bytes == 0)
        {
                goto cleanup;
        }

        file = ww_get_all_files_from_directory(dir, FILE_FILTER_YAML);
        if (file == nullptr)
        {
                goto cleanup;
        }

        ww_file_t *fptr = file;
        while (fptr != nullptr)
        {
                const ssize_t index = substrcmp(fptr->name,
                                                fptr->length,
                                                YAML_FILE_SUFFIX,
                                                sizeof(YAML_FILE_SUFFIX) - 1);
                if (index < 0)
                {
                        goto cleanup;
                }

                char fp[PATH_MAX];
                bytes = snprintf(fp, sizeof(fp) - 1, "%s/%s", dir, fptr->name);
                if (bytes < 0)
                {
                        goto cleanup;
                }

                char fb[4096];
                bytes = ww_get_file_content(fp, fb, sizeof(fb) - 1);
                if (bytes == 0)
                {
                        goto cleanup;
                }

                if ((yaml = yaml_load(fb, bytes)) == nullptr)
                {
                        goto cleanup;
                }

                yaml_node_s *root = yaml_root_node(yaml);
                if (root == nullptr)
                {
                        goto cleanup;
                }

                yaml_node_s *nURL = yaml_get_node(root, "url");
                if (nURL == nullptr)
                {
                        goto cleanup;
                }

                yaml_node_s *nGuid = yaml_get_node(root, "guid");
                if (nGuid == nullptr)
                {
                        goto cleanup;
                }

                yaml_node_s *nX = yaml_get_node(root, "x");
                if (nX == nullptr)
                {
                        goto cleanup;
                }

                yaml_node_s *nY = yaml_get_node(root, "y");
                if (nY == nullptr)
                {
                        goto cleanup;
                }

                char url[PATH_MAX];
                bytes = yaml_get_primitive(nURL, url, sizeof(url) - 1);
                if (bytes == 0)
                {
                        goto cleanup;
                }

                char guid[32];
                bytes = yaml_get_primitive(nGuid, guid, sizeof(guid) - 1);
                if (bytes == 0)
                {
                        goto cleanup;
                }

                char x[16];
                bytes = yaml_get_primitive(nX, x, sizeof(x) - 1);
                if (bytes == 0)
                {
                        goto cleanup;
                }

                char y[16];
                bytes = yaml_get_primitive(nY, y, sizeof(y) - 1);
                if (bytes == 0)
                {
                        goto cleanup;
                }

                char html[MAX_FILE_SIZE];
                const size_t htmlSize = sizeof(html) - 1;
                if (ww_get_file_content(&url[7], html, htmlSize) == 0)
                {
                        goto cleanup;
                }

                window_t *child = window_child_new(
                        self,
                        url,
                        (string_t){.data = html, .length = htmlSize},
                        strtoul(guid, nullptr, 10),
                        strtoul(x, nullptr, 10),
                        strtoul(y, nullptr, 10));

                if (child == nullptr)
                {
                        goto cleanup;
                }

                window_show(child);

                yaml_free(yaml);
                yaml = nullptr;

                fptr = fptr->next;
        }

cleanup:
        if (yaml != nullptr)
        {
                yaml_free(yaml);
        }

        if (file != nullptr)
        {
                ww_free_all_files_from_directory(file);
        }
}

// Public members

int
main()
{
        setenv("WEBKIT_DISABLE_COMPOSITING_MODE", "1", true);
        setenv("GDK_BACKEND", "x11", true);

        string12_t temp;
        if (ww_default_widgets_dir(temp, sizeof(temp) - 1) == true)
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

        window_t opts = {.width = 1000,
                         .height = 1000,
                         .x = 0,
                         .y = 0,
                         .opacity = 1,
                         .radius = 0,
                         .show_title_bar = true,
                         .is_child = false};

        window_t *self = window_new(
                opts, PROG_NAME, sizeof(PROG_NAME) - 1, 0, on_window_realized);

        window_set_url(self, html, strlen(html));

        window_register_event_callback(
                self,
                window_get_manager(self),
                dynamic_event_get_str(DYNAMIC_EVENT_ON_GET_WIDGET_FILENAMES),
                on_document_object_model_loaded,
                self);

        window_register_event_callback(
                self,
                window_get_manager(self),
                dynamic_event_get_str(DYNAMIC_EVENT_ON_OPEN_WIDGET_BY_FILENAME),
                on_widget_container_clicked,
                self);

        window_register_event_callback(
                self,
                window_get_manager(self),
                dynamic_event_get_str(DYNAMIC_EVENT_ON_OPEN_DEFAULT_DIRECTORY),
                on_open_default_directory,
                self);

        window_register_event_callback(
                self,
                window_get_manager(self),
                dynamic_event_get_str(
                        DYNAMIC_EVENT_ON_AGENT_USER_PROMPT_RECEIVED),
                on_agent_user_prompt_received,
                self);

        window_register_event_callback(
                self,
                window_get_manager(self),
                dynamic_event_get_str(DYNAMIC_EVEN_ON_AGENT_UNLOAD_FROM_MEMORY),
                on_agent_unload_from_memory,
                self);

        window_show(self);
        window_destroy(self);

        return EXIT_REASON_TERMINATED;
}
