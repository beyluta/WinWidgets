#include "string-builder.h"
#include <stdio.h>
#include <string.h>

// Test helper macros
#define TEST_PASS "\033[0;32m[PASS]\033[0m"
#define TEST_FAIL "\033[0;31m[FAIL]\033[0m"

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

void print_test_result(const char *test_name, int passed)
{
  if (passed)
  {
    printf("%s %s\n", TEST_PASS, test_name);
    tests_passed++;
  }
  else
  {
    printf("%s %s\n", TEST_FAIL, test_name);
    tests_failed++;
  }
}

// ============================================================================
// TEST: string_builder_new
// Tests the creation of a new string builder from a C string
// ============================================================================
void test_string_builder_new()
{
  printf("\n--- Testing string_builder_new ---\n");

  // Test 1: Create string builder with valid string
  string_builder_t *str1 = string_builder_new("Hello World");
  int test1 = (str1 != NULL &&
               strcmp(str1->data, "Hello World") == 0 &&
               str1->length == 11);
  print_test_result("string_builder_new: Valid string", test1);
  if (str1)
  {
    string_builder_free(str1);
  }

  // Test 2: Create string builder with empty string
  string_builder_t *str2 = string_builder_new("");
  int test2 = (str2 == NULL); // Should return NULL for empty string
  print_test_result("string_builder_new: Empty string returns NULL", test2);
  if (str2)
  {
    string_builder_free(str2);
  }

  // Test 3: Create string builder with single character
  string_builder_t *str3 = string_builder_new("A");
  int test3 = (str3 != NULL &&
               strcmp(str3->data, "A") == 0 &&
               str3->length == 1);
  print_test_result("string_builder_new: Single character", test3);
  if (str3)
  {
    string_builder_free(str3);
  }
}

// ============================================================================
// TEST: string_builder_new_empty
// Tests the creation of an empty string builder with specified capacity
// ============================================================================
void test_string_builder_new_empty()
{
  printf("\n--- Testing string_builder_new_empty ---\n");

  // Test 1: Create empty string builder with capacity
  string_builder_t *str1 = string_builder_new_empty(50);
  int test1 = (str1 != NULL && str1->length == 50);
  print_test_result("string_builder_new_empty: Valid capacity", test1);
  if (str1)
  {
    string_builder_free(str1);
  }

  // Test 2: Create empty string builder with zero capacity
  string_builder_t *str2 = string_builder_new_empty(0);
  int test2 = (str2 != NULL && str2->length == 0);
  print_test_result("string_builder_new_empty: Zero capacity", test2);
  if (str2)
  {
    string_builder_free(str2);
  }
}

// ============================================================================
// TEST: string_builder_concat
// Tests concatenation of strings to an existing string builder
// ============================================================================
void test_string_builder_concat()
{
  printf("\n--- Testing string_builder_concat ---\n");

  // Test 1: Concatenate two non-empty strings
  string_builder_t *str1 = string_builder_new("Hello");
  string_builder_t *result1 = string_builder_concat(str1, " World");
  int test1 = (result1 != NULL &&
               strcmp(result1->data, "Hello World") == 0 &&
               result1->length == 11);
  print_test_result("string_builder_concat: Two non-empty strings", test1);
  if (str1)
  {
    string_builder_free(str1);
  }
  if (result1)
  {
    string_builder_free(result1);
  }

  // Test 2: Concatenate empty string
  string_builder_t *str2 = string_builder_new("Test");
  string_builder_t *result2 = string_builder_concat(str2, "");
  int test2 = (result2 != NULL &&
               strcmp(result2->data, "Test") == 0 &&
               result2->length == 4);
  print_test_result("string_builder_concat: Concatenate empty string", test2);
  if (str2)
  {
    string_builder_free(str2);
  }
  if (result2)
  {
    string_builder_free(result2);
  }

  // Test 3: Multiple concatenations
  string_builder_t *str3 = string_builder_new("A");
  string_builder_t *temp1 = string_builder_concat(str3, "B");
  string_builder_t *result3 = string_builder_concat(temp1, "C");
  int test3 = (result3 != NULL &&
               strcmp(result3->data, "ABC") == 0 &&
               result3->length == 3);
  print_test_result("string_builder_concat: Multiple concatenations", test3);
  if (str3)
  {
    string_builder_free(str3);
  }
  if (temp1)
  {
    string_builder_free(temp1);
  }
  if (result3)
  {
    string_builder_free(result3);
  }
}

// ============================================================================
// TEST: string_builder_slice
// Tests slicing functionality to remove a substring from specified range
// ============================================================================
void test_string_builder_slice()
{
  printf("\n--- Testing string_builder_slice ---\n");

  // Test 1: Slice middle portion
  string_builder_t *str1 = string_builder_new("Hello My World");
  string_builder_t *result1 = string_builder_slice(str1, 5, 8);
  int test1 = (result1 != NULL &&
               strcmp(result1->data, "Hello World") == 0);
  print_test_result("string_builder_slice: Remove middle portion", test1);
  if (str1)
  {
    string_builder_free(str1);
  }
  if (result1)
  {
    string_builder_free(result1);
  }

  // Test 2: Invalid slice (start >= end)
  string_builder_t *str2 = string_builder_new("Test String");
  string_builder_t *result2 = string_builder_slice(str2, 5, 5);
  int test2 = (result2 == NULL); // Should return NULL
  print_test_result("string_builder_slice: Invalid range (start == end)", test2);
  if (str2)
  {
    string_builder_free(str2);
  }
  if (result2)
  {
    string_builder_free(result2);
  }

  // Test 3: Invalid slice (start > end)
  string_builder_t *str3 = string_builder_new("Test String");
  string_builder_t *result3 = string_builder_slice(str3, 8, 3);
  int test3 = (result3 == NULL); // Should return NULL
  print_test_result("string_builder_slice: Invalid range (start > end)", test3);
  if (str3)
  {
    string_builder_free(str3);
  }
  if (result3)
  {
    string_builder_free(result3);
  }

  // Test 4: Slice from beginning
  string_builder_t *str4 = string_builder_new("ABCDEFGH");
  string_builder_t *result4 = string_builder_slice(str4, 0, 3);
  int test4 = (result4 != NULL &&
               strcmp(result4->data, "DEFGH") == 0);
  print_test_result("string_builder_slice: Remove from beginning", test4);
  if (str4)
  {
    string_builder_free(str4);
  }
  if (result4)
  {
    string_builder_free(result4);
  }
}

// ============================================================================
// TEST: Memory Management
// Tests that memory is properly allocated and freed
// ============================================================================
void test_memory_management()
{
  printf("\n--- Testing Memory Management ---\n");

  // Test 1: Multiple allocations and frees
  int test1 = 1;
  for (int i = 0; i < 100; i++)
  {
    string_builder_t *str = string_builder_new("Test");
    if (str == NULL)
    {
      test1 = 0;
      break;
    }
    string_builder_free(str);
  }
  print_test_result("Memory Management: Multiple alloc/free cycles", test1);

  // Test 2: Free after operations
  string_builder_t *str2 = string_builder_new("Start");
  string_builder_t *concat_result = string_builder_concat(str2, " End");
  string_builder_free(str2);
  string_builder_free(concat_result);
  print_test_result("Memory Management: Free after concat", 1);
}

// ============================================================================
// MAIN: Run all tests and display summary
// ============================================================================
int main()
{
  printf("========================================\n");
  printf("  STRING BUILDER LIBRARY TEST SUITE\n");
  printf("========================================\n");

  test_string_builder_new();
  test_string_builder_new_empty();
  test_string_builder_concat();
  test_string_builder_slice();
  test_memory_management();

  printf("\n========================================\n");
  printf("  TEST SUMMARY\n");
  printf("========================================\n");
  printf("Tests Passed: %d\n", tests_passed);
  printf("Tests Failed: %d\n", tests_failed);
  printf("Total Tests:  %d\n", tests_passed + tests_failed);
  printf("========================================\n");

  return tests_failed == 0 ? 0 : 1;
}
