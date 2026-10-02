#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum ww_token_type
{
        WW_TOKEN_TYPE_IFBLOCK = 1,
        WW_TOKEN_TYPE_IFBLOCKEND = 2,
        WW_TOKEN_TYPE_HTMLSYNTAX = 4
};

struct ww_html_token
{
        char *data;
        size_t size;
        size_t ref;
};

struct ww_env_vars
{
        char **envv;
        size_t envc;
};

static void
printerr(const char *restrict const s)
{
        fprintf(stderr, "Error: %s\n", s);
        exit(1);
}

static struct ww_html_token *
ww_new_html_token(char *restrict const s, const size_t n)
{
        struct ww_html_token *result =
                (struct ww_html_token *)malloc(sizeof(struct ww_html_token));
        if (!result)
        {
                printerr("Allocating memory for html token struct");
        }

        result->data = s;
        result->size = n;
        result->ref = 1;

        return result;
}

static void
ww_html_token_ref_add(struct ww_html_token *restrict const s)
{
        s->ref++;
}

static void
ww_html_token_ref_sub(struct ww_html_token *restrict const s)
{
        if (!s)
        {
                return;
        }

        if (s->ref != 1)
        {
                s->ref--;
                return;
        }

        if (s->data)
        {

                free(s->data);
        }

        free(s);
}

static void
ww_free_html_tokens(struct ww_html_token **s, const size_t n)
{
        if (!s)
        {
                return;
        }

        for (size_t i = 0; i < n; i++)
        {
                ww_html_token_ref_sub(s[i]);
        }

        free(s);
}

static int
ww_html_token_is_delimiter(const char c)
{
        switch (c)
        {
        default:
                return 0;
        case ' ':
        case '\n':
                return 1;
        }
}

static size_t
ww_html_token_count(const char *restrict const s, const size_t n)
{
        size_t count = 0;
        for (size_t i = 0, j = 0; i < n; i++)
        {
                if ((!ww_html_token_is_delimiter(s[i]) || i == 0 ||
                     ww_html_token_is_delimiter(s[i - 1])) &&
                    i < n - 1)
                {
                        continue;
                }
                count++;
                j = i;
                do
                {
                        j++;
                } while (ww_html_token_is_delimiter(s[j]));
        }
        return count;
}

static struct ww_html_token **
ww_html_tokenize_begin(const char *restrict const s, size_t *restrict const n)
{
        FILE *file = fopen(s, "r");
        if (!file)
        {
                printerr("Opening file for tokenization");
        }

        fseek(file, 0L, SEEK_END);
        const size_t size = ftell(file);
        fseek(file, 0L, SEEK_SET);

        char *buff = (char *)malloc(sizeof(char) * (size + 1));
        if (!buff)
        {
                fclose(file);
                printerr("Allocating buffer for HTML file");
        }

        fread(buff, sizeof(char), size, file);
        buff[size] = 0;

        const size_t token_count = ww_html_token_count(buff, size);
        struct ww_html_token **token_arr = (struct ww_html_token **)malloc(
                sizeof(struct ww_html_token *) * token_count);
        *n = token_count;

        for (size_t i = 0, j = 0, k = 0; i < size; i++)
        {
                if ((!ww_html_token_is_delimiter(buff[i]) || i == 0 ||
                     ww_html_token_is_delimiter(buff[i - 1])) &&
                    i < size - 1)
                {
                        continue;
                }

                const size_t len = i - j;
                char *data = (char *)malloc(sizeof(char) * (len + 1));
                if (!data)
                {
                        free(token_arr);
                        free(buff);
                        fclose(file);
                        printerr("Allocating memory for buffer");
                }

                memcpy(data, &buff[j], len);
                data[len] = 0;

                struct ww_html_token *token = ww_new_html_token(data, len);
                if (!token)
                {
                        free(data);
                        free(token_arr);
                        free(buff);
                        fclose(file);
                        printerr("Allocating memory html token struct");
                }

                token_arr[k++] = token;

                j = i;
                do
                {
                        j++;
                } while (ww_html_token_is_delimiter(buff[j]));
        }

        free(buff);
        fclose(file);
        return token_arr;
}

static enum ww_token_type
ww_html_token_type(struct ww_html_token *restrict const s)
{
        if (strcmp(s->data, "#if") == 0)
        {
                return WW_TOKEN_TYPE_IFBLOCK;
        }

        if (strcmp(s->data, "#endif") == 0)
        {
                return WW_TOKEN_TYPE_IFBLOCKEND;
        }

        return WW_TOKEN_TYPE_HTMLSYNTAX;
}

static int
ww_html_env_vars_has(const char *restrict const s, struct ww_env_vars vars)
{
        for (size_t i = 0; i < vars.envc; i++)
        {
                if (strcmp(vars.envv[i], s) == 0)
                {
                        return 1;
                }
        }
        return 0;
}

static size_t
ww_html_tokens_size(struct ww_html_token *restrict const *restrict const s,
                    const size_t n)
{
        size_t sum = 0;
        for (size_t i = 0; i < n; i++)
        {
                if (!s[i])
                {
                        continue;
                }
                sum += s[i]->size + 1;
        }
        return sum - 1;
}

static void
ww_html_parse_begin(struct ww_html_token *restrict *restrict const s,
                    const size_t n,
                    struct ww_env_vars vars)
{
        int omitBlock = 0;

        for (size_t i = 0; i < n; i++)
        {
                const enum ww_token_type type = ww_html_token_type(s[i]);

                if (type == WW_TOKEN_TYPE_IFBLOCKEND)
                {
                        omitBlock = 0;
                        ww_html_token_ref_sub(s[i]);
                        s[i] = NULL;
                        continue;
                }

                if (i + 1 < n && type == WW_TOKEN_TYPE_IFBLOCK)
                {
                        const int hasVar =
                                ww_html_env_vars_has(s[i + 1]->data, vars);

                        ww_html_token_ref_sub(s[i]);
                        s[i] = NULL;
                        ww_html_token_ref_sub(s[i + 1]);
                        s[i + 1] = NULL;

                        i += 1;

                        if (!hasVar)
                        {
                                omitBlock = 1;
                        }

                        continue;
                }

                if (omitBlock)
                {
                        ww_html_token_ref_sub(s[i]);
                        s[i] = NULL;
                        continue;
                }
        }

        const size_t bytes = ww_html_tokens_size(s, n);
        char *data = (char *)malloc(sizeof(char) * (bytes + 1));
        if (!data)
        {
                printerr("Allocating memory for new html content");
        }

        size_t offset = 0;
        for (size_t i = 0; i < n; i++)
        {
                if (!s[i])
                {
                        continue;
                }

                memcpy(&data[offset], s[i]->data, s[i]->size);
                offset += s[i]->size + 1;

                if (i >= n - 1)
                {
                        data[offset - 1] = 0;
                        continue;
                }

                data[offset - 1] = ' ';
        }

        FILE *file = fopen("index.html", "w");
        if (!file)
        {
                free(data);
                printerr("Open new file");
        }

        fprintf(file, "%s", data);

        fclose(file);
        free(data);
}

int
main(int argc, char *argv[])
{
        if (argc < 3)
        {
                printerr("Correct usage: ./<SCRIPT> <PATH_TO_HTML_FILE> "
                         "<_WIN32 / __linux__>");
        }

        struct ww_env_vars vars = {.envv = argv, .envc = argc};

        size_t token_count;
        struct ww_html_token **token_arr =
                ww_html_tokenize_begin(argv[1], &token_count);

        ww_html_parse_begin(token_arr, token_count, vars);

        ww_free_html_tokens(token_arr, token_count);
}
