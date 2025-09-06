/*
 * Background Save Plugin - Test Utilities Implementation
 * Copyright (c) 2024 钟芳道 (DJD)
 *
 * Implementation of common utilities and helper functions for tests
 */

#include "test_utils.h"
#include <stdarg.h>
#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)
    #define rmdir(path) _rmdir(path)
    #define unlink(path) _unlink(path)
    #define setenv(name, value, overwrite) _putenv_s(name, value)
    #define getenv(name) getenv(name)
    #include <process.h>
#else
    #include <unistd.h>
    #include <sys/resource.h>
    #include <sys/utsname.h>
#endif
#include <math.h>

// Global test state
static int g_verbose = 0;
static char g_output_dir[TEST_MAX_PATH] = "tests/output";
static char g_temp_dir[TEST_MAX_PATH] = "/tmp";

// Progress tracking
static struct {
    char operation[128];
    size_t total_steps;
    size_t completed_steps;
    time_t start_time;
} g_progress = {0};

// Random number generator state
static uint32_t g_random_state = 1;

// Timing functions
void test_timer_start(test_timer_t *timer) {
#ifdef _WIN32
    // Use GetTickCount for Windows
    timer->start.tv_sec = GetTickCount64() / 1000;
    timer->start.tv_nsec = (GetTickCount64() % 1000) * 1000000;
#else
    clock_gettime(CLOCK_MONOTONIC, &timer->start);
#endif
}

void test_timer_stop(test_timer_t *timer) {
#ifdef _WIN32
    timer->end.tv_sec = GetTickCount64() / 1000;
    timer->end.tv_nsec = (GetTickCount64() % 1000) * 1000000;
#else
    clock_gettime(CLOCK_MONOTONIC, &timer->end);
#endif
    timer->elapsed_ms = (timer->end.tv_sec - timer->start.tv_sec) * 1000.0 +
                       (timer->end.tv_nsec - timer->start.tv_nsec) / 1000000.0;
}

double test_timer_elapsed_ms(const test_timer_t *timer) {
    return timer->elapsed_ms;
}

// File utilities
int test_file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0) ? 1 : 0;
}

int test_file_size(const char *path, size_t *size) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    *size = st.st_size;
    return 0;
}

int test_file_compare(const char *file1, const char *file2) {
    FILE *f1 = fopen(file1, "rb");
    FILE *f2 = fopen(file2, "rb");

    if (!f1 || !f2) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return -1;
    }

    int result = 0;
    int c1, c2;

    do {
        c1 = fgetc(f1);
        c2 = fgetc(f2);
        if (c1 != c2) {
            result = 1;
            break;
        }
    } while (c1 != EOF && c2 != EOF);

    fclose(f1);
    fclose(f2);
    return result;
}

int test_file_copy(const char *src, const char *dst) {
    FILE *source = fopen(src, "rb");
    if (!source) return -1;

    FILE *dest = fopen(dst, "wb");
    if (!dest) {
        fclose(source);
        return -1;
    }

    char buffer[8192];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, bytes, dest) != bytes) {
            fclose(source);
            fclose(dest);
            return -1;
        }
    }

    fclose(source);
    fclose(dest);
    return 0;
}

int test_file_remove(const char *path) {
    return unlink(path);
}

int test_dir_create(const char *path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

int test_dir_remove(const char *path) {
    return rmdir(path);
}

// String utilities
char* test_string_duplicate(const char *str) {
    if (!str) return NULL;

    size_t len = strlen(str);
    char *dup = malloc(len + 1);
    if (dup) {
        strcpy(dup, str);
    }
    return dup;
}

int test_string_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return 0;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) return 0;

    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

int test_string_starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

void test_string_replace_char(char *str, char old_char, char new_char) {
    if (!str) return;

    while (*str) {
        if (*str == old_char) {
            *str = new_char;
        }
        str++;
    }
}

// Random data generation
void test_random_seed(uint32_t seed) {
    g_random_state = seed ? seed : 1;
}

uint32_t test_random_uint32(void) {
    // Simple LCG random number generator
    g_random_state = g_random_state * 1664525 + 1013904223;
    return g_random_state;
}

void test_random_bytes(uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t)(test_random_uint32() & 0xFF);
    }
}

void test_random_string(char *buffer, size_t length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t charset_size = sizeof(charset) - 1;

    for (size_t i = 0; i < length; i++) {
        buffer[i] = charset[test_random_uint32() % charset_size];
    }
    buffer[length] = '\0';
}

void test_random_printable_string(char *buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        buffer[i] = (char)(32 + (test_random_uint32() % 95)); // ASCII 32-126
    }
    buffer[length] = '\0';
}

// Memory utilities
void* test_malloc_zero(size_t size) {
    void *ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void test_memory_pattern_fill(void *ptr, size_t size, uint8_t pattern) {
    memset(ptr, pattern, size);
}

int test_memory_pattern_check(const void *ptr, size_t size, uint8_t pattern) {
    const uint8_t *bytes = (const uint8_t*)ptr;
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != pattern) {
            return 0;
        }
    }
    return 1;
}

// Dataset management
test_dataset_t* test_dataset_create(size_t capacity) {
    test_dataset_t *dataset = test_malloc_zero(sizeof(test_dataset_t));
    if (!dataset) return NULL;

    dataset->items = calloc(capacity, sizeof(test_keyvalue_t));
    if (!dataset->items) {
        free(dataset);
        return NULL;
    }

    dataset->capacity = capacity;
    dataset->count = 0;
    return dataset;
}

void test_dataset_destroy(test_dataset_t *dataset) {
    if (!dataset) return;

    for (size_t i = 0; i < dataset->count; i++) {
        free(dataset->items[i].key);
        free(dataset->items[i].value);
    }

    free(dataset->items);
    free(dataset);
}

int test_dataset_add_string(test_dataset_t *dataset, const char *key, const char *value) {
    if (!dataset || dataset->count >= dataset->capacity) return -1;

    test_keyvalue_t *item = &dataset->items[dataset->count];

    item->key = test_string_duplicate(key);
    if (!item->key) return -1;

    item->value_size = strlen(value);
    item->value = malloc(item->value_size + 1);
    if (!item->value) {
        free(item->key);
        return -1;
    }

    strcpy((char*)item->value, value);
    item->type = 0; // String type
    item->expire_time = 0;

    dataset->count++;
    return 0;
}

int test_dataset_add_integer(test_dataset_t *dataset, const char *key, int64_t value) {
    if (!dataset || dataset->count >= dataset->capacity) return -1;

    test_keyvalue_t *item = &dataset->items[dataset->count];

    item->key = test_string_duplicate(key);
    if (!item->key) return -1;

    item->value_size = sizeof(int64_t);
    item->value = malloc(item->value_size);
    if (!item->value) {
        free(item->key);
        return -1;
    }

    *(int64_t*)item->value = value;
    item->type = 1; // Integer type
    item->expire_time = 0;

    dataset->count++;
    return 0;
}

int test_dataset_add_binary(test_dataset_t *dataset, const char *key, const void *data, size_t size) {
    if (!dataset || dataset->count >= dataset->capacity) return -1;

    test_keyvalue_t *item = &dataset->items[dataset->count];

    item->key = test_string_duplicate(key);
    if (!item->key) return -1;

    item->value_size = size;
    item->value = malloc(size);
    if (!item->value) {
        free(item->key);
        return -1;
    }

    memcpy(item->value, data, size);
    item->type = 2; // Binary type
    item->expire_time = 0;

    dataset->count++;
    return 0;
}

int test_dataset_generate_random(test_dataset_t *dataset, size_t count,
                                size_t avg_key_size, size_t avg_value_size) {
    if (!dataset || count > dataset->capacity) return -1;

    for (size_t i = 0; i < count; i++) {
        char key_buf[128];
        snprintf(key_buf, sizeof(key_buf), "random_key_%zu", i);

        size_t value_size = avg_value_size + (test_random_uint32() % (avg_value_size / 2));
        char *value = malloc(value_size + 1);
        if (!value) return -1;

        test_random_printable_string(value, value_size);

        int result = test_dataset_add_string(dataset, key_buf, value);
        free(value);

        if (result != 0) return -1;
    }

    return 0;
}

// Test environment
const char* test_get_output_dir(void) {
    const char *env_dir = getenv("RBS_TEST_OUTPUT_DIR");
    return env_dir ? env_dir : g_output_dir;
}

const char* test_get_temp_dir(void) {
    const char *env_dir = getenv("TMPDIR");
    if (!env_dir) env_dir = getenv("TMP");
    if (!env_dir) env_dir = getenv("TEMP");
    return env_dir ? env_dir : g_temp_dir;
}

int test_is_verbose(void) {
    return g_verbose || getenv("RBS_TEST_VERBOSE") != NULL;
}

void test_set_verbose(int verbose) {
    g_verbose = verbose;
}

// Progress reporting
void test_progress_start(const char *operation, size_t total_steps) {
    strncpy(g_progress.operation, operation, sizeof(g_progress.operation) - 1);
    g_progress.operation[sizeof(g_progress.operation) - 1] = '\0';
    g_progress.total_steps = total_steps;
    g_progress.completed_steps = 0;
    g_progress.start_time = time(NULL);

    if (test_is_verbose()) {
        printf("Starting: %s (0/%zu)\n", operation, total_steps);
    }
}

void test_progress_update(size_t completed_steps) {
    g_progress.completed_steps = completed_steps;

    if (test_is_verbose()) {
        double progress = (double)completed_steps / g_progress.total_steps;
        int percent = (int)(progress * 100);
        printf("\rProgress: %s [%d%%] (%zu/%zu)",
               g_progress.operation, percent, completed_steps, g_progress.total_steps);
        fflush(stdout);
    }
}

void test_progress_finish(void) {
    if (test_is_verbose()) {
        time_t elapsed = time(NULL) - g_progress.start_time;
        printf("\rCompleted: %s (%zu/%zu) in %ld seconds\n",
               g_progress.operation, g_progress.completed_steps, g_progress.total_steps, elapsed);
    }
}

// Formatted output
void test_print_header(const char *title) {
    printf("\n=== %s ===\n", title);
}

void test_print_separator(void) {
    printf("----------------------------------------\n");
}

void test_print_info(const char *format, ...) {
    if (test_is_verbose()) {
        printf("INFO: ");
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\n");
    }
}

void test_print_success(const char *format, ...) {
    printf("✓ ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void test_print_warning(const char *format, ...) {
    printf("⚠ WARNING: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void test_print_error(const char *format, ...) {
    printf("✗ ERROR: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

// System information
int test_get_system_info(test_system_info_t *info) {
    if (!info) return -1;

    memset(info, 0, sizeof(test_system_info_t));

#ifdef _WIN32
    // Windows implementation
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    info->cpu_count = si.dwNumberOfProcessors;
    info->page_size = si.dwPageSize;

    MEMORYSTATUSEX memstat;
    memstat.dwLength = sizeof(memstat);
    if (GlobalMemoryStatusEx(&memstat)) {
        info->total_memory_kb = memstat.ullTotalPhys / 1024;
        info->available_memory_kb = memstat.ullAvailPhys / 1024;
    }

    strncpy(info->os_name, "Windows", sizeof(info->os_name) - 1);

#ifdef _WIN64
    strncpy(info->arch_name, "x64", sizeof(info->arch_name) - 1);
#else
    strncpy(info->arch_name, "x86", sizeof(info->arch_name) - 1);
#endif

#else
    // Unix implementation
    info->cpu_count = (int)sysconf(_SC_NPROCESSORS_ONLN);
    info->page_size = (size_t)sysconf(_SC_PAGESIZE);

    long pages = sysconf(_SC_PHYS_PAGES);
    if (pages > 0) {
        info->total_memory_kb = (size_t)(pages * info->page_size / 1024);
    }

    long avail_pages = sysconf(_SC_AVPHYS_PAGES);
    if (avail_pages > 0) {
        info->available_memory_kb = (size_t)(avail_pages * info->page_size / 1024);
    }

    struct utsname uname_data;
    if (uname(&uname_data) == 0) {
        strncpy(info->os_name, uname_data.sysname, sizeof(info->os_name) - 1);
        strncpy(info->arch_name, uname_data.machine, sizeof(info->arch_name) - 1);
    }
#endif

    return 0;
}

void test_print_system_info(const test_system_info_t *info) {
    printf("System Information:\n");
    printf("  OS:              %s\n", info->os_name);
    printf("  Architecture:    %s\n", info->arch_name);
    printf("  CPU Cores:       %d\n", info->cpu_count);
    printf("  Page Size:       %zu bytes\n", info->page_size);
    printf("  Total Memory:    %.2f MB\n", info->total_memory_kb / 1024.0);
    printf("  Available Mem:   %.2f MB\n", info->available_memory_kb / 1024.0);
}

// Performance measurement
test_stats_t* test_stats_create(void) {
    test_stats_t *stats = test_malloc_zero(sizeof(test_stats_t));
    if (!stats) return NULL;

    stats->samples_capacity = 1000;
    stats->samples = malloc(stats->samples_capacity * sizeof(double));
    if (!stats->samples) {
        free(stats);
        return NULL;
    }

    stats->min_value = INFINITY;
    stats->max_value = -INFINITY;
    return stats;
}

void test_stats_destroy(test_stats_t *stats) {
    if (stats) {
        free(stats->samples);
        free(stats);
    }
}

void test_stats_add_sample(test_stats_t *stats, double value) {
    if (!stats) return;

    // Expand samples array if needed
    if (stats->count >= stats->samples_capacity) {
        stats->samples_capacity *= 2;
        stats->samples = realloc(stats->samples, stats->samples_capacity * sizeof(double));
        if (!stats->samples) return; // Memory allocation failed
    }

    stats->samples[stats->count] = value;
    stats->count++;

    // Update statistics
    stats->sum += value;
    stats->sum_squares += value * value;

    if (value < stats->min_value) stats->min_value = value;
    if (value > stats->max_value) stats->max_value = value;
}

double test_stats_mean(const test_stats_t *stats) {
    return stats->count > 0 ? stats->sum / stats->count : 0.0;
}

double test_stats_stddev(const test_stats_t *stats) {
    if (stats->count <= 1) return 0.0;

    double mean = test_stats_mean(stats);
    double variance = (stats->sum_squares - stats->count * mean * mean) / (stats->count - 1);
    return sqrt(variance);
}

static int compare_doubles(const void *a, const void *b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

double test_stats_percentile(const test_stats_t *stats, double percentile) {
    if (stats->count == 0) return 0.0;

    // Create a sorted copy of the samples
    double *sorted = malloc(stats->count * sizeof(double));
    if (!sorted) return 0.0;

    memcpy(sorted, stats->samples, stats->count * sizeof(double));
    qsort(sorted, stats->count, sizeof(double), compare_doubles);

    size_t index = (size_t)(percentile * (stats->count - 1) / 100.0);
    double result = sorted[index];

    free(sorted);
    return result;
}

void test_stats_print_summary(const test_stats_t *stats, const char *name) {
    printf("%s Statistics:\n", name);
    printf("  Samples:    %zu\n", stats->count);
    printf("  Mean:       %.4f\n", test_stats_mean(stats));
    printf("  Std Dev:    %.4f\n", test_stats_stddev(stats));
    printf("  Min:        %.4f\n", stats->min_value);
    printf("  Max:        %.4f\n", stats->max_value);
    printf("  50th %%:     %.4f\n", test_stats_percentile(stats, 50.0));
    printf("  95th %%:     %.4f\n", test_stats_percentile(stats, 95.0));
    printf("  99th %%:     %.4f\n", test_stats_percentile(stats, 99.0));
}

// Test result tracking
test_suite_results_t* test_suite_results_create(void) {
    test_suite_results_t *results = test_malloc_zero(sizeof(test_suite_results_t));
    if (!results) return NULL;

    results->capacity = 100;
    results->results = calloc(results->capacity, sizeof(test_result_t));
    if (!results->results) {
        free(results);
        return NULL;
    }

    return results;
}

void test_suite_results_destroy(test_suite_results_t *results) {
    if (results) {
        free(results->results);
        free(results);
    }
}

void test_suite_results_add(test_suite_results_t *results, const char *name,
                           int result_code, double duration_ms, const char *message) {
    if (!results || results->count >= results->capacity) return;

    test_result_t *result = &results->results[results->count];

    strncpy(result->name, name, TEST_MAX_NAME - 1);
    result->name[TEST_MAX_NAME - 1] = '\0';

    result->result_code = result_code;
    result->duration_ms = duration_ms;

    if (message) {
        strncpy(result->message, message, TEST_MAX_MESSAGE - 1);
        result->message[TEST_MAX_MESSAGE - 1] = '\0';
    }

    results->count++;
    results->total_time_ms += duration_ms;
}

void test_suite_results_print_summary(const test_suite_results_t *results) {
    if (!results) return;

    int passed = 0, failed = 0, skipped = 0, errors = 0;

    for (size_t i = 0; i < results->count; i++) {
        switch (results->results[i].result_code) {
            case TEST_OK: passed++; break;
            case TEST_FAIL: failed++; break;
            case TEST_SKIP: skipped++; break;
            case TEST_ERROR: errors++; break;
        }
    }

    printf("\nTest Suite Results:\n");
    printf("  Total tests:  %zu\n", results->count);
    printf("  Passed:       %d\n", passed);
    printf("  Failed:       %d\n", failed);
    printf("  Skipped:      %d\n", skipped);
    printf("  Errors:       %d\n", errors);
    printf("  Total time:   %.2f ms\n", results->total_time_ms);

    if (failed > 0 || errors > 0) {
        printf("\nFailed/Error tests:\n");
        for (size_t i = 0; i < results->count; i++) {
            if (results->results[i].result_code == TEST_FAIL ||
                results->results[i].result_code == TEST_ERROR) {
                printf("  %s: %s\n", results->results[i].name, results->results[i].message);
            }
        }
    }
}

int test_suite_results_get_failure_count(const test_suite_results_t *results) {
    if (!results) return 0;

    int failures = 0;
    for (size_t i = 0; i < results->count; i++) {
        if (results->results[i].result_code != TEST_OK &&
            results->results[i].result_code != TEST_SKIP) {
            failures++;
        }
    }
    return failures;
}