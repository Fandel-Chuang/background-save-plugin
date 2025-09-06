cmake_minimum_required(VERSION 3.10)
project(background-save VERSION 1.0.0 LANGUAGES C)

# 项目描述
set(PROJECT_DESCRIPTION "High-performance background data persistence library inspired by Redis RDB")
set(PROJECT_HOMEPAGE_URL "https://github.com/your-repo/background-save-plugin")

# 设置 C 标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)  # 禁用编译器扩展，确保标准兼容性

# 编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -DDEBUG")
endif()

# 包含目录
include_directories(include)

# 源文件
set(SOURCES
    src/rdb_save_background.c
    src/rdb_core.c
    src/redis_compatibility.c
    src/memory_utils.c
    src/process_utils.c
    src/log_utils.c
)

# 创建动态库
add_library(background_save SHARED ${SOURCES})

# 设置库的版本
set_target_properties(background_save PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "include/background_save.h"
)

# 链接系统库
target_link_libraries(background_save
    PRIVATE
    pthread
    m
)

# 安装规则
install(TARGETS background_save
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include
)

# 创建示例程序
add_executable(example examples/example.c)
target_link_libraries(example background_save)

# 可选：创建测试
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_executable(test_background_save tests/test_background_save.c)
    target_link_libraries(test_background_save background_save)
    add_test(NAME background_save_test COMMAND test_background_save)
endif()
