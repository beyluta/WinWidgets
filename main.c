#include "filesystem.h"
#include "global.h"
#include "widget.h"
#include <stdio.h>
#include <string.h>

#if _WIN32
#include <minwindef.h>
#endif

#if __linux__
int
main(void)
{
#elif _WIN32
int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, int nCmdShow)
{
#endif
        char temp[BUFFSIZE];
        if (ww_default_widgets_dir(temp) == true)
        {
                fprintf(stderr,
                        "Failed to create the default widgets directory\n");
                return EXIT_REASON_IO_FAILURE;
        }

        char html[BUFFSIZE];
        if (ww_default_index_html(html) == true)
        {
                fprintf(stderr, "Failed to get the default index webpage\n");
                return EXIT_REASON_IO_FAILURE;
        }

        // Creating the default window. This is the window manager
        // that displays a list of all the available widgets.
        ww_window_ctx window = {
                .width = 1000,
                .height = 1000,
                .x = 0,
                .y = 0,
                .index = 0, // Not really needed, but for consistency
                .title_bar = false,
                .child = false,
                .top_most = true,
                .title = "WinWidgets",
                .opacity = 1,
                .radius = 0};

        const size_t len = strlen(html);
        if (len >= BUFFSIZE)
        {
                fprintf(stderr,
                        "Buffer size of filename was larger than expected\n");
                return EXIT_REASON_MEM_FAILURE;
        }
        memcpy(window.filename, html, len);
        window.filename[len] = '\0';

        ww_widget_ctx widgets[MAX_WIDGETS];
        for (size_t i = 0; i < MAX_WIDGETS; i++)
        {
                widgets[i].window_context.index = i;
        }

#if __linux__
        if (ww_init_main(&window, widgets) == true)
        {
#elif _WIN32
        if (ww_init_main(hInstance, nCmdShow, &window, widgets) == true)
        {
#endif
                fprintf(stderr, "Failed to create the widget\n");
        }

        return EXIT_REASON_TERMINATED;
}
