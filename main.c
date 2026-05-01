#include "filesystem.h"
#include "global.h"
#include "remres.h"
#include "config.h"
#include "widget.h"
#include <stdint.h>
#include <stdio.h>
#include <minwindef.h>
#include <limits.h>

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR pCmdLine, int nCmdShow)
{
        char def_dir[PATH_MAX];
        if (ww_default_widgets_dir(def_dir) == true)
        {
                fprintf(stderr,
                        "Failed to create the default widgets directory\n");
                return EXIT_REASON_IO_FAILURE;
        }

        char url[255];
        if (ww_read_resource_string("remoteUrl", url, sizeof(url) - 1) > 0)
        {
                char archive[PATH_MAX];
                ww_get_default_resource_from_remote(
                        url, def_dir, archive, sizeof(archive) - 1);
                ww_unzip_archive(archive);
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
        if (ww_init_main(hInstance, nCmdShow, pCmdLine, &window) == true)
        {
                fprintf(stderr, "Failed to create the widget\n");
        }

        return EXIT_REASON_TERMINATED;
}
