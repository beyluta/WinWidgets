#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include <stddef.h>

typedef struct
{
  char *data;
  size_t length;
} string_builder_t;

string_builder_t *string_builder_new_empty(const size_t bytes);
string_builder_t *string_builder_new(const char *const string);
void string_builder_free(string_builder_t *struc);
string_builder_t *string_builder_concat(const string_builder_t *const struc, const char *const str);
string_builder_t *string_builder_slice(const string_builder_t *const struc, size_t start, size_t end);

#endif
