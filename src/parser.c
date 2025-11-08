#include <stdio.h>
#include <string.h>

static constexpr char META_TOKEN_TAG[] = "meta";
static constexpr char META_TOKEN_NAME[] = "name";
static constexpr char META_TOKEN_VALUE[] = "value";

static size_t
parse_delimited_tokens_from(const char *const src,
                            const size_t srcLen,
                            const size_t start,
                            const char delim)
{
        size_t i = start, j = 0;

        do
        {
                if (i >= srcLen)
                {
                        return start;
                }

                if (src[i] == delim)
                {
                        j++;
                }

                i++;

        } while (j < 2);

        return i - 1;
}

static size_t
parse_tokens_from(const char *const src,
                  const size_t srcLen,
                  const size_t start,
                  const char *const token,
                  const size_t tokenLen,
                  const char *const target,
                  char *const dest,
                  const size_t destLen,
                  bool *const success,
                  const char EOL)
{
        size_t i = start, j = 0;

        do
        {
                if (i >= srcLen)
                {
                        return start;
                }

                if (j >= tokenLen)
                {
                        const size_t from = i + 1;
                        i = parse_delimited_tokens_from(src, srcLen, from, '"');

                        char name[BUFSIZ];
                        memcpy(name, &src[from + 1], i - from);
                        name[i - from - 1] = '\0';

                        if (target != nullptr)
                        {
                                if (strcmp(name, target) == 0)
                                {
                                        i = parse_tokens_from(
                                                src,
                                                srcLen,
                                                i + 1,
                                                META_TOKEN_VALUE,
                                                (sizeof(META_TOKEN_VALUE) /
                                                 sizeof(META_TOKEN_VALUE[0])) -
                                                        1,
                                                nullptr,
                                                dest,
                                                destLen,
                                                success,
                                                EOL);
                                }
                        }
                        else
                        {
                                if (i - from < destLen)
                                {
                                        memcpy(dest, &src[from + 1], i - from);
                                        dest[i - from - 1] = '\0';
                                        *success = true;
                                }
                        }
                }

                if (j > 0 && src[i] != token[j])
                {
                        j = 0;
                }

                if (src[i] == token[j])
                {
                        j++;
                }

        } while (src[i++] != EOL);

        return i - 1;
}

bool
ww_begin_parsing(const char *const src,
                 const size_t srcLen,
                 const char *const target,
                 char *const dest,
                 const size_t destLen)
{
        for (size_t i = 0, j = 0; i < srcLen; i++)
        {
                if (j >=
                    (sizeof(META_TOKEN_TAG) / sizeof(META_TOKEN_TAG[0]) - 1))
                {
                        bool success = false;
                        const size_t from = i + 1;
                        i = parse_tokens_from(src,
                                              srcLen,
                                              from,
                                              META_TOKEN_NAME,
                                              (sizeof(META_TOKEN_NAME) /
                                               sizeof(META_TOKEN_NAME[0])) -
                                                      1,
                                              target,
                                              dest,
                                              destLen,
                                              &success,
                                              '>');
                        if (success)
                        {
                                return true;
                        }
                }

                if (j > 0 && src[i] != META_TOKEN_TAG[j])
                {
                        j = 0;
                }

                if (src[i] == META_TOKEN_TAG[j])
                {
                        j++;
                }
        }

        return false;
}
