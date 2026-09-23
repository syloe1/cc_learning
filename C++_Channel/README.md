# Ring Buffer — 高性能无锁/加锁环形队列

基于 C++17 实现的环形缓冲区（Ring Buffer）库，提供**单生产者/单消费者（SPSC）无锁队列**和**多生产者/多消费者（MPMC）加锁队列**。

## 项目结构

```
C++_Channel/
├── CMakeLists.txt
├── README.md
├── include/
│   └── ring_buffer.h    # 核心模板类
├── src/
│   └── main.cpp         # 演示与性能测试
└── tests/
    └── test_ring_buffer.cpp  # 单元测试
```

## 构建与运行

```bash
cd C++_Channel
mkdir build && cd build
cmake ..
make -j$(nproc)

# 运行演示程序
./ring_buffer_demo

# 运行单元测试
./ring_buffer_test
```

## 核心类说明

### RingBufferBase<T, Capacity>

基类，管理环形队列的**数据区**和**读写索引**。

- `buffer_[Capacity]` — 对齐到 64 字节缓存行的数据存储区
- `write_idx_` — 写索引（原子变量，独立缓存行）
- `read_idx_` — 读索引（原子变量，独立缓存行）
- `Capacity` 必须为 **2 的幂**（编译期 `static_assert` 强制检查），利用位掩码 `index & (Capacity - 1)` 实现高效取模

### SPSCQueue<T, Capacity>

继承 `RingBufferBase`，实现**单生产者/单消费者无锁队列**。

- **额外成员** `commit_idx_` — 提交索引（独立缓存行），用于保证**批次写入的有序提交**
- **批量入队 `enqueue_bulk`**：
  1. CAS 原子预留写槽位（`compare_exchange_weak` + `acq_rel`）
  2. 顺序填充数据到缓冲区
  3. 自旋等待前一批次提交完成
  4. `memory_order_release` 更新 `commit_idx_`
- **批量出队 `dequeue_bulk`**：
  1. 读取 `commit_idx_`（acquire）获取可读范围
  2. CAS 原子预留读槽位
  3. 拷贝数据到输出缓冲区
- 提供单元素封装 `enqueue` / `dequeue`

### MPMCLockedQueue<T, Capacity>

继承 `RingBufferBase`，使用 **`std::mutex`** 保护入队/出队操作，支持多线程并发访问。

- 入队/出队均以 `std::lock_guard` 加锁
- 无需 `commit_idx_`（锁本身保证有序性）

## 技术要点

### 缓存行填充（Cache Line Padding）

所有原子索引（`write_idx_`、`read_idx_`、`commit_idx_`）和数据区均使用 `alignas(64)` 对齐到独立的 64 字节缓存行，杜绝 **false sharing**（伪共享），避免不同核心修改相邻内存位置时引发不必要的缓存一致性协议开销。

### CAS（Compare-And-Swap）无锁同步

`SPSCQueue` 通过 `compare_exchange_weak` 实现对读写索引的原子预留：

- **成功路径**使用 `memory_order_acq_rel`，同时获取前值并发布新值
- **失败路径**使用 `memory_order_relaxed`，仅重试循环，避免不必要的同步开销

### 内存屏障（Memory Barrier）

| 操作 | 内存序 | 说明 |
|------|--------|------|
| 填充数据后更新 `commit_idx_` | `release` | 保证所有数据写入对消费者可见 |
| 消费者读取 `commit_idx_` | `acquire` | 保证能看到生产者已提交的全部数据 |
| CAS 成功 | `acq_rel` | 原子预留槽位的同时建立 happen-before 关系 |
| CAS 失败 | `relaxed` | 仅在重试循环中读取当前值，无需同步 |

### 2 的幂容量

`Capacity` 必须为 2 的幂，使得取模运算 `index % Capacity` 可替换为高效的按位与 `index & (Capacity - 1)`，避免整数除法指令开销。
