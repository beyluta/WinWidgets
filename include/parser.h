#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stddef.h>

/**
 * @brief Used to get meta tag values from inside a HTML file loaded into
 * memory.
 *
 * @param src Raw content of the HTML page
 * @param srcLen Length of the raw source string
 * @param target Name of the meta tag searched for
 * @param dest Destination string where the results will be saved to
 * @param destLen Max length of the destination string
 * @returns true on success, else false on failure
 */
bool
ww_begin_parsing(const char *const src,
                 const size_t srcLen,
                 const char *const target,
                 char *const dest,
                 const size_t destLen);

#endif
