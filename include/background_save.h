#ifndef BACKGROUND_SAVE_H
#define BACKGROUND_SAVE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

// Error codes
typedef enum {
    RBS_OK = 0,
    RBS_ERR = -1,
    RBS_ERR_FORK = -2,
    RBS_ERR_INPROGRESS = -3,
    RBS_ERR_INVALID_ARGS = -4,
    RBS_ERR_IO = -5,
    RBS_ERR_MEMORY = -6
} rbs_error_t;

// Log levels
typedef enum {
    RBS_LOG_DEBUG = 0,
    RBS_LOG_INFO = 1,
    RBS_LOG_WARN = 2,
    RBS_LOG_ERROR = 3
} rbs_log_level_t;

/* 保存配置结构 */
typedef struct {
    const char *filename;           /* RDB 文件名 */
    int compression_enabled;        /* 是否启用压缩 */
    int checksum_enabled;           /* 是否启用校验和 */
    int fsync_enabled;              /* 是否启用 fsync */

    /* 回调函数 */
    void (*progress_callback)(size_t saved_keys, size_t total_keys, void *userdata);
    void (*completion_callback)(int status, const char *error_msg, void *userdata);
    void (*log_callback)(rbs_log_level_t level, const char *message, void *userdata);
    void *userdata;                 /* 回调函数用户数据 */

    /* 内存限制 */
    size_t max_memory_usage;        /* 最大内存使用量 (字节) */

    /* 性能参数 */
    int key_save_delay;             /* 键保存延迟 (微秒) */
    int incremental_fsync;          /* 增量 fsync */
} rbs_config_t;

/* 保存状态结构 */
typedef struct {
    pid_t child_pid;                /* 子进程 PID */
    int is_active;                  /* 是否正在进行 */
    size_t saved_keys;              /* 已保存键数量 */
    size_t total_keys;              /* 总键数量 */
    time_t start_time;              /* 开始时间 */
    size_t memory_used;             /* 已使用内存 */
    const char *current_file;       /* 当前保存文件 */
} rbs_status_t;

/* 键值对结构 */
typedef struct {
    char *key;                      /* 键 */
    void *value;                    /* 值数据 */
    size_t value_size;              /* 值大小 */
    int value_type;                 /* 值类型 */
    int64_t expire_time;            /* 过期时间 (毫秒时间戳, 0 表示不过期) */
} rbs_keyvalue_t;

/* 数据库接口结构 */
typedef struct {
    /* 获取所有键的回调 */
    int (*get_all_keys)(char ***keys, size_t *count, void *userdata);

    /* 获取键值的回调 */
    int (*get_key_value)(const char *key, rbs_keyvalue_t *kv, void *userdata);

    /* 释放键值的回调 */
    void (*free_key_value)(rbs_keyvalue_t *kv, void *userdata);

    /* 用户数据 */
    void *userdata;
} rbs_database_interface_t;

/* 主要 API 函数 */

/**
 * 初始化后台保存库
 * @return RBS_OK 成功，其他值表示错误
 */
int rbs_init(void);

/**
 * 清理后台保存库
 */
void rbs_cleanup(void);

/**
 * 开始后台保存
 * @param config 保存配置
 * @param db_interface 数据库接口
 * @return RBS_OK 成功，其他值表示错误
 */
int rbs_save_background(const rbs_config_t *config,
                       const rbs_database_interface_t *db_interface);

/**
 * 获取当前保存状态
 * @param status 状态结构指针
 * @return RBS_OK 成功，其他值表示错误
 */
int rbs_get_status(rbs_status_t *status);

/**
 * 等待后台保存完成
 * @param timeout_ms 超时时间 (毫秒)，0 表示无限等待
 * @return RBS_OK 成功，其他值表示错误
 */
int rbs_wait_completion(int timeout_ms);

/**
 * 取消后台保存
 * @return RBS_OK 成功，其他值表示错误
 */
int rbs_cancel_save(void);

/**
 * 检查是否有后台保存正在进行
 * @return 1 正在进行，0 未进行
 */
int rbs_is_save_in_progress(void);

/**
 * 获取错误信息
 * @param error_code 错误代码
 * @return 错误信息字符串
 */
const char* rbs_get_error_string(int error_code);

/**
 * 设置默认配置
 * @param config 配置结构指针
 */
void rbs_set_default_config(rbs_config_t *config);

/**
 * 验证配置
 * @param config 配置结构指针
 * @return RBS_OK 配置有效，其他值表示配置错误
 */
int rbs_validate_config(const rbs_config_t *config);

/* 工具函数 */

/**
 * 获取 RDB 文件信息
 * @param filename RDB 文件名
 * @param file_size 文件大小输出
 * @param key_count 键数量输出
 * @return RBS_OK 成功，其他值表示错误
 */
int rbs_get_rdb_info(const char *filename, size_t *file_size, size_t *key_count);

/**
 * 验证 RDB 文件
 * @param filename RDB 文件名
 * @return RBS_OK 文件有效，其他值表示文件损坏或错误
 */
int rbs_verify_rdb_file(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* BACKGROUND_SAVE_H */