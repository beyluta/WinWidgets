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
                          .index = 0, // Not really needed, but for consistency
                          .title_bar = BOOLEAN_TRUE,
                          .child = BOOLEAN_FALSE,
                          .top_most = BOOLEAN_FALSE,
                          .title = "WinWidgets",
                          .opacity = 1,
                          .radius = 0};

  // Copying pointers to the struct
  const size_t len = strlen(html);
  if (len >= BUFFSIZE) {
    fprintf(stderr, "Buffer size of filename was larger than expected\n");
    return BOOLEAN_FALSE;
  }
  memcpy(window.filename, html, len);
  window.filename[len] = '\0';

  /* All widgets and their corresponding configuration will be saved inside this
   * variable. We also go through all widgets and set their index number here.
   * This way we avoid allocating memory for it later */
  ww_widget_ctx widgets[MAX_WIDGETS];
  for (size_t i = 0; i < MAX_WIDGETS; i++) {
    widgets[i].window_context.index = i;
  }

  // Initializing the main window
  if (ww_init_main(&window, widgets) == BOOLEAN_FALSE) {
    fprintf(stderr, "Failed to create the widget\n");
  }

  return BOOLEAN_TRUE;
}
