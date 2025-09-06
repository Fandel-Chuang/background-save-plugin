# 后台保存库

[![构建状态](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/your-repo/background-save-plugin)
[![许可证](https://img.shields.io/badge/license-MIT%20with%20Restrictions-blue.svg)](LICENSE)
[![版本](https://img.shields.io/badge/version-1.0.0-orange.svg)](https://github.com/your-repo/background-save-plugin/releases)

一个高性能、轻量级的 C 语言后台数据持久化库，灵感来源于 Redis 的 RDB 保存机制。该库为游戏服务器提供高效的非阻塞数据序列化和存储功能。

- 初次接触后台保存插件？从[什么是后台保存插件](#什么是后台保存插件)和[快速开始](#快速开始)开始
- 准备从源码构建？跳转到[从源码构建](#从源码构建)
- 想要贡献代码？查看[代码贡献](#代码贡献)部分
- 寻找详细文档？导航到[文档](#文档)

## 目录

- [后台保存库](#后台保存库)
  - [目录](#目录)
  - [什么是后台保存插件？](#什么是后台保存插件)
    - [核心特性](#核心特性)
  - [为什么选择后台保存插件？](#为什么选择后台保存插件)
  - [架构概览](#架构概览)
    - [系统架构](#系统架构)
    - [数据流架构](#数据流架构)
    - [后台保存流程](#后台保存流程)
  - [快速开始](#快速开始)
    - [快速安装](#快速安装)
    - [基本用法](#基本用法)
    - [配置](#配置)
  - [核心库](#核心库)
    - [libbgsave](#libbgsave)
    - [libremotebgsave](#libremotebgsave)
  - [API 参考](#api-参考)
    - [数据类型](#数据类型)
    - [函数参考](#函数参考)
  - [从源码构建](#从源码构建)
    - [在 Windows 上构建](#在-windows-上构建)
    - [在 Linux 上构建](#在-linux-上构建)
    - [在 macOS 上构建](#在-macos-上构建)
    - [构建标志和选项](#构建标志和选项)
  - [测试](#测试)
  - [性能](#性能)
  - [文档](#文档)
  - [代码贡献](#代码贡献)
  - [许可证](#许可证)

## 什么是后台保存插件？

后台保存插件是一个高性能、受 Redis 启发的持久化解决方案，专为游戏服务器设计。它利用 Redis 经过验证的 RDB（Redis 数据库）序列化格式和后台保存机制，为游戏服务器提供高效的非阻塞数据存储功能。

### 核心特性

该插件在各种游戏服务器场景中表现出色：

- **非阻塞持久化：** 利用基于 fork 的后台保存，避免在保存操作期间阻塞主游戏线程
- **高效序列化：** 实现带 LZF 压缩的 Redis RDB 格式，实现最佳存储效率
- **本地和远程存储：** 支持本地文件存储和远程分布式存储解决方案
- **数据完整性：** 包含 CRC64 校验和原子写入操作，确保数据一致性
- **高性能：** 针对低延迟操作优化，内存开销最小
- **可扩展性：** 设计用于处理大规模游戏服务器部署，支持数千名并发玩家
- **跨平台：** 在 Windows、Linux 和 macOS 环境下无缝工作

## 为什么选择后台保存插件？

后台保存插件专为需要可靠、高性能持久化解决方案的游戏开发者设计：

- **性能：** 实现亚毫秒级保存启动时间，通过异步操作对游戏服务器性能影响最小
- **可靠性：** 基于 Redis 久经考验的 RDB 格式构建，确保服务器重启时的数据持久性和一致性
- **灵活性：** 模块化架构允许与现有游戏服务器框架集成，无需大规模重构
- **可扩展性：** 处理从小型独立游戏到大型 MMO 部署，具有可配置的性能参数
- **开发者友好：** 简单的 C API，具有全面的文档和示例，可快速集成

## 架构概览

后台保存插件由两个主要组件组成：

### 系统架构

```
┌─────────────────────┐    ┌──────────────────────┐
│     libbgsave       │    │  libremotebgsave     │
│    (本地存储)        │    │    (远程存储)        │
│                     │    │                      │
│ ┌─────────────────┐ │    │ ┌──────────────────┐ │
│ │   RDB 引擎      │ │    │ │    网络层        │ │
│ │   - 序列化      │ │    │ │  - HTTP/HTTPS    │ │
│ │   - 压缩        │ │    │ │  - 传输          │ │
│ │   - 校验和      │ │    │ │  - 重试逻辑      │ │
│ └─────────────────┘ │    │ └──────────────────┘ │
│                     │    │                      │
│ ┌─────────────────┐ │    │ ┌──────────────────┐ │
│ │   文件管理器    │ │    │ │    数据同步      │ │
│ │   - 原子操作    │ │    │ │  - 一致性        │ │
│ │   - 备份        │ │    │ │  - 版本控制      │ │
│ └─────────────────┘ │    │ └──────────────────┘ │
└─────────────────────┘    └──────────────────────┘
```

### 数据流架构

后台保存插件使用两阶段保存过程：

1. **本地保存阶段：** 数据被序列化为 RDB 格式并存储在本地
2. **远程同步阶段：** 数据被传输到远程存储（可选）

### 后台保存流程

1. 游戏服务器请求保存操作
2. 系统 fork 一个后台进程
3. 后台进程序列化游戏数据为 RDB 格式
4. 应用 LZF 压缩和 CRC64 校验和
5. 原子性写入本地文件系统
6. 可选：传输到远程存储
7. 通知游戏服务器完成状态

## 快速开始

### 快速安装

**使用预构建二进制文件：**
```bash
# 下载最新版本
wget https://github.com/your-repo/background-save-plugin/releases/latest/download/bgsave-plugin-windows.zip
unzip bgsave-plugin-windows.zip
```

**使用包管理器（即将推出）：**
```bash
# 即将推出
vcpkg install background-save-plugin
```

### 基本用法

```c
#include "background_save.h"

// 数据库接口实现
int get_all_keys(char ***keys, size_t *count, void *userdata) {
    // 您的实现：获取所有键
    return RBS_OK;
}

int get_key_value(const char *key, rbs_keyvalue_t *kv, void *userdata) {
    // 您的实现：获取键值对
    return RBS_OK;
}

void free_key_value(rbs_keyvalue_t *kv, void *userdata) {
    // 您的实现：释放键值对
}

int main() {
    // 初始化库
    rbs_init();

    // 配置保存选项
    rbs_config_t config;
    rbs_set_default_config(&config);
    config.filename = "game_data.rdb";
    config.compression_enabled = 1;

    // 设置数据库接口
    rbs_database_interface_t db_interface = {
        .get_all_keys = get_all_keys,
        .get_key_value = get_key_value,
        .free_key_value = free_key_value,
        .userdata = NULL
    };

    // 开始后台保存
    int result = rbs_save_background(&config, &db_interface);
    if (result == RBS_OK) {
        printf("后台保存成功启动\n");

        // 等待完成
        rbs_wait_completion(0);  // 0 = 无限等待
    }

    // 清理
    rbs_cleanup();
    return 0;
}
```

### 配置

创建配置文件 `bgsave.conf`：

```ini
# 后台保存插件配置

[local]
data_dir = ./game_saves
compression = lzf
checksum = true
backup_count = 3
save_interval = 300  # 秒

[remote]
enabled = true
endpoint = https://your-game-cloud.com/api/saves
timeout = 30
retry_count = 3
format = rdb
```

## 核心库

### libbgsave

本地存储库提供高效的基于文件的持久化：

**核心函数：**
- `rbs_init()` - 初始化后台保存系统
- `rbs_save_background()` - 开始异步保存操作
- `rbs_get_status()` - 获取保存状态
- `rbs_wait_completion()` - 等待保存完成
- `rbs_cancel_save()` - 取消保存操作

**特性：**
- 基于 fork 的后台保存
- LZF 压缩
- 原子文件操作
- 自动备份轮换
- CRC64 完整性检查

### libremotebgsave

远程存储库处理分布式保存操作：

**核心函数：**
- `remote_rbs_init()` - 初始化远程保存系统
- `remote_rbs_upload()` - 上传保存数据到远程存储
- `remote_rbs_download()` - 从远程存储下载保存数据
- `remote_rbs_sync()` - 同步本地和远程保存
- `remote_rbs_list_remote()` - 列出远程保存文件

**特性：**
- HTTP/HTTPS 协议支持
- 指数退避自动重试
- 带宽限制
- 冲突解决
- 数据完整性验证

## API 参考

### 数据类型

```c
typedef enum {
    RBS_OK = 0,
    RBS_ERR = -1,
    RBS_ERR_FORK = -2,
    RBS_ERR_INPROGRESS = -3,
    RBS_ERR_INVALID_ARGS = -4,
    RBS_ERR_IO = -5,
    RBS_ERR_MEMORY = -6
} rbs_result_t;

typedef struct {
    const char *filename;
    int compression_enabled;
    int checksum_enabled;
    int fsync_enabled;
    size_t max_memory_usage;
    int key_save_delay;
    int incremental_fsync;

    // 回调函数
    void (*progress_callback)(size_t saved_keys, size_t total_keys, void *userdata);
    void (*completion_callback)(int status, const char *error_msg, void *userdata);
    void (*log_callback)(rbs_log_level_t level, const char *message, void *userdata);
    void *userdata;
} rbs_config_t;
```

### 函数参考

**本地存储 API：**

```c
// 初始化后台保存系统
int rbs_init(void);

// 异步保存操作
int rbs_save_background(const rbs_config_t *config,
                       const rbs_database_interface_t *db_interface);

// 获取操作状态
int rbs_get_status(rbs_status_t *status);

// 等待完成
int rbs_wait_completion(int timeout_ms);

// 检查是否有保存正在进行
int rbs_is_save_in_progress(void);

// 清理资源
void rbs_cleanup(void);
```

## 从源码构建

### 在 Windows 上构建

**先决条件：**
- Visual Studio 2019 或更高版本
- CMake 3.15+
- vcpkg（推荐）

**构建步骤：**

1. 克隆仓库：
   ```cmd
   git clone https://github.com/your-repo/background-save-plugin.git
   cd background-save-plugin
   ```

2. 使用 CMake 配置：
   ```cmd
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
   ```

3. 构建项目：
   ```cmd
   cmake --build . --config Release
   ```

4. 运行测试：
   ```cmd
   ctest -C Release
   ```

### 在 Linux 上构建

**先决条件：**
- GCC 9+ 或 Clang 10+
- CMake 3.15+
- Make

**构建步骤：**

1. 安装依赖：
   ```bash
   # Ubuntu/Debian
   sudo apt-get update
   sudo apt-get install -y build-essential cmake libssl-dev zlib1g-dev

   # CentOS/RHEL
   sudo yum groupinstall "Development Tools"
   sudo yum install cmake openssl-devel zlib-devel
   ```

2. 克隆并构建：
   ```bash
   git clone https://github.com/your-repo/background-save-plugin.git
   cd background-save-plugin
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   make -j$(nproc)
   ```

### 在 macOS 上构建

**先决条件：**
- Xcode 命令行工具
- CMake（通过 Homebrew）

**构建步骤：**

1. 安装依赖：
   ```bash
   xcode-select --install
   brew install cmake openssl zlib
   ```

2. 构建：
   ```bash
   git clone https://github.com/your-repo/background-save-plugin.git
   cd background-save-plugin
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   make -j$(sysctl -n hw.ncpu)
   ```

### 构建标志和选项

**CMake 选项：**

```bash
# 启用/禁用特性
cmake .. -DRBS_ENABLE_COMPRESSION=ON      # LZF 压缩支持
cmake .. -DRBS_ENABLE_REMOTE=ON           # 远程存储支持
cmake .. -DRBS_ENABLE_TESTS=ON            # 构建测试套件
cmake .. -DRBS_ENABLE_EXAMPLES=ON         # 构建示例

# 性能选项
cmake .. -DRBS_OPTIMIZE_SIZE=OFF          # 优化速度而不是大小
cmake .. -DRBS_USE_JEMALLOC=ON            # 使用 jemalloc 分配器

# 调试选项
cmake .. -DCMAKE_BUILD_TYPE=Debug         # 调试构建
cmake .. -DRBS_ENABLE_LOGGING=ON          # 启用调试日志
cmake .. -DRBS_ENABLE_PROFILING=ON        # 启用性能分析
```

## 测试

运行综合测试套件：

```bash
# 构建并运行所有测试
cd build
make test

# 运行特定测试类别
ctest -L "unit"        # 仅单元测试
ctest -L "integration" # 仅集成测试
ctest -L "performance" # 仅性能测试

# 详细输出运行测试
ctest --verbose

# 使用内存检查运行测试（需要 valgrind）
ctest -D ExperimentalMemCheck
```

## 性能

**性能基于 Redis RDB 格式特性：**

该插件利用 Redis 经过验证的 RDB 序列化性能。实际性能会根据以下因素而变化：

- **硬件规格**（CPU、RAM、存储类型）
- **数据大小和复杂性**（简单结构 vs 复杂嵌套数据）
- **压缩设置**（LZF 压缩权衡）
- **网络条件**（远程保存）
- **系统负载**（并发操作）

**预期性能特征：**
- ✅ **非阻塞保存** - 后台保存期间主线程继续运行
- ✅ **内存高效** - 基于 fork 的写时复制机制
- ✅ **快速序列化** - Redis RDB 格式高度优化
- ✅ **可扩展** - 性能随硬件能力扩展

## 文档

- [API 文档](docs/api.md) - 完整的 API 参考
- [集成指南](docs/integration.md) - 逐步集成说明
- [配置参考](docs/configuration.md) - 所有配置选项
- [性能调优](docs/performance.md) - 优化指南
- [故障排除](docs/troubleshooting.md) - 常见问题和解决方案
- [示例](examples/) - 示例代码和用例

## 代码贡献

我们欢迎对后台保存插件项目的贡献！请遵循以下指南：

**开始：**
1. Fork 仓库
2. 创建功能分支（`git checkout -b feature/amazing-feature`）
3. 进行更改
4. 为新功能添加测试
5. 确保所有测试通过
6. 提交拉取请求

**编码标准：**
- 遵循 C99 标准
- 使用一致的缩进（4个空格）
- 为公共 API 添加全面注释
- 为新功能包含单元测试
- 根据需要更新文档

**提交前：**
- 运行 `make format` 格式化代码
- 运行 `make lint` 检查代码风格
- 确保所有测试通过 `make test`
- 使用您的更改更新 CHANGELOG.md

更多详情，请参阅 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

本项目在 MIT 许可证下获得许可 - 有关详细信息，请参阅 [LICENSE](LICENSE) 文件。

**第三方组件：**
- LZF 压缩库（BSD-2-Clause）- *可选，可以禁用*
- CRC64 校验和实现（公共域）
- 仅使用标准 C 库 - **无需外部依赖**

**我们不包含的内容：**
- ❌ 无 Lua 脚本引擎（不像 Redis）
- ❌ 无网络服务器功能
- ❌ 无 Redis 协议实现
- ❌ 纯 C99 实现，依赖最少

---

**维护者：** 钟芳道 (DJD) [zhongfangdao888@gmail.com](mailto:zhongfangdao888@gmail.com)
**项目主页：** [https://github.com/your-repo/background-save-plugin](https://github.com/your-repo/background-save-plugin)
**问题跟踪器：** [https://github.com/your-repo/background-save-plugin/issues](https://github.com/your-repo/background-save-plugin/issues)
