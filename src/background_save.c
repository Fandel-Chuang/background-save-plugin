/*
 * Background Save Plugin - Core Implementation
 * Copyright (c) 2024 钟芳道 (DJD)
 *
 * Main implementation of the background save functionality
 */

#include "background_save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #include <io.h>
    #include <direct.h>
    #define fork() -1  // fork not available on Windows
    #define waitpid(pid, status, options) -1
    #define kill(pid, sig) TerminateProcess((HANDLE)pid, 1)
    #define fsync(fd) _commit(fd)
    #define usleep(microseconds) Sleep((microseconds + 999) / 1000)
    #define WIFEXITED(status) 1
    #define WEXITSTATUS(status) (status)
    #define WNOHANG 1
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <signal.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

// Global state
static rbs_status_t g_save_status = {0};
static int g_initialized = 0;

// Error messages
static const char* error_messages[] = {
    "Success",
    "General error",
    "Fork failed",
    "Save already in progress",
    "Invalid arguments",
    "I/O error",
    "Memory allocation failed"
};

// RDB format constants
#define RDB_VERSION 9
#define RDB_MAGIC "REDIS"
#define RDB_OPCODE_EOF 0xFF
#define RDB_OPCODE_SELECTDB 0xFE
#define RDB_OPCODE_EXPIRETIME 0xFD
#define RDB_OPCODE_EXPIRETIME_MS 0xFC
#define RDB_OPCODE_RESIZEDB 0xFB

// Value types
#define RDB_TYPE_STRING 0
#define RDB_TYPE_LIST 1
#define RDB_TYPE_SET 2
#define RDB_TYPE_ZSET 3
#define RDB_TYPE_HASH 4

// Initialize the library
int rbs_init(void) {
    if (g_initialized) {
        return RBS_OK;
    }

    memset(&g_save_status, 0, sizeof(g_save_status));
    g_initialized = 1;
    return RBS_OK;
}

// Cleanup the library
void rbs_cleanup(void) {
    if (g_save_status.is_active && g_save_status.child_pid > 0) {
#ifdef _WIN32
        kill(g_save_status.child_pid, 1);  // Use generic termination signal
#else
        kill(g_save_status.child_pid, SIGTERM);
#endif
        waitpid(g_save_status.child_pid, NULL, 0);
    }

    memset(&g_save_status, 0, sizeof(g_save_status));
    g_initialized = 0;
}

// Set default configuration
void rbs_set_default_config(rbs_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(rbs_config_t));
    config->filename = "dump.rdb";
    config->compression_enabled = 1;
    config->checksum_enabled = 1;
    config->fsync_enabled = 1;
    config->max_memory_usage = 100 * 1024 * 1024; // 100MB
    config->key_save_delay = 0;
    config->incremental_fsync = 0;
}

// Validate configuration
int rbs_validate_config(const rbs_config_t *config) {
    if (!config) return RBS_ERR_INVALID_ARGS;
    if (!config->filename) return RBS_ERR_INVALID_ARGS;
    if (strlen(config->filename) == 0) return RBS_ERR_INVALID_ARGS;

    return RBS_OK;
}

// Get error string
const char* rbs_get_error_string(int error_code) {
    int index = -error_code;
    if (index >= 0 && index < (int)(sizeof(error_messages) / sizeof(error_messages[0]))) {
        return error_messages[index];
    }
    return "Unknown error";
}

// Check if save is in progress
int rbs_is_save_in_progress(void) {
    if (!g_save_status.is_active) return 0;

    // Check if child process is still running
    if (g_save_status.child_pid > 0) {
        int status;
        pid_t result = waitpid(g_save_status.child_pid, &status, WNOHANG);
        if (result == g_save_status.child_pid) {
            // Child has exited
            g_save_status.is_active = 0;
            g_save_status.child_pid = 0;
            return 0;
        }
    }

    return g_save_status.is_active;
}

// Get current status
int rbs_get_status(rbs_status_t *status) {
    if (!status) return RBS_ERR_INVALID_ARGS;

    *status = g_save_status;
    return RBS_OK;
}

// Write integer to file
static int write_int32(FILE *fp, uint32_t value) {
    uint8_t buf[4];
    buf[0] = value & 0xFF;
    buf[1] = (value >> 8) & 0xFF;
    buf[2] = (value >> 16) & 0xFF;
    buf[3] = (value >> 24) & 0xFF;
    return fwrite(buf, 1, 4, fp) == 4 ? 0 : -1;
}

// Write string to file
static int write_string(FILE *fp, const char *str, size_t len) {
    // Write length encoding
    if (len < 64) {
        uint8_t enclen = len;
        if (fwrite(&enclen, 1, 1, fp) != 1) return -1;
    } else if (len < 16384) {
        uint8_t enclen[2];
        enclen[0] = (64 | ((len >> 8) & 0x3F));
        enclen[1] = len & 0xFF;
        if (fwrite(enclen, 1, 2, fp) != 2) return -1;
    } else {
        uint8_t enclen[5];
        enclen[0] = 128;
        enclen[1] = (len >> 24) & 0xFF;
        enclen[2] = (len >> 16) & 0xFF;
        enclen[3] = (len >> 8) & 0xFF;
        enclen[4] = len & 0xFF;
        if (fwrite(enclen, 1, 5, fp) != 5) return -1;
    }

    // Write string data
    if (len > 0 && fwrite(str, 1, len, fp) != len) return -1;
    return 0;
}

// Child process function for saving
static int save_rdb_child(const rbs_config_t *config, const rbs_database_interface_t *db_interface) {
    FILE *fp = fopen(config->filename, "wb");
    if (!fp) return RBS_ERR_IO;

    int ret = RBS_OK;
    char **keys = NULL;
    size_t key_count = 0;

    do {
        // Write RDB header
        if (fwrite(RDB_MAGIC, 1, 5, fp) != 5) {
            ret = RBS_ERR_IO;
            break;
        }

        // Write version
        char version[4];
        snprintf(version, sizeof(version), "%04d", RDB_VERSION);
        if (fwrite(version, 1, 4, fp) != 4) {
            ret = RBS_ERR_IO;
            break;
        }

        // Get all keys
        ret = db_interface->get_all_keys(&keys, &key_count, db_interface->userdata);
        if (ret != RBS_OK) break;

        // Write database selector
        uint8_t db_selector = RDB_OPCODE_SELECTDB;
        if (fwrite(&db_selector, 1, 1, fp) != 1) {
            ret = RBS_ERR_IO;
            break;
        }

        // Write database number (0)
        uint8_t db_num = 0;
        if (fwrite(&db_num, 1, 1, fp) != 1) {
            ret = RBS_ERR_IO;
            break;
        }

        // Write resize DB info
        uint8_t resize_db = RDB_OPCODE_RESIZEDB;
        if (fwrite(&resize_db, 1, 1, fp) != 1) {
            ret = RBS_ERR_IO;
            break;
        }

        // Write key count and expire count (both as key_count for simplicity)
        if (write_int32(fp, key_count) != 0 || write_int32(fp, 0) != 0) {
            ret = RBS_ERR_IO;
            break;
        }

        // Write key-value pairs
        for (size_t i = 0; i < key_count; i++) {
            rbs_keyvalue_t kv;
            ret = db_interface->get_key_value(keys[i], &kv, db_interface->userdata);
            if (ret != RBS_OK) break;

            // Write expire time if set
            if (kv.expire_time > 0) {
                uint8_t expire_opcode = RDB_OPCODE_EXPIRETIME_MS;
                if (fwrite(&expire_opcode, 1, 1, fp) != 1) {
                    ret = RBS_ERR_IO;
                    db_interface->free_key_value(&kv, db_interface->userdata);
                    break;
                }

                uint64_t expire_ms = kv.expire_time;
                if (fwrite(&expire_ms, 1, 8, fp) != 8) {
                    ret = RBS_ERR_IO;
                    db_interface->free_key_value(&kv, db_interface->userdata);
                    break;
                }
            }

            // Write value type
            uint8_t value_type = RDB_TYPE_STRING; // Default to string
            if (fwrite(&value_type, 1, 1, fp) != 1) {
                ret = RBS_ERR_IO;
                db_interface->free_key_value(&kv, db_interface->userdata);
                break;
            }

            // Write key
            if (write_string(fp, kv.key, strlen(kv.key)) != 0) {
                ret = RBS_ERR_IO;
                db_interface->free_key_value(&kv, db_interface->userdata);
                break;
            }

            // Write value
            if (write_string(fp, (const char*)kv.value, kv.value_size) != 0) {
                ret = RBS_ERR_IO;
                db_interface->free_key_value(&kv, db_interface->userdata);
                break;
            }

            db_interface->free_key_value(&kv, db_interface->userdata);

            // Progress callback
            if (config->progress_callback) {
                config->progress_callback(i + 1, key_count, config->userdata);
            }

            // Key save delay
            if (config->key_save_delay > 0) {
                usleep(config->key_save_delay);
            }
        }

        if (ret != RBS_OK) break;

        // Write EOF
        uint8_t eof_marker = RDB_OPCODE_EOF;
        if (fwrite(&eof_marker, 1, 1, fp) != 1) {
            ret = RBS_ERR_IO;
            break;
        }

        // Write checksum (simplified - just write 8 zero bytes)
        if (config->checksum_enabled) {
            uint64_t checksum = 0;
            if (fwrite(&checksum, 1, 8, fp) != 8) {
                ret = RBS_ERR_IO;
                break;
            }
        }

    } while (0);

    // Cleanup
    if (keys) {
        for (size_t i = 0; i < key_count; i++) {
            free(keys[i]);
        }
        free(keys);
    }

    // Fsync if enabled
    if (config->fsync_enabled && ret == RBS_OK) {
        if (fsync(fileno(fp)) != 0) {
            ret = RBS_ERR_IO;
        }
    }

    fclose(fp);

    // Completion callback
    if (config->completion_callback) {
        const char *error_msg = (ret == RBS_OK) ? NULL : rbs_get_error_string(ret);
        config->completion_callback(ret, error_msg, config->userdata);
    }

    return ret;
}

// Start background save
int rbs_save_background(const rbs_config_t *config, const rbs_database_interface_t *db_interface) {
    if (!g_initialized) return RBS_ERR;
    if (!config || !db_interface) return RBS_ERR_INVALID_ARGS;

    int ret = rbs_validate_config(config);
    if (ret != RBS_OK) return ret;

    if (rbs_is_save_in_progress()) {
        return RBS_ERR_INPROGRESS;
    }

#ifdef _WIN32
    // On Windows, we can't fork, so just do synchronous save for now
    // In a real implementation, you would use CreateThread
    printf("  [INFO] Windows detected - performing synchronous save\n");

    g_save_status.is_active = 1;
    g_save_status.child_pid = 0;
    g_save_status.start_time = time(NULL);
    g_save_status.current_file = config->filename;
    g_save_status.saved_keys = 0;
    g_save_status.total_keys = 0;
    g_save_status.memory_used = 0;

    ret = save_rdb_child(config, db_interface);

    g_save_status.is_active = 0;
    g_save_status.child_pid = 0;

    return ret;
#else
    // Fork child process on Unix systems
    pid_t pid = fork();
    if (pid == -1) {
        return RBS_ERR_FORK;
    }

    if (pid == 0) {
        // Child process
        int exit_code = save_rdb_child(config, db_interface);
        exit(exit_code == RBS_OK ? 0 : 1);
    } else {
        // Parent process
        g_save_status.child_pid = pid;
        g_save_status.is_active = 1;
        g_save_status.start_time = time(NULL);
        g_save_status.current_file = config->filename;
        g_save_status.saved_keys = 0;
        g_save_status.total_keys = 0;
        g_save_status.memory_used = 0;
    }

    return RBS_OK;
#endif
}

// Wait for completion
int rbs_wait_completion(int timeout_ms) {
    if (!g_save_status.is_active) return RBS_OK;

    time_t start_time = time(NULL);
    int status;

    while (g_save_status.is_active) {
        pid_t result = waitpid(g_save_status.child_pid, &status, WNOHANG);

        if (result == g_save_status.child_pid) {
            // Child has completed
            g_save_status.is_active = 0;
            g_save_status.child_pid = 0;
            return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? RBS_OK : RBS_ERR;
        } else if (result == -1) {
            // Error in waitpid
            g_save_status.is_active = 0;
            g_save_status.child_pid = 0;
            return RBS_ERR;
        }

        // Check timeout
        if (timeout_ms > 0) {
            time_t elapsed = time(NULL) - start_time;
            if (elapsed * 1000 >= timeout_ms) {
                return RBS_ERR; // Timeout
            }
        }

        usleep(10000); // Sleep 10ms
    }

    return RBS_OK;
}

// Cancel save
int rbs_cancel_save(void) {
    if (!g_save_status.is_active) return RBS_OK;

    if (g_save_status.child_pid > 0) {
#ifdef _WIN32
        kill(g_save_status.child_pid, 1);
#else
        kill(g_save_status.child_pid, SIGTERM);
#endif
        waitpid(g_save_status.child_pid, NULL, 0);
    }

    g_save_status.is_active = 0;
    g_save_status.child_pid = 0;

    return RBS_OK;
}

// Get RDB file info (improved implementation)
int rbs_get_rdb_info(const char *filename, size_t *file_size, size_t *key_count) {
    if (!filename) return RBS_ERR_INVALID_ARGS;

    struct stat st;
    if (stat(filename, &st) != 0) {
        return RBS_ERR_IO;
    }

    if (file_size) *file_size = st.st_size;

    if (key_count) {
        *key_count = 0;

        // Try to read key count from RDB file
        FILE *fp = fopen(filename, "rb");
        if (fp) {
            // Skip magic (5 bytes) and version (4 bytes)
            if (fseek(fp, 9, SEEK_SET) == 0) {
                uint8_t opcode;

                // Look for SELECTDB opcode
                if (fread(&opcode, 1, 1, fp) == 1 && opcode == RDB_OPCODE_SELECTDB) {
                    // Skip database number
                    fseek(fp, 1, SEEK_CUR);

                    // Look for RESIZEDB opcode
                    if (fread(&opcode, 1, 1, fp) == 1 && opcode == RDB_OPCODE_RESIZEDB) {
                        uint32_t keys;
                        if (fread(&keys, 1, 4, fp) == 4) {
                            *key_count = keys;
                        }
                    }
                }
            }
            fclose(fp);
        }
    }

    return RBS_OK;
}

// Verify RDB file (simplified implementation)
int rbs_verify_rdb_file(const char *filename) {
    if (!filename) return RBS_ERR_INVALID_ARGS;

    FILE *fp = fopen(filename, "rb");
    if (!fp) return RBS_ERR_IO;

    char magic[6] = {0};
    if (fread(magic, 1, 5, fp) != 5) {
        fclose(fp);
        return RBS_ERR_IO;
    }

    fclose(fp);

    if (memcmp(magic, RDB_MAGIC, 5) != 0) {
        return RBS_ERR;
    }

    return RBS_OK;
}