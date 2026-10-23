#include "fetch.h"
#include <curl/curl.h>

// Private members

static size_t
write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
        return fwrite(ptr, size, nmemb, stream);
}

static int
progress_func(void *data,
              curl_off_t totalDownload,
              curl_off_t downloaded,
              curl_off_t,
              curl_off_t)
{
        if (!data)
        {
                return 0;
        }

        void (*cb)(double, double) = (void (*)(double, double))data;

        cb((double)totalDownload, (double)downloaded);

        return 0;
}

// Public members

size_t
fetch_download_file(void (*cb)(double, double),
                    const string filename,
                    const string ca_cert,
                    const string url)
{
        FILE *file = fopen(filename, "wb");
        if (!file)
        {

                fprintf(stderr, "Failed open model file for writing\n");
                return 1;
        }

        CURL *cURL = curl_easy_init();
        if (!cURL)
        {
                fprintf(stderr, "Failed to init cURL to download model\n");
                fclose(file);
                return 1;
        }

        curl_easy_setopt(cURL, CURLOPT_CAINFO, ca_cert);
        curl_easy_setopt(cURL, CURLOPT_URL, url);
        curl_easy_setopt(cURL, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(cURL, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(cURL, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(cURL, CURLOPT_WRITEDATA, file);

        if (cb)
        {
                curl_easy_setopt(cURL, CURLOPT_NOPROGRESS, FALSE);
                curl_easy_setopt(cURL, CURLOPT_XFERINFOFUNCTION, progress_func);
                curl_easy_setopt(cURL, CURLOPT_XFERINFODATA, cb);
        }

        const CURLcode res = curl_easy_perform(cURL);
        ssize_t code = 0;
        curl_easy_getinfo(cURL, CURLINFO_RESPONSE_CODE, &code);

        curl_easy_cleanup(cURL);
        fclose(file);

        return res == CURLE_OK && code < 400 ? 0 : code;
}
