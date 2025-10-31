#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

typedef enum : uint8_t
{
        widget_char_space = 32,
        widget_char_quote = 34,
        widget_char_slash = 47,
        widget_char_b_slash = 92,
        widget_char_gt = 62
} widget_char_t;

/**
 * @brief Gets a set of values separated by a whitespace from a string
 * @param src Source containing the numeric values
 * @param a First numeric value
 * @param b Second numeric value
 * @retuns true if successful, else false
 */
bool
Get2DValue(const char *const src, size_t *const a, size_t *const b);

/**
 * @brief Trimps the start of a string by a specified offset
 * @param src Original string to edit
 * @param dest Where the output will be saved to
 * @param offset offset to trimp up to
 * @returns True if successful, else false
 */
bool
TrimStart(const char *const src, char *const dest, const size_t offset);

/**
 * @brief Hashes the given string into a number
 * @param src String to be hashed
 * @returns The hashed string
 */
size_t
GetHashFromString(const char *const src,
                  const size_t prime,
                  const size_t modulus);

/**
 * @brief Search and replaces all chars in a string
 * @param srcDest Source string to be edited
 * @param target Delimiter to search for
 * @param replace Char to replace the delimiter
 */
void
ReplaceChars(char *const srcDest, const char target, const char replace);

/**
 * @brief Gets a substring of a string by its start and end indexes
 * @pram src Source string
 * @param dest Destination string
 * @param start Start index
 * @param end End index
 */
void
GetSubstring(const char *const src,
             char *const dest,
             const size_t start,
             const size_t end);

/**
 * @brief Gets the value of a meta tag as a string
 * @param filenaem Path to the file
 * @param src Name of the tag to search for
 * @param dest Destination string to save the result to
 * @returns Whether the function was successful or not
 */
bool
GetMetaTagValue(const char *const content,
                const char *const src,
                char *const dest,
                const size_t destLen);

#endif
