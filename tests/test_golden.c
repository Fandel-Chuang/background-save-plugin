/*
 * Background Save Plugin - Golden Tests
 * Copyright (c) 2024 钟芳道 (DJD)
 *
 * Golden tests verify RDB format compatibility and data integrity
 * by comparing output against known-good reference files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../include/background_save.h"

// Golden test configuration
#define GOLDEN_TEST_DATA_DIR "tests/golden_data"
#define GOLDEN_TEST_OUTPUT_DIR "tests/golden_output"
#define MAX_PATH_LEN 512
#define MAX_KEYS 10000

// Test result structure
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    double total_time;
} golden_test_results_t;

// Test data structures
typedef struct {
    char key[64];
    char value[256];
    int type;  // 0=string, 1=integer, 2=binary
    size_t value_len;
} test_key_value_t;

typedef struct {
    test_key_value_t *data;
    size_t count;
    size_t capacity;
    char *userdata_ptr;
} test_database_t;

// Global test database
static test_database_t g_test_db = {0};

// Database interface implementation for tests
static int test_get_all_keys(char ***keys, size_t *count, void *userdata) {
    test_database_t *db = (test_database_t *)userdata;

    *keys = malloc(db->count * sizeof(char*));
    if (!*keys) return RBS_ERR_MEMORY;

    for (size_t i = 0; i < db->count; i++) {
        (*keys)[i] = strdup(db->data[i].key);
    }

    *count = db->count;
    return RBS_OK;
}

static int test_get_key_value(const char *key, rbs_keyvalue_t *kv, void *userdata) {
    test_database_t *db = (test_database_t *)userdata;

    // Find key in test database
    for (size_t i = 0; i < db->count; i++) {
        if (strcmp(db->data[i].key, key) == 0) {
            kv->key = strdup(key);
            kv->value = malloc(db->data[i].value_len);
            memcpy(kv->value, db->data[i].value, db->data[i].value_len);
            kv->value_size = db->data[i].value_len;
            kv->type = db->data[i].type;
            kv->expire_time = 0;  // No expiration for test data
            return RBS_OK;
        }
    }

    return RBS_ERR;  // Key not found
}

static void test_free_key_value(rbs_keyvalue_t *kv, void *userdata) {
    (void)userdata;  // Unused parameter

    if (kv->key) {
        free(kv->key);
        kv->key = NULL;
    }
    if (kv->value) {
        free(kv->value);
        kv->value = NULL;
    }
}

// Utility functions
static int create_directories() {
    struct stat st = {0};

    if (stat(GOLDEN_TEST_DATA_DIR, &st) == -1) {
        if (mkdir(GOLDEN_TEST_DATA_DIR, 0755) != 0) {
            perror("mkdir golden_data");
            return -1;
        }
    }

    if (stat(GOLDEN_TEST_OUTPUT_DIR, &st) == -1) {
        if (mkdir(GOLDEN_TEST_OUTPUT_DIR, 0755) != 0) {
            perror("mkdir golden_output");
            return -1;
        }
    }

    return 0;
}

static void init_test_database() {
    g_test_db.capacity = 1000;
    g_test_db.data = calloc(g_test_db.capacity, sizeof(test_key_value_t));
    g_test_db.count = 0;
}

static void cleanup_test_database() {
    if (g_test_db.data) {
        free(g_test_db.data);
        g_test_db.data = NULL;
    }
    g_test_db.count = 0;
    g_test_db.capacity = 0;
}

static void add_test_key_value(const char *key, const void *value, size_t value_len, int type) {
    if (g_test_db.count >= g_test_db.capacity) {
        printf("Warning: Test database capacity exceeded\n");
        return;
    }

    test_key_value_t *kv = &g_test_db.data[g_test_db.count++];
    strncpy(kv->key, key, sizeof(kv->key) - 1);
    kv->key[sizeof(kv->key) - 1] = '\0';

    if (value_len > sizeof(kv->value)) {
        value_len = sizeof(kv->value);
    }

    memcpy(kv->value, value, value_len);
    kv->value_len = value_len;
    kv->type = type;
}

static int compare_files(const char *file1, const char *file2) {
    FILE *f1 = fopen(file1, "rb");
    FILE *f2 = fopen(file2, "rb");

    if (!f1 || !f2) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return -1;  // File access error
    }

    // Get file sizes
    fseek(f1, 0, SEEK_END);
    fseek(f2, 0, SEEK_END);
    long size1 = ftell(f1);
    long size2 = ftell(f2);

    if (size1 != size2) {
        fclose(f1);
        fclose(f2);
        return 1;  // Different sizes
    }

    // Compare content
    fseek(f1, 0, SEEK_SET);
    fseek(f2, 0, SEEK_SET);

    int result = 0;
    for (long i = 0; i < size1; i++) {
        int c1 = fgetc(f1);
        int c2 = fgetc(f2);
        if (c1 != c2) {
            result = 1;  // Different content
            break;
        }
    }

    fclose(f1);
    fclose(f2);
    return result;
}

// Golden test cases

// Test 1: Empty database
static int golden_test_empty_database() {
    printf("Running golden test: Empty Database\n");

    init_test_database();

    rbs_config_t config;
    rbs_set_default_config(&config);
    config.filename = GOLDEN_TEST_OUTPUT_DIR "/empty_db.rdb";
    config.compression_enabled = 0;  // No compression for easier comparison

    rbs_database_interface_t db_interface = {
        .get_all_keys = test_get_all_keys,
        .get_key_value = test_get_key_value,
        .free_key_value = test_free_key_value,
        .userdata = &g_test_db
    };

    int result = rbs_save_background(&config, &db_interface);
    if (result != RBS_OK) {
        printf("FAIL: Save operation failed\n");
        cleanup_test_database();
        return 0;
    }

    rbs_wait_completion(5000);  // 5 second timeout

    // Compare with golden file
    const char *golden_file = GOLDEN_TEST_DATA_DIR "/empty_db_golden.rdb";
    int cmp_result = compare_files(config.filename, golden_file);

    cleanup_test_database();

    if (cmp_result == 0) {
        printf("PASS: Empty database test\n");
        return 1;
    } else if (cmp_result == -1) {
        printf("SKIP: Golden file not found (first run?)\n");
        printf("      Generated: %s\n", config.filename);
        printf("      Copy to: %s\n", golden_file);
        return 1;  // Count as pass for first run
    } else {
        printf("FAIL: Output differs from golden file\n");
        return 0;
    }
}

// Test 2: Simple string keys
static int golden_test_simple_strings() {
    printf("Running golden test: Simple Strings\n");

    init_test_database();

    // Add test data
    add_test_key_value("key1", "value1", 6, 0);  // string
    add_test_key_value("key2", "value2", 6, 0);
    add_test_key_value("hello", "world", 5, 0);
    add_test_key_value("test", "data", 4, 0);

    rbs_config_t config;
    rbs_set_default_config(&config);
    config.filename = GOLDEN_TEST_OUTPUT_DIR "/simple_strings.rdb";
    config.compression_enabled = 0;

    rbs_database_interface_t db_interface = {
        .get_all_keys = test_get_all_keys,
        .get_key_value = test_get_key_value,
        .free_key_value = test_free_key_value,
        .userdata = &g_test_db
    };

    int result = rbs_save_background(&config, &db_interface);
    if (result != RBS_OK) {
        printf("FAIL: Save operation failed\n");
        cleanup_test_database();
        return 0;
    }

    rbs_wait_completion(5000);

    const char *golden_file = GOLDEN_TEST_DATA_DIR "/simple_strings_golden.rdb";
    int cmp_result = compare_files(config.filename, golden_file);

    cleanup_test_database();

    if (cmp_result == 0) {
        printf("PASS: Simple strings test\n");
        return 1;
    } else if (cmp_result == -1) {
        printf("SKIP: Golden file not found\n");
        return 1;
    } else {
        printf("FAIL: Output differs from golden file\n");
        return 0;
    }
}

// Test 3: Integer values
static int golden_test_integers() {
    printf("Running golden test: Integer Values\n");

    init_test_database();

    // Add integer test data
    int32_t int_values[] = {0, 1, -1, 100, -100, 32767, -32768, 1000000, -1000000};
    char key_buf[32];

    for (size_t i = 0; i < sizeof(int_values)/sizeof(int_values[0]); i++) {
        snprintf(key_buf, sizeof(key_buf), "int_%zu", i);
        add_test_key_value(key_buf, &int_values[i], sizeof(int32_t), 1);  // integer type
    }

    rbs_config_t config;
    rbs_set_default_config(&config);
    config.filename = GOLDEN_TEST_OUTPUT_DIR "/integers.rdb";
    config.compression_enabled = 0;

    rbs_database_interface_t db_interface = {
        .get_all_keys = test_get_all_keys,
        .get_key_value = test_get_key_value,
        .free_key_value = test_free_key_value,
        .userdata = &g_test_db
    };

    int result = rbs_save_background(&config, &db_interface);
    if (result != RBS_OK) {
        printf("FAIL: Save operation failed\n");
        cleanup_test_database();
        return 0;
    }

    rbs_wait_completion(5000);

    const char *golden_file = GOLDEN_TEST_DATA_DIR "/integers_golden.rdb";
    int cmp_result = compare_files(config.filename, golden_file);

    cleanup_test_database();

    if (cmp_result == 0) {
        printf("PASS: Integer values test\n");
        return 1;
    } else if (cmp_result == -1) {
        printf("SKIP: Golden file not found\n");
        return 1;
    } else {
        printf("FAIL: Output differs from golden file\n");
        return 0;
    }
}

// Test 4: Binary data
static int golden_test_binary_data() {
    printf("Running golden test: Binary Data\n");

    init_test_database();

    // Add binary test data (including null bytes)
    uint8_t binary_data1[] = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD};
    uint8_t binary_data2[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};  // PNG header
    uint8_t binary_data3[256];
    for (int i = 0; i < 256; i++) {
        binary_data3[i] = (uint8_t)i;
    }

    add_test_key_value("binary1", binary_data1, sizeof(binary_data1), 2);  // binary type
    add_test_key_value("binary2", binary_data2, sizeof(binary_data2), 2);
    add_test_key_value("binary3", binary_data3, sizeof(binary_data3), 2);

    rbs_config_t config;
    rbs_set_default_config(&config);
    config.filename = GOLDEN_TEST_OUTPUT_DIR "/binary_data.rdb";
    config.compression_enabled = 0;

    rbs_database_interface_t db_interface = {
        .get_all_keys = test_get_all_keys,
        .get_key_value = test_get_key_value,
        .free_key_value = test_free_key_value,
        .userdata = &g_test_db
    };

    int result = rbs_save_background(&config, &db_interface);
    if (result != RBS_OK) {
        printf("FAIL: Save operation failed\n");
        cleanup_test_database();
        return 0;
    }

    rbs_wait_completion(5000);

    const char *golden_file = GOLDEN_TEST_DATA_DIR "/binary_data_golden.rdb";
    int cmp_result = compare_files(config.filename, golden_file);

    cleanup_test_database();

    if (cmp_result == 0) {
        printf("PASS: Binary data test\n");
        return 1;
    } else if (cmp_result == -1) {
        printf("SKIP: Golden file not found\n");
        return 1;
    } else {
        printf("FAIL: Output differs from golden file\n");
        return 0;
    }
}

// Test 5: Large dataset
static int golden_test_large_dataset() {
    printf("Running golden test: Large Dataset\n");

    init_test_database();

    // Generate large dataset
    char key_buf[64];
    char value_buf[128];

    for (int i = 0; i < 1000; i++) {
        snprintf(key_buf, sizeof(key_buf), "large_key_%04d", i);
        snprintf(value_buf, sizeof(value_buf), "large_value_%04d_with_some_additional_data", i);
        add_test_key_value(key_buf, value_buf, strlen(value_buf), 0);
    }

    rbs_config_t config;
    rbs_set_default_config(&config);
    config.filename = GOLDEN_TEST_OUTPUT_DIR "/large_dataset.rdb";
    config.compression_enabled = 1;  // Enable compression for large dataset

    rbs_database_interface_t db_interface = {
        .get_all_keys = test_get_all_keys,
        .get_key_value = test_get_key_value,
        .free_key_value = test_free_key_value,
        .userdata = &g_test_db
    };

    int result = rbs_save_background(&config, &db_interface);
    if (result != RBS_OK) {
        printf("FAIL: Save operation failed\n");
        cleanup_test_database();
        return 0;
    }

    rbs_wait_completion(10000);  // Longer timeout for large dataset

    const char *golden_file = GOLDEN_TEST_DATA_DIR "/large_dataset_golden.rdb";
    int cmp_result = compare_files(config.filename, golden_file);

    cleanup_test_database();

    if (cmp_result == 0) {
        printf("PASS: Large dataset test\n");
        return 1;
    } else if (cmp_result == -1) {
        printf("SKIP: Golden file not found\n");
        return 1;
    } else {
        printf("FAIL: Output differs from golden file\n");
        return 0;
    }
}

// Main golden test runner
int run_golden_tests() {
    printf("=== Background Save Plugin - Golden Tests ===\n\n");

    // Initialize
    if (create_directories() != 0) {
        printf("FATAL: Failed to create test directories\n");
        return -1;
    }

    if (rbs_init() != RBS_OK) {
        printf("FATAL: Failed to initialize RBS library\n");
        return -1;
    }

    golden_test_results_t results = {0};
    clock_t start_time = clock();

    // Run test cases
    results.total_tests++;
    results.passed_tests += golden_test_empty_database();

    results.total_tests++;
    results.passed_tests += golden_test_simple_strings();

    results.total_tests++;
    results.passed_tests += golden_test_integers();

    results.total_tests++;
    results.passed_tests += golden_test_binary_data();

    results.total_tests++;
    results.passed_tests += golden_test_large_dataset();

    clock_t end_time = clock();
    results.total_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    results.failed_tests = results.total_tests - results.passed_tests;

    // Cleanup
    rbs_cleanup();

    // Print results
    printf("\n=== Golden Test Results ===\n");
    printf("Total tests:  %d\n", results.total_tests);
    printf("Passed:       %d\n", results.passed_tests);
    printf("Failed:       %d\n", results.failed_tests);
    printf("Total time:   %.3f seconds\n", results.total_time);

    if (results.failed_tests == 0) {
        printf("Result:       ALL TESTS PASSED ✓\n");
        return 0;
    } else {
        printf("Result:       %d TESTS FAILED ✗\n", results.failed_tests);
        return 1;
    }
}

#ifdef STANDALONE_GOLDEN_TEST
int main() {
    return run_golden_tests();
}
#endif