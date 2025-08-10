#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "global.h"
#include <stddef.h>

constexpr unsigned char WRITE_APPEND = 0;
constexpr unsigned char WRITE_OVERWRITE = 1;

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
 */
bool
ww_default_widgets_dir(char *dest);

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

#endif
