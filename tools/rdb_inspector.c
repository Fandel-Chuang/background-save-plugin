/*
 * Background Save Plugin - RDB File Inspector
 * Copyright (c) 2024 钟芳道 (DJD)
 *
 * Simple tool to inspect RDB files
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define RDB_MAGIC "REDIS"
#define RDB_OPCODE_EOF 0xFF
#define RDB_OPCODE_SELECTDB 0xFE
#define RDB_OPCODE_EXPIRETIME 0xFD
#define RDB_OPCODE_EXPIRETIME_MS 0xFC
#define RDB_OPCODE_RESIZEDB 0xFB

// Read length encoding
static int read_length(FILE *fp, uint32_t *length) {
    uint8_t first_byte;
    if (fread(&first_byte, 1, 1, fp) != 1) return -1;

    int type = (first_byte & 0xC0) >> 6;

    switch (type) {
        case 0: // 6-bit length
            *length = first_byte & 0x3F;
            return 0;

        case 1: // 14-bit length
            {
                uint8_t second_byte;
                if (fread(&second_byte, 1, 1, fp) != 1) return -1;
                *length = ((first_byte & 0x3F) << 8) | second_byte;
                return 0;
            }

        case 2: // 32-bit length
            if (fread(length, 4, 1, fp) != 1) return -1;
            return 0;

        default:
            return -1; // Special encoding
    }
}

// Read string
static char* read_string(FILE *fp) {
    uint32_t length;
    if (read_length(fp, &length) != 0) return NULL;

    if (length == 0) {
        char *str = malloc(1);
        if (str) str[0] = '\0';
        return str;
    }

    char *str = malloc(length + 1);
    if (!str) return NULL;

    if (fread(str, 1, length, fp) != length) {
        free(str);
        return NULL;
    }

    str[length] = '\0';
    return str;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <rdb_file>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("Error: Cannot open file '%s'\n", filename);
        return 1;
    }

    printf("RDB File Inspector\n");
    printf("==================\n");
    printf("File: %s\n\n", filename);

    // Check magic
    char magic[6] = {0};
    if (fread(magic, 1, 5, fp) != 5) {
        printf("Error: Cannot read magic bytes\n");
        fclose(fp);
        return 1;
    }

    if (memcmp(magic, RDB_MAGIC, 5) != 0) {
        printf("Error: Invalid RDB magic: %.5s (expected REDIS)\n", magic);
        fclose(fp);
        return 1;
    }

    printf("Magic: %.5s ✓\n", magic);

    // Read version
    char version[5] = {0};
    if (fread(version, 1, 4, fp) != 4) {
        printf("Error: Cannot read version\n");
        fclose(fp);
        return 1;
    }

    printf("Version: %.4s\n\n", version);

    // Parse content
    int key_count = 0;
    int current_db = -1;

    while (1) {
        uint8_t opcode;
        if (fread(&opcode, 1, 1, fp) != 1) {
            printf("Unexpected end of file\n");
            break;
        }

        switch (opcode) {
            case RDB_OPCODE_EOF:
                printf("EOF marker reached\n");

                // Try to read checksum
                uint64_t checksum;
                if (fread(&checksum, 8, 1, fp) == 1) {
                    printf("Checksum: 0x%016llx\n", (unsigned long long)checksum);
                }

                printf("\nSummary:\n");
                printf("  Total keys: %d\n", key_count);
                if (current_db >= 0) {
                    printf("  Database: %d\n", current_db);
                }

                fclose(fp);
                return 0;

            case RDB_OPCODE_SELECTDB:
                {
                    uint8_t db_num;
                    if (fread(&db_num, 1, 1, fp) != 1) {
                        printf("Error reading database number\n");
                        fclose(fp);
                        return 1;
                    }
                    current_db = db_num;
                    printf("Select database: %d\n", db_num);
                }
                break;

            case RDB_OPCODE_RESIZEDB:
                {
                    uint32_t db_size, expire_size;
                    if (fread(&db_size, 1, 4, fp) != 4 || fread(&expire_size, 1, 4, fp) != 4) {
                        printf("Error reading resize DB info\n");
                        fclose(fp);
                        return 1;
                    }
                    printf("Resize DB: %u keys, %u expires\n", db_size, expire_size);
                }
                break;

            case RDB_OPCODE_EXPIRETIME:
                {
                    uint32_t expire_time;
                    if (fread(&expire_time, 1, 4, fp) != 4) {
                        printf("Error reading expire time\n");
                        fclose(fp);
                        return 1;
                    }
                    printf("Expire time (seconds): %u\n", expire_time);
                }
                break;

            case RDB_OPCODE_EXPIRETIME_MS:
                {
                    uint64_t expire_time;
                    if (fread(&expire_time, 1, 8, fp) != 8) {
                        printf("Error reading expire time (ms)\n");
                        fclose(fp);
                        return 1;
                    }
                    printf("Expire time (ms): %llu\n", (unsigned long long)expire_time);
                }
                break;

            default:
                // This should be a value type followed by key-value pair
                {
                    printf("Value type: %d\n", opcode);

                    // Read key
                    char *key = read_string(fp);
                    if (!key) {
                        printf("Error reading key\n");
                        fclose(fp);
                        return 1;
                    }

                    // Read value
                    char *value = read_string(fp);
                    if (!value) {
                        printf("Error reading value\n");
                        free(key);
                        fclose(fp);
                        return 1;
                    }

                    printf("Key: \"%s\"\n", key);
                    printf("Value: \"%s\" (%zu bytes)\n", value, strlen(value));
                    printf("---\n");

                    key_count++;

                    free(key);
                    free(value);
                }
                break;
        }
    }

    fclose(fp);
    return 0;
}