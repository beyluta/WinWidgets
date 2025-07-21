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

constexpr char DEFAULT_HTML_PATH[] = "assets/index.html";
constexpr char DEFAULT_HTML_DIR[] = ".local/share/WinWidgets";

static size_t get_executable_path(char *dest, const size_t max_len) {
#if __linux__
  const ssize_t len = readlink("/proc/self/exe", dest, max_len - 1);
  if (len == OPERATION_STATE_INDEX_OUT_OF_BOUNDS) {
    fprintf(stderr, "Failed to get path to the executable\n");
    return OPERATION_STATE_INDEX_OUT_OF_BOUNDS;
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
  ssize_t size = snprintf(dest, len + 1, "file://%s", src);
  if (size == OPERATION_STATE_INDEX_OUT_OF_BOUNDS) {
    fprintf(stderr, "Failed to append file prefix to path\n");
    return OPERATION_STATE_INDEX_OUT_OF_BOUNDS;
  }
  dest[len] = '\0';
  return size;
}

static bool create_dir(const char *path) {
  if (mkdir(path, 0777) <= OPERATION_STATE_INDEX_OUT_OF_BOUNDS) {
    fprintf(stderr, "Failed to run mkdir on path %s\n", path);
  }
  return false;
}

static bool get_app_directory(char *dest) {
  const char *home = getenv("HOME");
  if (home == NULL) {
    fprintf(stderr, "Failed to get the user's home directory\n");
    return true;
  }

  size_t len = strlen(home) + strlen(DEFAULT_HTML_DIR) + 1;
  snprintf(dest, len + 1, "%s/%s", home, DEFAULT_HTML_DIR);
  dest[len] = '\0';

  return false;
}

bool ww_default_index_html(char *dest) {
  char exec_path[BUFFSIZE];
  const ssize_t exec_len = get_executable_path(exec_path, sizeof(exec_path));
  if (exec_len == OPERATION_STATE_INDEX_OUT_OF_BOUNDS) {
    fprintf(stderr, "Failed get path to the HTML file\n");
    return true;
  }

  const char *directory = dirname(exec_path);
  const size_t total_len = strlen(directory) + strlen(DEFAULT_HTML_PATH) + 2;
  const ssize_t len =
      snprintf(dest, total_len, "%s/%s", exec_path, DEFAULT_HTML_PATH);
  if (len == OPERATION_STATE_INDEX_OUT_OF_BOUNDS) {
    fprintf(stderr, "Failed to concatenate HTML file path\n");
    return true;
  }
  dest[total_len] = '\0';

  char file[BUFFSIZE];
  const ssize_t file_size = str_to_file(dest, file);
  if (file_size == OPERATION_STATE_INDEX_OUT_OF_BOUNDS) {
    fprintf(stderr, "Failed to build correct string\n");
    return true;
  }
  memcpy(dest, file, file_size);
  dest[file_size] = '\0';

  return false;
}

bool ww_default_widgets_dir(char *dest) {
  char app_dir[BUFFSIZE];
  if (get_app_directory(app_dir) == true) {
    fprintf(stderr, "Failed to get default HTML directory\n");
    return true;
  }

  if (create_dir(app_dir) == true) {
    fprintf(stderr, "Failed to create the default directory for widgets\n");
  } // Don't return inside this condition; it's just a warning

  const size_t len = strlen(app_dir);
  memcpy(dest, app_dir, len);
  dest[len] = '\0';

  return false;
}

static bool str_ends_with(const char *src, const char *prefix) {
  const size_t src_len = strlen(src);
  const size_t prefix_len = strlen(prefix);
  if (src < prefix) {
    return true;
  }

  for (size_t i = src_len, k = prefix_len; i > 0; i--) {
    if (src[i] == prefix[k]) {
      k--;
      if (k <= 0) {
        return false;
      }
      continue;
    }
    return true;
  }
  return true;
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
        str_ends_with(dir->d_name, ".html") == true)
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

bool ww_get_file_content(const char *src, char *dest, const size_t max_len) {
  FILE *file = fopen(src, "rb");
  if (file == NULL) {
    fprintf(stderr, "Failed to open file for reading\n");
    return true;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    fprintf(stderr, "Failed to read file to end\n");
    return true;
  }

  const long length = ftell(file);
  if (length <= -1) {
    fclose(file);
    fprintf(stderr, "Failed to get current position of the stream\n");
    return true;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    fprintf(stderr, "Failed to set seek\n");
    return true;
  }

  const unsigned long fileLength = fread(dest, 1, length, file);
  if (fileLength <= 0) {
    fclose(file);
    fprintf(stderr, "Failed to read contents into memory\n");
    return true;
  }

  if (fileLength > max_len) {
    fclose(file);
    fprintf(stderr, "Buffer is too small to hold file contents\n");
    return true;
  }

  fclose(file);
  dest[length] = '\0';
  return false;
}

bool ww_get_root_dir(const char *src, char *dest) {
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
    return true;
  }
  dest[end] = '\0';
  return false;
}

bool ww_get_filename_from_absolute_path(const char *src, char *dest) {
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
    return true;
  }
  dest[size] = '\0';
  return false;
}

bool ww_write_to_file(const char *src, const char *content, const size_t mode) {
  FILE *file = fopen(src, mode == 0 ? "a" : "w");
  if (file == NULL) {
    fprintf(stderr, "Failed to open file %s\n", src);
    return true;
  }

  if (fprintf(file, "%s", content) < 0) {
    fclose(file);
    fprintf(stderr, "Failed to write to file %s\n", src);
    return true;
  }

  fclose(file);
  return false;
}

bool ww_open_folder(const char *src) {
  char cmd[BUFFSIZE];

  // Length of the path + length of command + null terminator
  const size_t len = strlen(src) + 9 + 1;

  // Building the command to open the folder
  if (snprintf(cmd, len, "xdg-open %s", src) < 0) {
    fprintf(stderr, "Failed to construct absolute path to folder\n");
  }
  cmd[len] = '\0';

  // Calling the command here
  system(cmd);
  return false;
}
