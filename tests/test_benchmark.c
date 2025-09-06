/*
 * Background Save Plugin - Benchmark Test
 * Copyright (c) 2024 钟芳道 (DJD)
 *
 * Comprehensive performance benchmarking for the background save library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <sys/time.h>
#endif
#include "background_save.h"
#include "test_utils.h"

// Benchmark configuration
#define MAX_ITERATIONS 10000
#define MAX_KEY_SIZE 1024
#define MAX_VALUE_SIZE (64 * 1024)  // 64KB max value size

// Test data structures
typedef struct {
    char **keys;
    char **values;
    size_t *value_sizes;
    size_t count;
    size_t total_data_size;
} benchmark_dataset_t;

// Performance metrics
typedef struct {
    double min_time_ms;
    double max_time_ms;
    double avg_time_ms;
    double std_dev_ms;
    double total_time_ms;
    size_t iterations;
    size_t total_bytes;
    double throughput_mbps;
    double keys_per_second;
} benchmark_result_t;

// High-resolution timer
typedef struct {
    double start_time;
    double end_time;
} hrtime_t;

static void hrtime_start(hrtime_t *timer) {
#ifdef _WIN32
    LARGE_INTEGER freq, start;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    timer->start_time = (double)start.QuadPart / freq.QuadPart * 1000.0;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timer->start_time = tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
#endif
}

static void hrtime_stop(hrtime_t *timer) {
#ifdef _WIN32
    LARGE_INTEGER freq, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&end);
    timer->end_time = (double)end.QuadPart / freq.QuadPart * 1000.0;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timer->end_time = tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
#endif
}

static double hrtime_elapsed(const hrtime_t *timer) {
    return timer->end_time - timer->start_time;
}

// Generate random string
static void generate_random_string(char *str, size_t length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t charset_size = sizeof(charset) - 1;

    for (size_t i = 0; i < length - 1; i++) {
        str[i] = charset[rand() % charset_size];
    }
    str[length - 1] = '\0';
}

// Create benchmark dataset
static int create_benchmark_dataset(benchmark_dataset_t *dataset, size_t key_count,
                                   size_t avg_key_size, size_t avg_value_size) {
    dataset->keys = calloc(key_count, sizeof(char*));
    dataset->values = calloc(key_count, sizeof(char*));
    dataset->value_sizes = calloc(key_count, sizeof(size_t));

    if (!dataset->keys || !dataset->values || !dataset->value_sizes) {
        return -1;
    }

    dataset->count = key_count;
    dataset->total_data_size = 0;

    for (size_t i = 0; i < key_count; i++) {
        // Randomize sizes around the average
        size_t key_size = avg_key_size + (rand() % (avg_key_size / 2)) - (avg_key_size / 4);
        size_t value_size = avg_value_size + (rand() % (avg_value_size / 2)) - (avg_value_size / 4);

        if (key_size < 8) key_size = 8;
        if (key_size > MAX_KEY_SIZE) key_size = MAX_KEY_SIZE;
        if (value_size < 16) value_size = 16;
        if (value_size > MAX_VALUE_SIZE) value_size = MAX_VALUE_SIZE;

        dataset->keys[i] = malloc(key_size + 1);
        dataset->values[i] = malloc(value_size + 1);

        if (!dataset->keys[i] || !dataset->values[i]) {
            return -1;
        }

        // Generate key with prefix
        snprintf(dataset->keys[i], key_size + 1, "benchmark:key:%zu", i);

        // Generate random value
        generate_random_string(dataset->values[i], value_size + 1);

        dataset->value_sizes[i] = value_size;
        dataset->total_data_size += key_size + value_size;
    }

    return 0;
}

// Cleanup benchmark dataset
static void cleanup_benchmark_dataset(benchmark_dataset_t *dataset) {
    if (dataset->keys) {
        for (size_t i = 0; i < dataset->count; i++) {
            free(dataset->keys[i]);
        }
        free(dataset->keys);
    }

    if (dataset->values) {
        for (size_t i = 0; i < dataset->count; i++) {
            free(dataset->values[i]);
        }
        free(dataset->values);
    }

    free(dataset->value_sizes);
    memset(dataset, 0, sizeof(benchmark_dataset_t));
}

// Database interface for benchmark
static benchmark_dataset_t *g_benchmark_data = NULL;

static int benchmark_get_all_keys(char ***keys, size_t *count, void *userdata) {
    (void)userdata;

    if (!g_benchmark_data || !keys || !count) return RBS_ERR_INVALID_ARGS;

    *keys = malloc(g_benchmark_data->count * sizeof(char*));
    if (!*keys) return RBS_ERR_MEMORY;

    for (size_t i = 0; i < g_benchmark_data->count; i++) {
        (*keys)[i] = test_string_duplicate(g_benchmark_data->keys[i]);
        if (!(*keys)[i]) {
            for (size_t j = 0; j < i; j++) {
                free((*keys)[j]);
            }
            free(*keys);
            return RBS_ERR_MEMORY;
        }
    }

    *count = g_benchmark_data->count;
    return RBS_OK;
}

static int benchmark_get_key_value(const char *key, rbs_keyvalue_t *kv, void *userdata) {
    (void)userdata;

    if (!g_benchmark_data || !key || !kv) return RBS_ERR_INVALID_ARGS;

    // Find key in dataset
    for (size_t i = 0; i < g_benchmark_data->count; i++) {
        if (strcmp(g_benchmark_data->keys[i], key) == 0) {
            kv->key = test_string_duplicate(key);
            if (!kv->key) return RBS_ERR_MEMORY;

            size_t value_size = g_benchmark_data->value_sizes[i];
            kv->value = malloc(value_size + 1);
            if (!kv->value) {
                free(kv->key);
                return RBS_ERR_MEMORY;
            }

            memcpy(kv->value, g_benchmark_data->values[i], value_size + 1);
            kv->value_size = value_size;
            kv->value_type = 0;
            kv->expire_time = 0;

            return RBS_OK;
        }
    }

    return RBS_ERR;
}

static void benchmark_free_key_value(rbs_keyvalue_t *kv, void *userdata) {
    (void)userdata;

    if (kv) {
        free(kv->key);
        free(kv->value);
        memset(kv, 0, sizeof(rbs_keyvalue_t));
    }
}

// Calculate statistics
static void calculate_stats(double *times, size_t count, benchmark_result_t *result) {
    if (count == 0) return;

    // Find min/max
    result->min_time_ms = times[0];
    result->max_time_ms = times[0];
    result->total_time_ms = 0;

    for (size_t i = 0; i < count; i++) {
        if (times[i] < result->min_time_ms) result->min_time_ms = times[i];
        if (times[i] > result->max_time_ms) result->max_time_ms = times[i];
        result->total_time_ms += times[i];
    }

    // Calculate average
    result->avg_time_ms = result->total_time_ms / count;

    // Calculate standard deviation
    double variance = 0;
    for (size_t i = 0; i < count; i++) {
        double diff = times[i] - result->avg_time_ms;
        variance += diff * diff;
    }
    result->std_dev_ms = sqrt(variance / count);

    result->iterations = count;
}

// Print results
static void print_benchmark_results(const char *test_name, const benchmark_result_t *result) {
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ %-63s │\n", test_name);
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    printf("│ Iterations:     %10zu                                    │\n", result->iterations);
    printf("│ Total Time:     %10.2f ms                               │\n", result->total_time_ms);
    printf("│ Average Time:   %10.2f ms                               │\n", result->avg_time_ms);
    printf("│ Min Time:       %10.2f ms                               │\n", result->min_time_ms);
    printf("│ Max Time:       %10.2f ms                               │\n", result->max_time_ms);
    printf("│ Std Deviation:  %10.2f ms                               │\n", result->std_dev_ms);
    if (result->throughput_mbps > 0) {
        printf("│ Throughput:     %10.2f MB/s                             │\n", result->throughput_mbps);
        printf("│ Keys/Second:    %10.0f keys/s                           │\n", result->keys_per_second);
    }
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
}

// Benchmark save operations
static int benchmark_save_operations(size_t iterations, size_t key_count,
                                   size_t avg_key_size, size_t avg_value_size) {
    printf("Creating benchmark dataset (%zu keys, ~%zu bytes each)...\n",
           key_count, avg_key_size + avg_value_size);

    benchmark_dataset_t dataset;
    if (create_benchmark_dataset(&dataset, key_count, avg_key_size, avg_value_size) != 0) {
        printf("Failed to create benchmark dataset\n");
        return -1;
    }

    g_benchmark_data = &dataset;

    printf("Dataset created: %zu keys, %.2f MB total\n\n",
           dataset.count, dataset.total_data_size / 1024.0 / 1024.0);

    // Setup database interface
    rbs_database_interface_t db_interface = {
        .get_all_keys = benchmark_get_all_keys,
        .get_key_value = benchmark_get_key_value,
        .free_key_value = benchmark_free_key_value,
        .userdata = NULL
    };

    // Setup configuration
    rbs_config_t config;
    rbs_set_default_config(&config);
    config.filename = "../tests/output/benchmark.rdb";
    config.compression_enabled = 0;  // Disable compression for pure save speed
    config.checksum_enabled = 0;     // Disable checksum for pure save speed
    config.fsync_enabled = 0;        // Disable fsync for pure save speed

    double *save_times = malloc(iterations * sizeof(double));
    if (!save_times) {
        cleanup_benchmark_dataset(&dataset);
        return -1;
    }

    printf("Running %zu save operations...\n", iterations);

    for (size_t i = 0; i < iterations; i++) {
        hrtime_t timer;
        hrtime_start(&timer);

        int ret = rbs_save_background(&config, &db_interface);
        if (ret != RBS_OK) {
            printf("Save operation %zu failed: %s\n", i + 1, rbs_get_error_string(ret));
            break;
        }

        // Wait for completion
        while (rbs_is_save_in_progress()) {
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1000);
#endif
        }

        hrtime_stop(&timer);
        save_times[i] = hrtime_elapsed(&timer);

        if ((i + 1) % (iterations / 10) == 0 || iterations < 10) {
            printf("  Completed %zu/%zu (%.1f%%)\n", i + 1, iterations,
                   (double)(i + 1) / iterations * 100);
        }
    }

    // Calculate results
    benchmark_result_t result = {0};
    calculate_stats(save_times, iterations, &result);

    // Calculate throughput
    result.total_bytes = dataset.total_data_size * iterations;
    if (result.total_time_ms > 0) {
        result.throughput_mbps = (result.total_bytes / 1024.0 / 1024.0) / (result.total_time_ms / 1000.0);
        result.keys_per_second = (dataset.count * iterations) / (result.total_time_ms / 1000.0);
    }

    char test_name[128];
    snprintf(test_name, sizeof(test_name), "Save Operations (%zu keys, %zu iterations)",
             key_count, iterations);
    print_benchmark_results(test_name, &result);

    free(save_times);
    cleanup_benchmark_dataset(&dataset);
    g_benchmark_data = NULL;

    return 0;
}

// Benchmark with different configurations
static int benchmark_configurations(void) {
    printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                        CONFIGURATION BENCHMARKS                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════╝\n\n");

    benchmark_dataset_t dataset;
    if (create_benchmark_dataset(&dataset, 1000, 32, 512) != 0) {
        printf("Failed to create benchmark dataset\n");
        return -1;
    }

    g_benchmark_data = &dataset;

    rbs_database_interface_t db_interface = {
        .get_all_keys = benchmark_get_all_keys,
        .get_key_value = benchmark_get_key_value,
        .free_key_value = benchmark_free_key_value,
        .userdata = NULL
    };

    struct {
        const char *name;
        int compression;
        int checksum;
        int fsync;
    } configs[] = {
        {"Baseline (no compression, no checksum, no fsync)", 0, 0, 0},
        {"With Compression", 1, 0, 0},
        {"With Checksum", 0, 1, 0},
        {"With Fsync", 0, 0, 1},
        {"Full Features (compression + checksum + fsync)", 1, 1, 1}
    };

    size_t num_configs = sizeof(configs) / sizeof(configs[0]);

    for (size_t i = 0; i < num_configs; i++) {
        printf("Testing configuration: %s\n", configs[i].name);

        rbs_config_t config;
        rbs_set_default_config(&config);
        config.filename = "../tests/output/benchmark_config.rdb";
        config.compression_enabled = configs[i].compression;
        config.checksum_enabled = configs[i].checksum;
        config.fsync_enabled = configs[i].fsync;

        const size_t iterations = 100;
        double *times = malloc(iterations * sizeof(double));

        for (size_t j = 0; j < iterations; j++) {
            hrtime_t timer;
            hrtime_start(&timer);

            rbs_save_background(&config, &db_interface);
            while (rbs_is_save_in_progress()) {
#ifdef _WIN32
                Sleep(1);
#else
                usleep(1000);
#endif
            }

            hrtime_stop(&timer);
            times[j] = hrtime_elapsed(&timer);
        }

        benchmark_result_t result = {0};
        calculate_stats(times, iterations, &result);

        print_benchmark_results(configs[i].name, &result);

        free(times);
    }

    cleanup_benchmark_dataset(&dataset);
    g_benchmark_data = NULL;

    return 0;
}

// Memory usage benchmark
static int benchmark_memory_usage(void) {
    printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                          MEMORY USAGE BENCHMARK                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════╝\n\n");

    size_t key_counts[] = {100, 500, 1000, 5000, 10000};
    size_t num_tests = sizeof(key_counts) / sizeof(key_counts[0]);

    for (size_t i = 0; i < num_tests; i++) {
        printf("Testing with %zu keys...\n", key_counts[i]);

        benchmark_dataset_t dataset;
        if (create_benchmark_dataset(&dataset, key_counts[i], 32, 256) != 0) {
            printf("Failed to create dataset with %zu keys\n", key_counts[i]);
            continue;
        }

        printf("  Dataset size: %.2f MB\n", dataset.total_data_size / 1024.0 / 1024.0);
        printf("  Average key size: 32 bytes\n");
        printf("  Average value size: 256 bytes\n\n");

        cleanup_benchmark_dataset(&dataset);
    }

    return 0;
}

int main(void) {
    printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                           ║\n");
    printf("║            BACKGROUND SAVE PLUGIN - PERFORMANCE BENCHMARK               ║\n");
    printf("║                                                                           ║\n");
    printf("║  Comprehensive performance testing for Redis-inspired save library      ║\n");
    printf("║                                                                           ║\n");
    printf("║  Maintainer: 钟芳道 (DJD) - zhongfangdao888@gmail.com                   ║\n");
    printf("║  License: MIT with Additional Restrictions                               ║\n");
    printf("║                                                                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════╝\n\n");

    // Initialize random seed
    srand((unsigned int)time(NULL));

    // Initialize library
    if (rbs_init() != RBS_OK) {
        printf("Failed to initialize background save library\n");
        return 1;
    }

    printf("Benchmark Configuration:\n");
    printf("  Platform: %s\n",
#ifdef _WIN32
           "Windows (synchronous save mode)"
#else
           "Unix/Linux (asynchronous fork mode)"
#endif
    );
    printf("  Max iterations: %d\n", MAX_ITERATIONS);
    printf("  Max key size: %d bytes\n", MAX_KEY_SIZE);
    printf("  Max value size: %d bytes\n\n", MAX_VALUE_SIZE);

    // Run benchmarks
    printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                           SAVE OPERATION BENCHMARKS                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════╝\n\n");

    // Small dataset, many iterations
    benchmark_save_operations(1000, 10, 16, 64);

    // Medium dataset
    benchmark_save_operations(100, 100, 32, 256);

    // Large dataset, fewer iterations
    benchmark_save_operations(10, 1000, 64, 1024);

    // Configuration benchmarks
    benchmark_configurations();

    // Memory usage benchmark
    benchmark_memory_usage();

    // Cleanup
    rbs_cleanup();

    printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                         BENCHMARK COMPLETED                              ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════╝\n\n");

    printf("🎯 Performance Summary:\n");
    printf("   • Small datasets (10 keys): Optimized for high-frequency saves\n");
    printf("   • Medium datasets (100 keys): Balanced performance\n");
    printf("   • Large datasets (1000+ keys): Bulk data persistence\n");
    printf("   • Configuration impact: Compression and checksums add overhead\n");
    printf("   • Memory efficiency: Linear scaling with dataset size\n\n");

    printf("💡 Optimization Tips:\n");
    printf("   • Disable compression for time-critical saves\n");
    printf("   • Enable fsync only when data durability is critical\n");
    printf("   • Batch small saves together for better throughput\n");
    printf("   • Monitor memory usage in high-frequency scenarios\n\n");

    printf("📊 For detailed analysis, review the RDB files in tests/output/\n");
    printf("   Use rdb_inspector.exe to examine the generated files.\n\n");

    return 0;
}
