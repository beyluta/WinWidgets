#ifndef FETCH_H
#define FETCH_H

#include "utils.h"

/**
 * @brief Function to download a resource from a remote resource
 * @param cb Pointer to a callback function for the progress; may be null
 * @param filename Name of the file to write to disk
 * @param ca_cert Path to a valid certificate authority
 * @param url Remote resource URL to download
 * @return HTTP code with the result of the operation
 */
size_t
fetch_download_file(void (*cb)(double, double),
                    const string filename,
                    const string ca_cert,
                    const string url);

#endif
