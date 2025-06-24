#include "filesystem.h"
#include "global.h"
#include "widget.h"
#include <stdio.h>
#include <string.h>

int main(void) {
  // Creating the default widgets directory
  char temp[BUFFSIZE];
  if (ww_default_widgets_dir(temp) == BOOLEAN_FALSE) {
    fprintf(stderr, "Failed to create the default widgets directory\n");
    return BOOLEAN_FALSE;
  }

  // Getting the default HTML index file for the widget manager.
  char html[BUFFSIZE];
  if (ww_default_index_html(html) == BOOLEAN_FALSE) {
    fprintf(stderr, "Failed to get the default index webpage\n");
    return BOOLEAN_FALSE;
  }

  // Creating the default window. This is the window manager
  // that displays a list of all the available widgets.
  ww_window_ctx window = {.width = 1000,
                          .height = 1000,
                          .x = 0,
                          .y = 0,
                          .title_bar = BOOLEAN_TRUE,
                          .child = BOOLEAN_FALSE,
                          .top_most = BOOLEAN_FALSE,
                          // .filename = html,
                          .title = "WinWidgets",
                          .opacity = 1,
                          .radius = 0};
  // Copying pointers to the struct
  strncpy(window.filename, html, strlen(html));

  // Default list of all Widgets
  ww_widget_ctx widgets[MAX_WIDGETS];

  // Initializing the main window
  if (ww_init_main(&window, widgets) == BOOLEAN_FALSE) {
    fprintf(stderr, "Failed to create the widget\n");
  }

  return BOOLEAN_TRUE;
}
