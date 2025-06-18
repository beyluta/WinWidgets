#include "filesystem.h"
#include "global.h"
#include <dirent.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_HTML_PATH "assets/index.html"
#define DEFAULT_HTML_DIR ".local/share/WinWidgets"

static size_t get_executable_path(char *dest, const size_t max_len) {
#if __linux__
  const ssize_t len = readlink("/proc/self/exe", dest, max_len - 1);
  if (len == OOB) {
    fprintf(stderr, "Failed to get path to the executable\n");
    return OOB;
  }
  dest[len] = '\0';
  return len;
#else
  // NOTE: Use GetModuleFileName(NULL, dest, sizeof(dest - 1)) for Windows
  // NOTE: There is also _pgmptr from <windows.h>
  fprintf(stderr, "Not implemented for this OS\n");
#endif
  return 0;
}

static size_t str_to_file(const char *src, char *dest) {
  size_t len = strlen(src) + 7;
  size_t size = snprintf(dest, len + 1, "file://%s", src);
  if (size == OOB) {
    fprintf(stderr, "Failed to append file prefix to path\n");
    return OOB;
  }
  dest[len] = '\0';
  return size;
}

static BOOLEAN create_dir(const char *path) {
  if (mkdir(path, 0777) <= OOB) {
    fprintf(stderr, "Failed to run mkdir on path %s\n", path);
  }
  return BOOLEAN_TRUE;
}

static BOOLEAN get_app_directory(char *dest) {
  const char *home = getenv("HOME");
  if (home == NULL) {
    fprintf(stderr, "Failed to get the user's home directory\n");
    return BOOLEAN_FALSE;
  }

  size_t len = strlen(home) + strlen(DEFAULT_HTML_DIR) + 1;
  snprintf(dest, len + 1, "%s/%s", home, DEFAULT_HTML_DIR);
  dest[len] = '\0';

  return BOOLEAN_TRUE;
}

BOOLEAN ww_default_index_html(char *dest) {
  char exec_path[BUFFSIZE];
  const size_t exec_len = get_executable_path(exec_path, sizeof(exec_path));
  if (exec_len == OOB) {
    fprintf(stderr, "Failed get path to the HTML file\n");
    return BOOLEAN_FALSE;
  }

  const char *directory = dirname(exec_path);
  const char html_path[] = DEFAULT_HTML_PATH;
  const size_t total_len = strlen(directory) + strlen(html_path) + 2;
  const size_t len = snprintf(dest, total_len, "%s/%s", exec_path, html_path);
  if (len == OOB) {
    fprintf(stderr, "Failed to concatenate HTML file path\n");
    return BOOLEAN_FALSE;
  }
  dest[total_len] = '\0';

  char file[BUFFSIZE];
  const size_t file_size = str_to_file(dest, file);
  if (file_size == OOB) {
    fprintf(stderr, "Failed to build correct string\n");
    return BOOLEAN_FALSE;
  }
  memcpy(dest, file, file_size);
  dest[file_size] = '\0';

  return BOOLEAN_TRUE;
}

BOOLEAN ww_default_widgets_dir(char *dest) {
  char app_dir[BUFFSIZE];
  if (get_app_directory(app_dir) == BOOLEAN_FALSE) {
    fprintf(stderr, "Failed to get default HTML directory\n");
    return BOOLEAN_FALSE;
  }

  if (create_dir(app_dir) == BOOLEAN_FALSE) {
    fprintf(stderr, "Failed to create the default directory for widgets\n");
  } // Don't return inside this condition; it's just a warning

  const size_t len = strlen(app_dir);
  memcpy(dest, app_dir, len);
  dest[len] = '\0';

  return BOOLEAN_TRUE;
}

static BOOLEAN str_ends_with(const char *src, const char *prefix) {
  const size_t src_len = strlen(src);
  const size_t prefix_len = strlen(prefix);
  if (src < prefix) {
    return BOOLEAN_FALSE;
  }

  for (size_t i = src_len, k = prefix_len; i > 0; i--) {
    if (src[i] == prefix[k]) {
      k--;
      if (k <= 0) {
        return BOOLEAN_TRUE;
      }
      continue;
    }
    return BOOLEAN_FALSE;
  }
  return BOOLEAN_FALSE;
}

size_t ww_get_files_from_dir(const char *src, char dest[][BUFFSIZE],
                             const size_t size) {
  DIR *directory = opendir(src);
  if (directory == NULL) {
    fprintf(stderr, "Failed to open directory %s\n", src);
    return 0;
  }

  size_t i = 0;
  const struct dirent *dir = NULL;
  while ((dir = readdir(directory)) != NULL) {
    if (dir->d_name[0] == '.' || i >= size ||
        str_ends_with(dir->d_name, ".html") == BOOLEAN_FALSE)
      continue;

    const size_t total_len = strlen(src) + strlen(dir->d_name) + 2;
    char temp[total_len];
    snprintf(temp, total_len, "%s/%s", src, dir->d_name);
    temp[total_len - 1] = '\0';

    const size_t len = strlen(temp);
    memcpy(dest[i++], temp, len);
  }
  closedir(directory);

  return i;
}

BOOLEAN ww_get_file_content(const char *src, char *dest, const size_t max_len) {
  FILE *file = fopen(src, "rb");
  if (file == NULL) {
    fprintf(stderr, "Failed to open file for reading\n");
    return BOOLEAN_FALSE;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    fprintf(stderr, "Failed to read file to end\n");
    return BOOLEAN_FALSE;
  }

  const long length = ftell(file);
  if (length <= -1) {
    fclose(file);
    fprintf(stderr, "Failed to get current position of the stream\n");
    return BOOLEAN_FALSE;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    fprintf(stderr, "Failed to set seek\n");
    return BOOLEAN_FALSE;
  }

  const unsigned long fileLength = fread(dest, 1, length, file);
  if (fileLength <= 0) {
    fclose(file);
    fprintf(stderr, "Failed to read contents into memory\n");
    return BOOLEAN_FALSE;
  }

  if (fileLength > max_len) {
    fclose(file);
    fprintf(stderr, "Buffer is too small to hold file contents\n");
    return BOOLEAN_FALSE;
  }

  fclose(file);
  dest[length] = '\0';
  return BOOLEAN_TRUE;
}

BOOLEAN ww_get_root_dir(const char *src, char *dest) {
  // Getting the starting and ending indexes
  const size_t len = strlen(src);
  size_t end = len;
  for (size_t i = len; i > 0; i--) {
    if (src[i] == '/' && i < len && len > 1) {
      end = i;
      break;
    }
  }

  if (strncpy(dest, src, end) == NULL) {
    fprintf(stderr, "Failed to return the root directory of %s\n", src);
    return BOOLEAN_FALSE;
  }
  dest[end] = '\0';
  return BOOLEAN_TRUE;
}

BOOLEAN ww_get_filename_from_absolute_path(const char *src, char *dest) {
  const size_t len = strlen(src);
  size_t start = 0;
  for (size_t i = len; i > 0; i--) {
    if (src[i] == '/') {
      start = i;
      break;
    }
  }

  const size_t size = len - start;
  if (strncpy(dest, &src[start], size) == NULL) {
    fprintf(stderr, "Failed to get the filename of %s\n", src);
    return BOOLEAN_FALSE;
  }
  dest[size] = '\0';
  return BOOLEAN_TRUE;
}

BOOLEAN ww_write_to_file(const char *src, const char *content) {
  FILE *file = fopen(src, "w");
  if (file == NULL) {
    fprintf(stderr, "Failed to open file %s\n", src);
    return BOOLEAN_FALSE;
  }

  if (fprintf(file, "%s", content) < 0) {
    fclose(file);
    fprintf(stderr, "Failed to write to file %s\n", src);
    return BOOLEAN_FALSE;
  }

  fclose(file);
  return BOOLEAN_TRUE;
}
