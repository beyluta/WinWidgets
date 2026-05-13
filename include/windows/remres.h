#ifndef REMRES_H
#define REMRES_H

#include "utils.h"

/**
 * @brief Unzips the archive and writes the contents to the default
 * widgets directory.
 * @param s Absolute path to the archive file
 */
void
ww_unzip_archive(const string);

/**
 * @brief Downloads remote resources as a .zip file from remote
 * @param remoteUrl URL to download the archive from
 * @param dest Destination string to save the path of the archive to
 * @param max Maximum destination string length
 * @param filename Name of the file or path to the file on the server
 */
void
ww_get_default_resource_from_remote(const string remoteUrl,
                                    const string def_dir,
                                    string dest,
                                    const size_t max,
                                    const string filename);

#endif
