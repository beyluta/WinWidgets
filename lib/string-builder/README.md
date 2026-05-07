# String Builder Library

A lightweight, efficient C library for dynamic string manipulation with automatic memory management and alignment optimization.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [API Reference](#api-reference)
  - [Data Structures](#data-structures)
  - [Functions](#functions)
- [Usage Examples](#usage-examples)
- [Memory Management](#memory-management)
- [Building and Testing](#building-and-testing)
- [Performance Considerations](#performance-considerations)
- [License](#license)

## Overview

The String Builder library provides a robust solution for building and manipulating strings in C. Unlike standard C strings, this library manages memory automatically and optimizes allocation through alignment strategies, reducing fragmentation and improving performance for string operations.

## Features

- **Dynamic Memory Management**: Automatically handles memory allocation and reallocation
- **Memory Alignment**: Uses 255-byte alignment boundaries for efficient memory usage
- **Safe String Operations**: All functions include NULL checks and validation
- **Immutable Operations**: Operations return new instances, preserving original strings
- **String Concatenation**: Efficiently join multiple strings together
- **String Slicing**: Remove portions of strings by index range
- **Lightweight**: Minimal overhead with straightforward API

## API Reference

### Data Structures

#### `string_builder_t`

The main structure representing a dynamic string.

```c
typedef struct {
    char *data;      // Pointer to the string data
    size_t length;   // Length of the string in bytes
} string_builder_t;
```

**Fields:**

- `data`: Null-terminated C string containing the actual character data
- `length`: Number of characters in the string (excluding null terminator)

### Functions

#### `string_builder_new`

Creates a new string builder from an existing C string.

```c
string_builder_t *string_builder_new(const char *const string);
```

**Parameters:**

- `string`: Source C string to initialize the builder with

**Returns:**

- Pointer to newly allocated `string_builder_t` on success
- `NULL` if the string is empty or memory allocation fails

**Example:**

```c
string_builder_t *str = string_builder_new("Hello World");
if (str != NULL) {
    printf("%s\n", str->data);  // Prints: Hello World
    string_builder_free(str);
}
```

---

#### `string_builder_new_empty`

Creates an empty string builder with specified capacity.

```c
string_builder_t *string_builder_new_empty(const size_t bytes);
```

**Parameters:**

- `bytes`: Desired capacity in bytes (actual allocation may be larger due to alignment)

**Returns:**

- Pointer to newly allocated `string_builder_t` on success
- `NULL` if memory allocation fails

**Example:**

```c
string_builder_t *str = string_builder_new_empty(100);
if (str != NULL) {
    // str->length will be 100
    // Actual allocated space will be aligned to 255-byte boundaries
    string_builder_free(str);
}
```

---

#### `string_builder_concat`

Concatenates a C string to an existing string builder, creating a new instance.

```c
string_builder_t *string_builder_concat(const string_builder_t *const builder, 
                                       const char *const str);
```

**Parameters:**

- `builder`: Source string builder to concatenate to
- `str`: C string to append

**Returns:**

- Pointer to new `string_builder_t` containing concatenated result
- `NULL` if memory allocation fails

**Important:** The original `builder` is NOT modified. This function returns a new instance.

**Example:**

```c
string_builder_t *str1 = string_builder_new("Hello");
string_builder_t *str2 = string_builder_concat(str1, " World");

printf("%s\n", str2->data);  // Prints: Hello World

string_builder_free(str1);
string_builder_free(str2);
```

---

#### `string_builder_slice`

Removes a substring from the specified index range, creating a new instance.

```c
string_builder_t *string_builder_slice(const string_builder_t *const struc, 
                                      size_t start, 
                                      size_t end);
```

**Parameters:**

- `struc`: Source string builder to slice
- `start`: Starting index of the range to remove (inclusive)
- `end`: Ending index of the range to remove (exclusive)

**Returns:**

- Pointer to new `string_builder_t` with specified range removed
- `NULL` if `start >= end` or memory allocation fails

**Important:** The original `struc` is NOT modified. This function returns a new instance.

**Example:**

```c
string_builder_t *str1 = string_builder_new("Hello My World");
// Remove " My" (indices 5-8)
string_builder_t *str2 = string_builder_slice(str1, 5, 8);

printf("%s\n", str2->data);  // Prints: Hello World

string_builder_free(str1);
string_builder_free(str2);
```

---

#### `string_builder_free`

Frees memory allocated for a string builder.

```c
void string_builder_free(string_builder_t *struc);
```

**Parameters:**

- `struc`: String builder to deallocate

**Important:** Always call this function to prevent memory leaks. After calling, do not use the pointer.

**Example:**

```c
string_builder_t *str = string_builder_new("Example");
// ... use str ...
string_builder_free(str);
// str is now invalid, do not use
```

## Usage Examples

### Basic String Creation

```c
#include "string-builder.h"
#include <stdio.h>

int main() {
    // Create a string builder
    string_builder_t *greeting = string_builder_new("Hello");
    
    if (greeting != NULL) {
        printf("String: %s\n", greeting->data);
        printf("Length: %zu\n", greeting->length);
        string_builder_free(greeting);
    }
    
    return 0;
}
```

### String Concatenation

```c
#include "string-builder.h"
#include <stdio.h>

int main() {
    string_builder_t *first = string_builder_new("Hello");
    string_builder_t *second = string_builder_concat(first, " ");
    string_builder_t *third = string_builder_concat(second, "World");
    
    printf("%s\n", third->data);  // Output: Hello World
    
    // Clean up
    string_builder_free(first);
    string_builder_free(second);
    string_builder_free(third);
    
    return 0;
}
```

### String Slicing

```c
#include "string-builder.h"
#include <stdio.h>

int main() {
    string_builder_t *original = string_builder_new("ABCDEFGH");
    
    // Remove "BCD" (indices 1-4)
    string_builder_t *sliced = string_builder_slice(original, 1, 4);
    
    printf("Original: %s\n", original->data);  // Output: ABCDEFGH
    printf("Sliced: %s\n", sliced->data);      // Output: AEFGH
    
    string_builder_free(original);
    string_builder_free(sliced);
    
    return 0;
}
```

### Building Complex Strings

```c
#include "string-builder.h"
#include <stdio.h>

int main() {
    // Build a sentence
    string_builder_t *s1 = string_builder_new("The");
    string_builder_t *s2 = string_builder_concat(s1, " quick");
    string_builder_t *s3 = string_builder_concat(s2, " brown");
    string_builder_t *s4 = string_builder_concat(s3, " fox");
    
    printf("%s\n", s4->data);  // Output: The quick brown fox
    
    // Clean up all intermediate strings
    string_builder_free(s1);
    string_builder_free(s2);
    string_builder_free(s3);
    string_builder_free(s4);
    
    return 0;
}
```

## Memory Management

### Alignment Strategy

The library uses a 255-byte (`0xFF`) alignment boundary for memory allocation. This means:

- Memory is allocated in multiples of 255 bytes plus 1 byte for null terminator
- For a string of length `n`, the multiplier is calculated as `n / 255` (minimum 1)
- This reduces memory fragmentation and improves cache performance

**Example:**

- String length 10: Allocates 256 bytes (255 × 1 + 1)
- String length 300: Allocates 511 bytes (255 × 2 + 1)
- String length 1000: Allocates 1021 bytes (255 × 4 + 1)

### Important Memory Considerations

1. **Always Free Resources**: Every `string_builder_new*` or operation that returns a new builder must be freed with `string_builder_free`

2. **Immutable Operations**: Functions like `concat` and `slice` return NEW instances. The original remains unchanged and must be freed separately.

3. **NULL Checks**: Always check return values for NULL before using them

4. **No Dangling Pointers**: After calling `string_builder_free`, do not use that pointer

### Example of Proper Memory Management

```c
string_builder_t *str1 = string_builder_new("Hello");
if (str1 == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return 1;
}

string_builder_t *str2 = string_builder_concat(str1, " World");
if (str2 == NULL) {
    fprintf(stderr, "Concatenation failed\n");
    string_builder_free(str1);  // Clean up what we allocated
    return 1;
}

// Use str2...
printf("%s\n", str2->data);

// Free both instances
string_builder_free(str1);
string_builder_free(str2);
```

## Building and Testing

### Compile the Test Suite

The project includes a comprehensive test suite in `main.c`:

```bash
make test
```

### Test Coverage

The test suite covers:

- Creating string builders from C strings
- Creating empty string builders with capacity
- String concatenation (single and multiple)
- String slicing (various ranges)
- Memory management and leak prevention
- Edge cases (empty strings, NULL handling)

### Expected Output

```
========================================
  STRING BUILDER LIBRARY TEST SUITE
========================================

--- Testing string_builder_new ---
[PASS] string_builder_new: Valid string
[PASS] string_builder_new: Empty string returns NULL
[PASS] string_builder_new: Single character

--- Testing string_builder_new_empty ---
[PASS] string_builder_new_empty: Valid capacity
[PASS] string_builder_new_empty: Zero capacity

--- Testing string_builder_concat ---
[PASS] string_builder_concat: Two non-empty strings
[PASS] string_builder_concat: Concatenate empty string
[PASS] string_builder_concat: Multiple concatenations

--- Testing string_builder_slice ---
[PASS] string_builder_slice: Remove middle portion
[PASS] string_builder_slice: Invalid range (start == end)
[PASS] string_builder_slice: Invalid range (start > end)
[PASS] string_builder_slice: Remove from beginning

--- Testing Memory Management ---
[PASS] Memory Management: Multiple alloc/free cycles
[PASS] Memory Management: Free after concat

========================================
  TEST SUMMARY
========================================
Tests Passed: 14
Tests Failed: 0
Total Tests:  14
========================================
```

## Performance Considerations

### Best Practices

1. **Pre-allocate when possible**: If you know the approximate final size, use `string_builder_new_empty` to minimize reallocations

2. **Minimize intermediate strings**: For multiple concatenations, consider the memory cost of keeping intermediate results

3. **Batch operations**: Plan your string operations to minimize the number of allocations

### Performance Characteristics

- **Memory allocation**: O(1) amortized due to alignment strategy
- **Concatenation**: O(n + m) where n and m are string lengths
- **Slicing**: O(n) where n is the resulting string length
- **Free**: O(1)

---

**Note**: This library is designed for educational and practical use in C projects requiring dynamic string manipulation. For production use, ensure thorough testing with your specific use cases.
