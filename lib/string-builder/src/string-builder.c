#include "string-builder.h"
#include <stdlib.h>
#include <string.h>

// Private members

#define STR_DATA_ALIGNMENT 0xFF

static size_t string_builder_alignment_multiplier(const size_t length)
{
  const size_t m = length / STR_DATA_ALIGNMENT;
  return m == 0 ? 1 : m;
}

// Public members

string_builder_t *string_builder_new_empty(const size_t bytes)
{
  string_builder_t *struc = (string_builder_t *)malloc(sizeof(string_builder_t));
  if (struc == NULL)
  {
    return NULL;
  }

  const size_t multiplier = string_builder_alignment_multiplier(bytes);

  char *data = (char *)malloc(sizeof(char) * ((STR_DATA_ALIGNMENT * multiplier) + 1));
  if (data == NULL)
  {
    free(struc);
    return NULL;
  }

  struc->data = data;
  struc->length = bytes;

  return struc;
}

string_builder_t *string_builder_new(const char *const string)
{
  const size_t length = strlen(string);
  if (length <= 0)
  {
    return NULL;
  }

  string_builder_t *str_ptr = (string_builder_t *)malloc(sizeof(string_builder_t));
  if (str_ptr == NULL)
  {
    return NULL;
  }

  const size_t multiplier = string_builder_alignment_multiplier(length);
  char *data = (char *)malloc(sizeof(char) * ((STR_DATA_ALIGNMENT * multiplier) + 1));
  if (data == NULL)
  {
    free(str_ptr);
    return NULL;
  }

  memcpy(data, string, length);
  data[length] = '\0';

  str_ptr->data = data;
  str_ptr->length = length;

  return str_ptr;
}

void string_builder_free(string_builder_t *struc)
{
  if (struc->data != NULL)
  {
    free(struc->data);
    struc->data = NULL;
  }

  free(struc);
  struc = NULL;
}

string_builder_t *string_builder_concat(const string_builder_t *const builder, const char *const str)
{
  const size_t length = strlen(str);
  const size_t multiplier1 = string_builder_alignment_multiplier(builder->length);
  const size_t multiplier2 = string_builder_alignment_multiplier(length);
  const size_t total_multiplier = multiplier1 + multiplier2;

  char *data = (char *)malloc(sizeof(char) * ((STR_DATA_ALIGNMENT * total_multiplier) + 1));
  if (data == NULL)
  {
    return NULL;
  }

  memcpy(data, builder->data, builder->length);
  memcpy(&data[builder->length], str, length);
  data[builder->length + length] = '\0';

  string_builder_t *string_builder = string_builder_new(data);
  if (string_builder == NULL)
  {
    free(data);
    return NULL;
  }

  free(data);

  return string_builder;
}

string_builder_t *string_builder_slice(const string_builder_t *const struc, size_t start, size_t end)
{
  if (start >= end)
  {
    return NULL;
  }

  const size_t length = struc->length - (end - start);
  string_builder_t *sliced_struc = string_builder_new_empty(length);
  if (sliced_struc == NULL)
  {
    return NULL;
  }

  strncpy(sliced_struc->data, struc->data, start);
  strncpy(&sliced_struc->data[start], &struc->data[end], length);
  sliced_struc->data[length] = '\0';

  return sliced_struc;
}
