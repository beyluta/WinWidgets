#include <limits.h>
#include <string.h>
#include <stdio.h>
#include "cyaml.h"
#include "global.h"
#include "filesystem.h"
#include "utils.h"

static constexpr char RES_FILE_NAME[] = "resources.yaml";

ssize_t
ww_read_resource_string(const string s, string dest, const size_t max)
{
        yaml_s *yaml = nullptr;
        yaml_node_s *remoteUrl = nullptr;
        ssize_t bytes;

        char appPath[PATH_MAX];
        const ssize_t err =
                ww_get_executable_path(appPath, sizeof(appPath) - 1);
        if (err != EXIT_REASON_TERMINATED)
        {
                bytes = -1;
                goto cleanup;
        }

        char rootDir[PATH_MAX];
        if (!ww_dir_up(appPath, strlen(appPath), rootDir, sizeof(rootDir)))
        {
                bytes = -1;
                goto cleanup;
        }

        char absPath[PATH_MAX];
        if (strlen(rootDir) + sizeof(RES_FILE_NAME) + 1 >= sizeof(absPath))
        {
                bytes = -1;
                goto cleanup;
        }

        bytes = snprintf(
                absPath, sizeof(absPath), "%s\\%s", rootDir, RES_FILE_NAME);
        if (bytes <= 0)
        {
                bytes = -1;
                goto cleanup;
        }

        if ((bytes = ww_get_file_bytes(absPath)) < 1)
        {
                bytes = -1;
                goto cleanup;
        }

        char yamlFile[BUFFSIZE];
        if (ww_get_file_content(absPath, yamlFile, sizeof(yamlFile) - 1))
        {
                bytes = -1;
                goto cleanup;
        }

        if ((yaml = yaml_load(yamlFile, strlen(yamlFile))) == nullptr)
        {
                bytes = -1;
                goto cleanup;
        }

        if ((remoteUrl = yaml_get_node(yaml_root_node(yaml), s)) == nullptr)
        {
                bytes = -1;
                goto cleanup;
        }

        if ((bytes = yaml_get_primitive(remoteUrl, dest, max)) < 1)
        {
                bytes = -1;
                goto cleanup;
        }
        dest[bytes - 1] = '\0';

cleanup:
        if (yaml != nullptr)
        {
                yaml_free(yaml);
        }

        return bytes;
}
