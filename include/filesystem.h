#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "global.h"
#include "utils.h"
#include <stddef.h>
#include <sys/types.h>

constexpr unsigned char WRITE_APPEND = 0;
constexpr unsigned char WRITE_OVERWRITE = 1;

typedef struct ww_file_t ww_file_t;

struct ww_file_t
{
        string name;
        ww_file_t *next;
        size_t length;
};

/**
 * @brief Gets the default HTML file for the wigets manager
 * @param dest Full path to the file
 */
bool
ww_default_index_html(char *dest);

/**
 * @brief Gets the default path to the Widgets.
 * Creates the directory first if it doesn't exist.
 * @param dest Full path to the widgets directory
 * @param n Max number of bytes dest can have
 * @returns Bytes written to the destination buffer
 */
size_t
ww_default_widgets_dir(char *const dest, const size_t n);

/**
 * @brief Gets a list of files inside a directory
 * @param src Directory to search for the files
 * @param dest Array of all files
 * @param size Size of the files to load into memory
 * @return Count of files read from directory
 */
size_t
ww_get_files_from_dir(const char *src,
                      char dest[][BUFFSIZE],
                      const size_t size);

/**
 * @brief Gets the amount of bytes a file has
 * @param src File to count the bytes
 * @returns Amount of bytes reported by the operating system
 */
size_t
ww_get_file_bytes(const char *const src);

/**
 * @brief Gets the entire content of a file
 * @param src Full path to the file on the system
 * @param dest String to save the content to
 * @param max_len Max length of the file content
 * @return Status of the operation
 */
bool
ww_get_file_content(const char *src, char *dest, const size_t max_len);

/**
 * @brief Gets the root directory of a file
 * @param src Full path to the file on the system
 * @param dest String to save the content to
 * @return Status of the operation
 */
bool
ww_get_root_dir(const char *src, char *dest);

/**
 * @brief Gets the name of a file from the absolute path
 * @param src Full path to the file on the system
 * @param dest String to save the content to
 * @return Status of the operation
 */
bool
ww_get_filename_from_absolute_path(const char *src, char *dest);

/**
 * @brief Saves the content to a file
 * @param src Full path to the file on the system
 * @param content Content to write to the file
 * @param mode How to write to the file `overwrite = 1` or `append = 0`
 * @return Status of the operation
 */
bool
ww_write_to_file(const char *src, const char *content, const size_t mode);

/**
 * @brief Opens the file explorer and nagivates to the specified directory
 * @param src Absolute path to the file on the system
 * @return Status of the operation
 */
bool
ww_open_folder(const char *src);

/**
 * @brief Gets whether a specific folder exists
 * @param src Full path to the folder
 * @param true if successful, else false is returned
 */
bool
ww_folder_exists(const char *const src);

/**
 * @brief Gets the path to the executable binary
 * @param dest String where the result will be saved to
 * @param max_len Max length of the destination string
 * @returns 0 if successful, else some numeric error code on failure
 */
ssize_t
ww_get_executable_path(char *const dest, const size_t max_len);

/**
 * @brief Goes up a chain of directories inside a string. The string may contain
 * both / or \ as delimiters.
 *
 * @param src Source containing the entire path
 * @param srcLen Length of the source string
 * @param dest Destination string
 * @param destLen Length of the destination string
 * @returns true if successful, else false on failure
 */
bool
ww_dir_up(const char *const src,
          const size_t srcLen,
          char *const dest,
          const size_t destLen);

/**
 * @brief Gets all files from a directory
 * @param src Directory to get the files from
 * @returns A struct containing a list of files
 */
ww_file_t *
ww_get_all_files_from_directory(const string src);

/**
 * @brief Frees all files fetched from directory
 * @brief src all files fetched from directory
 */
void
ww_free_all_files_from_directory(ww_file_t *src);

#endif
