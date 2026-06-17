cmake_minimum_required(VERSION 3.14)
project(RingBuffer VERSION 1.0.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 头文件路径
include_directories(${CMAKE_SOURCE_DIR}/include)

# 查找 pthread
find_package(Threads REQUIRED)

# 可执行文件 1: 演示程序
add_executable(ring_buffer_demo src/main.cpp)
target_link_libraries(ring_buffer_demo PRIVATE Threads::Threads)

# 可执行文件 2: 测试程序
add_executable(ring_buffer_test tests/test_ring_buffer.cpp)
target_link_libraries(ring_buffer_test PRIVATE Threads::Threads)
