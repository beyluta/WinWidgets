#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
Get2DValue(const char *const src, size_t *const a, size_t *const b)
{
        char substrA[512], substrB[512];
        const size_t size = strlen(src);
        size_t spaceIndex = 0;
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

bool
GetMetaTagValue(const char *const content,
                const char *const src,
                char *const dest,
                const size_t destLen)
{
        const size_t len = strlen(content);
        bool isTarget = false;
        bool isMeta = false;
        bool isName = false;
        bool isValue = false;
        ssize_t start = -1, end = -1;
        const char *const metaTag = "<meta";
        const char *const nameAttr = "name=";
        const char *const valueAttr = "value=";
        const size_t metaTagLen = strlen(metaTag);
        const size_t nameAttrLen = strlen(nameAttr);
        const size_t valueAttrLen = strlen(valueAttr);
        for (size_t i = 0, j = 0; i < len; i++)
        {
                const char c = content[i];

                // Put values between start and end inside this variable
                char value[destLen];
                size_t valueLen = 0;
                if (start > -1 && end > -1)
                {
                        valueLen = end - start;
                        memcpy(value, &content[start], valueLen);
                        value[valueLen] = '\0';
                }

                // Searching for the Meta tag
                if (c == metaTag[j] && !isMeta)
                {
                        j++;
                }

                if (j >= metaTagLen && !isMeta)
                {
                        j = 0;
                        isMeta = true;
                        continue;
                }

                if (!isMeta)
                {
                        continue;
                }

                // Searching for meta close tag then resetting all flags
                if (c == widget_char_gt)
                {
                        isTarget = false;
                        isMeta = false;
                        isName = false;
                        isValue = false;
                        start = -1;
                        end = -1;
                        continue;
                }

                // Searching for the name attribute
                if (c == nameAttr[j] && !isName)
                {
                        j++;
                }

                if (j >= nameAttrLen && !isName)
                {
                        j = 0;
                        isName = true;
                        continue;
                }

                if (!isName)
                {
                        continue;
                }

                // Getting the value of name between quotes
                if (c == widget_char_quote && start < 0 && !isTarget)
                {
                        start = i + 1;
                        continue;
                }

                if (c == widget_char_quote && end < 0 && !isTarget)
                {
                        end = i;
                        continue;
                }

                if ((start < 0 || end < 0) && !isTarget)
                {
                        continue;
                }

                if (valueLen > 0 && !isTarget)
                {
                        isTarget = strcmp(value, src) == 0;
                        j = 0;
                        start = -1;
                        end = -1;
                        valueLen = 0;
                        continue;
                }

                // Searching for the value attribute
                if (c == valueAttr[j] && !isValue)
                {
                        j++;
                }

                if (j >= valueAttrLen && !isValue)
                {
                        j = 0;
                        isValue = true;
                        continue;
                }

                if (!isValue)
                {
                        continue;
                }

                // Getting the value of the attribute "value" between quotes
                if (c == widget_char_quote && start < 0)
                {
                        start = i + 1;
                        continue;
                }

                if (c == widget_char_quote && end < 0)
                {
                        end = i;
                        continue;
                }

                if (valueLen <= 0)
                {
                        continue;
                }

                memcpy(dest, value, destLen);
                dest[destLen] = '\0';

                return true;
        }

        return false;
}
