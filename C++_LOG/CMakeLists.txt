cmake_minimum_required(VERSION 3.16)
project(SimpleLogger VERSION 1.0.0 LANGUAGES CXX)

# ---- C++20 标准，禁用编译器扩展 ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- 自动包含头文件目录 ----
include_directories(${CMAKE_SOURCE_DIR}/include)

# ---- 汇总源文件 ----
set(SOURCES
    src/log_file.cpp
    src/logger.cpp
    main.cpp
)

# ---- 构建可执行文件 ----
add_executable(simple_logger ${SOURCES})

# ---- Linux 平台链接 pthread 线程库 ----
if(UNIX)
    target_link_libraries(simple_logger PRIVATE pthread)
endif()

# ---- 编译选项建议 ----
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(simple_logger PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- 输出构建信息 ----
message(STATUS "C++ 标准: C++${CMAKE_CXX_STANDARD}")
message(STATUS "编译器:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
