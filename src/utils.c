#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
Get2DValue(const char *const src, size_t *const a, size_t *const b)
{
        char substrA[512], substrB[512];
        const size_t size = strlen(src);
        for (size_t i = 0; i < size; i++)
        {
                const char c = src[i];
                if (c == widget_char_space)
                {
                        memcpy(substrA, src, i);
                        substrA[i] = '\0';

                        const size_t substrBLen = size - i - 1;
                        memcpy(substrB, &src[i + 1], substrBLen);
                        substrB[substrBLen] = '\0';

                        *a = strtol(substrA, nullptr, 10);
                        *b = strtol(substrB, nullptr, 10);

                        return true;
                }
        }

        return false;
}

bool
TrimStart(const char *const src, char *const dest, const size_t offset)
{
        const size_t len = strlen(src);
        if (snprintf(dest, (len - offset) + 1, "%s", &src[offset]) < 0)
        {
                fprintf(stderr, "Failed to copy string into buffer\n");
                return false;
        }
        return true;
}

size_t
GetHashFromString(const char *const src,
                  const size_t prime,
                  const size_t modulus)
{
        size_t hash = prime;
        const size_t length = strlen(src);
        for (size_t i = 0; i < length; i++)
        {
                hash = ((hash << 5) + hash) + src[i];
        }
        return hash % modulus;
}

void
ReplaceChars(char *const srcDest, const char target, const char replace)
{
        const size_t len = strlen(srcDest);
        for (size_t i = 0; i < len; i++)
        {
                srcDest[i] = srcDest[i] == target ? replace : srcDest[i];
        }
}

void
GetSubstring(const char *const src,
             char *const dest,
             const size_t start,
             const size_t end)
{
        const size_t maxSize = (end - start) + 1;
        memcpy(dest, &src[start], maxSize);
        dest[maxSize] = '\0';
}
