// ======================= Purpose ==========================
// Contains functions that can connect to remote endpoints to
// make HTTP requests via cURL.
// ==========================================================
#include <curl/curl.h>
#include <curl/easy.h>
#include <limits.h>
#include <zip.h>
#include "remres.h"
#include "filesystem.h"
#include "utils.h"

constexpr char ARCHIVE_FILENAME[] = "def_widgets.zip";

static size_t
ww_write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
        return fwrite(ptr, size, nmemb, stream);
}

void
ww_unzip_archive(const string s)
{
        zip_t *zip = nullptr;
        zip_file_t *fd = nullptr;
        FILE *fp = nullptr;
        char *buffer = nullptr;

        if ((zip = zip_open(s, 0, nullptr)) == nullptr)
        {
                goto cleanup;
        }

        zip_stat_t stat;
        zip_stat_init(&stat);

        size_t count = 0;
        while (zip_stat_index(zip, count, 0, &stat) == 0)
        {
                if ((buffer = (char *)malloc(sizeof(char) * stat.size)) ==
                    nullptr)
                {
                        goto cleanup;
                }

                if ((fd = zip_fopen_index(zip, count, 0)) == nullptr)
                {
                        goto cleanup;
                }

                if (zip_fread(fd, buffer, stat.size) < 0)
                {
                        goto cleanup;
                }

                char path_buff[PATH_MAX];
                ww_default_widgets_dir(path_buff);

                char res_buff[PATH_MAX];
                if (snprintf(res_buff,
                             sizeof(res_buff) - 1,
                             "%s/%s",
                             path_buff,
                             stat.name) < 0)
                {
                        goto cleanup;
                }

                if ((fp = fopen(res_buff, "wb")) == nullptr)
                {
                        goto cleanup;
                }

                if (fwrite(buffer, sizeof(char), stat.size, fp) < 1)
                {
                        goto cleanup;
                }

                fclose(fp);
                fp = nullptr;

                zip_fclose(fd);
                fd = nullptr;

                free(buffer);
                buffer = nullptr;

                count++;
        }

cleanup:
        if (buffer != nullptr)
        {
                free(buffer);
        }

        if (fp != nullptr)
        {
                fclose(fp);
        }

        if (fd != nullptr)
        {
                zip_fclose(fd);
        }

        if (zip != nullptr)
        {
                zip_close(zip);
        }
}

void
ww_get_default_resource_from_remote(const string remoteUrl,
                                    const string def_dir,
                                    string dest,
                                    const size_t max)
{
        CURL *curl = nullptr;
        FILE *file = nullptr;

        if (snprintf(dest, max, "%s/%s", def_dir, ARCHIVE_FILENAME) < 0)
        {
                goto cleanup;
        }

        if ((curl = curl_easy_init()) == nullptr)
        {
                goto cleanup;
        }

        if ((file = fopen(dest, "wb")) == nullptr)
        {
                goto cleanup;
        }

        curl_easy_setopt(curl, CURLOPT_URL, remoteUrl);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ww_write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

        if (curl_easy_perform(curl) != CURLE_OK)
        {
                goto cleanup;
        }

        fclose(file);
        file = nullptr;

cleanup:
        if (file != nullptr)
        {
                fclose(file);
        }

        if (curl != nullptr)
        {
                curl_easy_cleanup(curl);
        }
}
