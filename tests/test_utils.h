/*
 * Background Save Plugin - Test Utilities
 * Copyright (c) 2024 钟芳道 (DJD)
 *
 * Common utilities and helper functions for tests
 */

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

// Test result codes
#define TEST_OK           0
#define TEST_FAIL         1
#define TEST_SKIP         2
#define TEST_ERROR        3

// Test configuration
#define TEST_MAX_PATH     512
#define TEST_MAX_NAME     128
#define TEST_MAX_MESSAGE  1024

// Test timing
typedef struct {
    struct timespec start;
    struct timespec end;
    double elapsed_ms;
} test_timer_t;

// Test assertion macros
#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "ASSERTION FAILED: %s at %s:%d\n", message, __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "ASSERTION FAILED: %s - Expected %ld, got %ld at %s:%d\n", \
                   message, (long)(expected), (long)(actual), __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual, message) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            fprintf(stderr, "ASSERTION FAILED: %s - Expected \"%s\", got \"%s\" at %s:%d\n", \
                   message, (expected), (actual), __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr, message) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "ASSERTION FAILED: %s - Pointer is NULL at %s:%d\n", \
                   message, __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

#define ASSERT_NULL(ptr, message) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "ASSERTION FAILED: %s - Pointer is not NULL at %s:%d\n", \
                   message, __FILE__, __LINE__); \
            return TEST_FAIL; \
        } \
    } while(0)

// Timing functions
void test_timer_start(test_timer_t *timer);
void test_timer_stop(test_timer_t *timer);
double test_timer_elapsed_ms(const test_timer_t *timer);

// File utilities
int test_file_exists(const char *path);
int test_file_size(const char *path, size_t *size);
int test_file_compare(const char *file1, const char *file2);
int test_file_copy(const char *src, const char *dst);
int test_file_remove(const char *path);
int test_dir_create(const char *path);
int test_dir_remove(const char *path);

// String utilities
char* test_string_duplicate(const char *str);
int test_string_ends_with(const char *str, const char *suffix);
int test_string_starts_with(const char *str, const char *prefix);
void test_string_replace_char(char *str, char old_char, char new_char);

// Random data generation
void test_random_seed(uint32_t seed);
uint32_t test_random_uint32(void);
void test_random_bytes(uint8_t *buffer, size_t size);
void test_random_string(char *buffer, size_t length);
void test_random_printable_string(char *buffer, size_t length);

// Memory utilities
void* test_malloc_zero(size_t size);
void test_memory_pattern_fill(void *ptr, size_t size, uint8_t pattern);
int test_memory_pattern_check(const void *ptr, size_t size, uint8_t pattern);

// Test data generators
typedef struct {
    char *key;
    void *value;
    size_t value_size;
    int type;
    time_t expire_time;
} test_keyvalue_t;

typedef struct {
    test_keyvalue_t *items;
    size_t count;
    size_t capacity;
} test_dataset_t;

// Dataset management
test_dataset_t* test_dataset_create(size_t capacity);
void test_dataset_destroy(test_dataset_t *dataset);
int test_dataset_add_string(test_dataset_t *dataset, const char *key, const char *value);
int test_dataset_add_integer(test_dataset_t *dataset, const char *key, int64_t value);
int test_dataset_add_binary(test_dataset_t *dataset, const char *key, const void *data, size_t size);
int test_dataset_generate_random(test_dataset_t *dataset, size_t count,
                                size_t avg_key_size, size_t avg_value_size);

// Test environment
const char* test_get_output_dir(void);
const char* test_get_temp_dir(void);
int test_is_verbose(void);
void test_set_verbose(int verbose);

// Progress reporting
void test_progress_start(const char *operation, size_t total_steps);
void test_progress_update(size_t completed_steps);
void test_progress_finish(void);

// Formatted output
void test_print_header(const char *title);
void test_print_separator(void);
void test_print_info(const char *format, ...);
void test_print_success(const char *format, ...);
void test_print_warning(const char *format, ...);
void test_print_error(const char *format, ...);

// System information
typedef struct {
    size_t total_memory_kb;
    size_t available_memory_kb;
    size_t page_size;
    int cpu_count;
    char os_name[64];
    char arch_name[32];
} test_system_info_t;

int test_get_system_info(test_system_info_t *info);
void test_print_system_info(const test_system_info_t *info);

// Performance measurement
typedef struct {
    double min_value;
    double max_value;
    double sum;
    double sum_squares;
    size_t count;
    double *samples;
    size_t samples_capacity;
} test_stats_t;

test_stats_t* test_stats_create(void);
void test_stats_destroy(test_stats_t *stats);
void test_stats_add_sample(test_stats_t *stats, double value);
double test_stats_mean(const test_stats_t *stats);
double test_stats_stddev(const test_stats_t *stats);
double test_stats_percentile(const test_stats_t *stats, double percentile);
void test_stats_print_summary(const test_stats_t *stats, const char *name);

// Test result tracking
typedef struct {
    char name[TEST_MAX_NAME];
    int result_code;
    double duration_ms;
    char message[TEST_MAX_MESSAGE];
} test_result_t;

typedef struct {
    test_result_t *results;
    size_t count;
    size_t capacity;
    double total_time_ms;
} test_suite_results_t;

test_suite_results_t* test_suite_results_create(void);
void test_suite_results_destroy(test_suite_results_t *results);
void test_suite_results_add(test_suite_results_t *results, const char *name,
                           int result_code, double duration_ms, const char *message);
void test_suite_results_print_summary(const test_suite_results_t *results);
int test_suite_results_get_failure_count(const test_suite_results_t *results);

#ifdef __cplusplus
}
#endif

#endif // TEST_UTILS_H