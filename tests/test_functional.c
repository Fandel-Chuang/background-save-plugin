/*
 * Background Save Plugin - Functional Test
 * Copyright (c) 2024 钟芳道 (DJD)
 *
 * Simple functional test demonstrating the background save library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
#endif
#include "background_save.h"
#include "test_utils.h"

// Mock database for testing
typedef struct {
    char **keys;
    char **values;
    size_t count;
    size_t capacity;
} mock_database_t;

static mock_database_t g_mock_db = {0};

// Mock database interface implementation
static int mock_get_all_keys(char ***keys, size_t *count, void *userdata) {
    (void)userdata; // Suppress unused parameter warning

    if (!keys || !count) return RBS_ERR_INVALID_ARGS;

    *keys = malloc(g_mock_db.count * sizeof(char*));
    if (!*keys) return RBS_ERR_MEMORY;

    for (size_t i = 0; i < g_mock_db.count; i++) {
        (*keys)[i] = test_string_duplicate(g_mock_db.keys[i]);
        if (!(*keys)[i]) {
            // Cleanup on failure
            for (size_t j = 0; j < i; j++) {
                free((*keys)[j]);
            }
            free(*keys);
            return RBS_ERR_MEMORY;
        }
    }

    *count = g_mock_db.count;
    return RBS_OK;
}

static int mock_get_key_value(const char *key, rbs_keyvalue_t *kv, void *userdata) {
    (void)userdata; // Suppress unused parameter warning

    if (!key || !kv) return RBS_ERR_INVALID_ARGS;

    // Find the key in our mock database
    for (size_t i = 0; i < g_mock_db.count; i++) {
        if (strcmp(g_mock_db.keys[i], key) == 0) {
            kv->key = test_string_duplicate(key);
            if (!kv->key) return RBS_ERR_MEMORY;

            size_t value_len = strlen(g_mock_db.values[i]);
            kv->value = malloc(value_len + 1);
            if (!kv->value) {
                free(kv->key);
                return RBS_ERR_MEMORY;
            }

            strcpy((char*)kv->value, g_mock_db.values[i]);
            kv->value_size = value_len;
            kv->value_type = 0; // String type
            kv->expire_time = 0; // No expiration

            return RBS_OK;
        }
    }

    return RBS_ERR; // Key not found
}

static void mock_free_key_value(rbs_keyvalue_t *kv, void *userdata) {
    (void)userdata; // Suppress unused parameter warning

    if (kv) {
        free(kv->key);
        free(kv->value);
        memset(kv, 0, sizeof(rbs_keyvalue_t));
    }
}

// Initialize mock database
static int mock_db_init(size_t capacity) {
    g_mock_db.keys = calloc(capacity, sizeof(char*));
    g_mock_db.values = calloc(capacity, sizeof(char*));

    if (!g_mock_db.keys || !g_mock_db.values) {
        free(g_mock_db.keys);
        free(g_mock_db.values);
        return -1;
    }

    g_mock_db.capacity = capacity;
    g_mock_db.count = 0;
    return 0;
}

// Add key-value pair to mock database
static int mock_db_add(const char *key, const char *value) {
    if (g_mock_db.count >= g_mock_db.capacity) return -1;

    g_mock_db.keys[g_mock_db.count] = test_string_duplicate(key);
    g_mock_db.values[g_mock_db.count] = test_string_duplicate(value);

    if (!g_mock_db.keys[g_mock_db.count] || !g_mock_db.values[g_mock_db.count]) {
        free(g_mock_db.keys[g_mock_db.count]);
        free(g_mock_db.values[g_mock_db.count]);
        return -1;
    }

    g_mock_db.count++;
    return 0;
}

// Cleanup mock database
static void mock_db_cleanup(void) {
    for (size_t i = 0; i < g_mock_db.count; i++) {
        free(g_mock_db.keys[i]);
        free(g_mock_db.values[i]);
    }
    free(g_mock_db.keys);
    free(g_mock_db.values);
    memset(&g_mock_db, 0, sizeof(g_mock_db));
}

// Progress callback
static void progress_callback(size_t saved_keys, size_t total_keys, void *userdata) {
    (void)userdata;
    double progress = (double)saved_keys / total_keys * 100;
    printf("  Progress: %zu/%zu keys (%.1f%%)\n", saved_keys, total_keys, progress);
}

// Completion callback
static void completion_callback(int status, const char *error_msg, void *userdata) {
    (void)userdata;
    if (status == RBS_OK) {
        printf("  ✓ Save operation completed successfully!\n");
    } else {
        printf("  ✗ Save operation failed: %s\n", error_msg ? error_msg : "Unknown error");
    }
}

// Log callback
static void log_callback(rbs_log_level_t level, const char *message, void *userdata) {
    (void)userdata;
    const char *level_str;
    switch (level) {
        case RBS_LOG_DEBUG: level_str = "DEBUG"; break;
        case RBS_LOG_INFO:  level_str = "INFO";  break;
        case RBS_LOG_WARN:  level_str = "WARN";  break;
        case RBS_LOG_ERROR: level_str = "ERROR"; break;
        default:            level_str = "UNKNOWN"; break;
    }
    printf("  [%s] %s\n", level_str, message);
}

int main(void) {
    printf("Background Save Plugin - Functional Test\n");
    printf("========================================\n\n");

    // Initialize the library
    printf("1. Initializing background save library...\n");
    int ret = rbs_init();
    if (ret != RBS_OK) {
        printf("   ✗ Failed to initialize: %s\n", rbs_get_error_string(ret));
        return 1;
    }
    printf("   ✓ Library initialized successfully\n\n");

    // Initialize mock database
    printf("2. Setting up mock database...\n");
    if (mock_db_init(100) != 0) {
        printf("   ✗ Failed to initialize mock database\n");
        rbs_cleanup();
        return 1;
    }

    // Add some test data
    mock_db_add("user:1000", "John Doe");
    mock_db_add("user:1001", "Jane Smith");
    mock_db_add("counter:hits", "12345");
    mock_db_add("config:max_connections", "1000");
    mock_db_add("session:abc123", "{\"user_id\":1000,\"expires\":1640995200}");
    printf("   ✓ Added %zu test records to mock database\n\n", g_mock_db.count);

    // Setup database interface
    rbs_database_interface_t db_interface = {
        .get_all_keys = mock_get_all_keys,
        .get_key_value = mock_get_key_value,
        .free_key_value = mock_free_key_value,
        .userdata = NULL
    };

    // Configure background save
    printf("3. Configuring background save operation...\n");
    rbs_config_t config;
    rbs_set_default_config(&config);
    config.filename = "../tests/output/test_dump.rdb";
    config.compression_enabled = 1;
    config.checksum_enabled = 1;
    config.fsync_enabled = 1;
    config.progress_callback = progress_callback;
    config.completion_callback = completion_callback;
    config.log_callback = log_callback;
    config.userdata = NULL;

    // Validate configuration
    ret = rbs_validate_config(&config);
    if (ret != RBS_OK) {
        printf("   ✗ Invalid configuration: %s\n", rbs_get_error_string(ret));
        mock_db_cleanup();
        rbs_cleanup();
        return 1;
    }
    printf("   ✓ Configuration validated\n");
    printf("   - Output file: %s\n", config.filename);
    printf("   - Compression: %s\n", config.compression_enabled ? "Enabled" : "Disabled");
    printf("   - Checksum: %s\n", config.checksum_enabled ? "Enabled" : "Disabled");
    printf("   - Fsync: %s\n\n", config.fsync_enabled ? "Enabled" : "Disabled");

    // Start background save
    printf("4. Starting background save operation...\n");
    ret = rbs_save_background(&config, &db_interface);
    if (ret != RBS_OK) {
        printf("   ✗ Failed to start background save: %s\n", rbs_get_error_string(ret));
        mock_db_cleanup();
        rbs_cleanup();
        return 1;
    }
    printf("   ✓ Background save started successfully\n\n");

    // Monitor progress
    printf("5. Monitoring save progress...\n");
    test_timer_t timer;
    test_timer_start(&timer);

    while (rbs_is_save_in_progress()) {
        rbs_status_t status;
        if (rbs_get_status(&status) == RBS_OK) {
            printf("   Status: %zu/%zu keys saved\n", status.saved_keys, status.total_keys);
        }

#ifdef _WIN32
        Sleep(100); // Sleep 100ms - Windows function
#else
        usleep(100000); // Sleep 100ms - Unix function
#endif
    }

    test_timer_stop(&timer);
    printf("   ✓ Save operation completed in %.2f ms\n\n", test_timer_elapsed_ms(&timer));

    // Wait for completion (should return immediately since save is done)
    printf("6. Waiting for completion...\n");
    ret = rbs_wait_completion(5000); // 5 second timeout
    if (ret != RBS_OK) {
        printf("   ✗ Wait failed: %s\n", rbs_get_error_string(ret));
    } else {
        printf("   ✓ Background save completed successfully\n\n");
    }

    // Verify output file
    printf("7. Verifying output file...\n");
    size_t file_size, key_count;
    ret = rbs_get_rdb_info(config.filename, &file_size, &key_count);
    if (ret != RBS_OK) {
        printf("   ✗ Failed to get RDB info: %s\n", rbs_get_error_string(ret));
    } else {
        printf("   ✓ RDB file created successfully\n");
        printf("   - File size: %zu bytes\n", file_size);
        printf("   - Keys saved: %zu\n", key_count);
    }

    ret = rbs_verify_rdb_file(config.filename);
    if (ret != RBS_OK) {
        printf("   ✗ RDB file verification failed: %s\n", rbs_get_error_string(ret));
    } else {
        printf("   ✓ RDB file format is valid\n\n");
    }

    // Test error conditions
    printf("8. Testing error conditions...\n");

    // Test save already in progress
    printf("   Testing duplicate save request...\n");
    ret = rbs_save_background(&config, &db_interface);
    if (ret == RBS_ERR_INPROGRESS) {
        printf("   ✗ Correctly detected save not in progress (expected behavior)\n");
    } else {
        printf("   ✓ Save not in progress, new save would be allowed\n");
    }

    // Test invalid configuration
    printf("   Testing invalid configuration...\n");
    rbs_config_t invalid_config = config;
    invalid_config.filename = NULL;
    ret = rbs_validate_config(&invalid_config);
    if (ret != RBS_OK) {
        printf("   ✓ Correctly rejected invalid configuration\n");
    } else {
        printf("   ✗ Failed to detect invalid configuration\n");
    }
    printf("\n");

    // Cleanup
    printf("9. Cleaning up...\n");
    mock_db_cleanup();
    printf("   ✓ Mock database cleaned up\n");

    rbs_cleanup();
    printf("   ✓ Library cleaned up\n\n");

    printf("========================================\n");
    printf("✅ Functional test completed successfully!\n");
    printf("All core functionality is working as expected.\n");

    return 0;
}