# cc_learning

C++ 学习练手项目集合。每个子目录是一个独立项目，各自有独立的构建配置，互不依赖。

| 项目 | 简介 | 标准 | 构建 |
| --- | --- | --- | --- |
| [logger/](logger/) | 简易异步日志库 | C++20 | CMake |
| [ring_buffer/](ring_buffer/) | 环形缓冲区 / 无锁 SPSC 队列 | C++17 | CMake |
| [thread_pool/](thread_pool/) | 线程池（动态扩缩容、优先级队列） | C++11 | Makefile |
| [reactor_net/](reactor_net/) | 仿 muduo 的 Reactor 网络库 + HTTP 静态服务器 | C++17 | CMake |
| [pool/](pool/) | 内存池（arena + free list 分配器） | C11 | gcc |
| [struct/](struct/) | 结构体内存布局 / 512 字节文件头 | C11 | Makefile |

## 构建

各项目独立构建，进入对应目录后：

- logger / ring_buffer / reactor_net：

  ```bash
  cmake -B build && cmake --build build
  ```

- thread_pool / struct（就地编译）：

  ```bash
  make
  ```

- pool：

  ```bash
  gcc -Wall pool.c -o pool
  ```
