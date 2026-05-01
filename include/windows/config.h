#ifndef CONFIG_H
#define CONFIG_H

#include "stdint.h"
#include "utils.h"

/**
 * @brief Reads the string-value of a key from the default YAML file
 * @param s Name of the key to read the value from
 * @param dest Destination string to save the value to
 * @param max Maximum size of the destination string
 * @returns The number of bytes written to dest; else -1 on failure
 */
ssize_t
ww_read_resource_string(const string s, string dest, const size_t max);

#endif
