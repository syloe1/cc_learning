#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>

// ============================================================================
// RingBufferBase — 共享数据区与读写索引
// ============================================================================
template <typename T, size_t Capacity> class RingBufferBase {
public:
  // 强制容量为2的幂，取模运算优化
  static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of 2");

  RingBufferBase() = default;

  // 禁止拷贝和移动（原子成员不可拷贝）
  RingBufferBase(const RingBufferBase &) = delete;
  RingBufferBase &operator=(const RingBufferBase &) = delete;

  // 数据区与索引各自对齐到不同缓存行（64 字节），避免 false sharing

  // alignas(64) 缓存行隔离，解决伪共享(false sharing)
  // CPU缓存行一般64字节，多个原子变量放同一缓存行时，互相写会频繁失效缓存
  alignas(64) T buffer_[Capacity]{};             // 数据存储区
  alignas(64) std::atomic<size_t> write_idx_{0}; // 生产者写位置索引
  alignas(64) std::atomic<size_t> read_idx_{0};  // 消费者读位置索引
};

// ============================================================================
// SPSCQueue — 单生产者 / 单消费者无锁队列
// ============================================================================
template <typename T, size_t Capacity>
class SPSCQueue : public RingBufferBase<T, Capacity> {
public:
  SPSCQueue() = default;

  // ---- 单元素操作 ----

  bool enqueue(const T &item) { return enqueue_bulk(&item, 1) == 1; }

  bool dequeue(T &item) { return dequeue_bulk(&item, 1) == 1; }

  // ---- 批量入队 ----
  // 1. CAS 预留写槽位
  // 2. 顺序填充数据
  // 3. 等待前一批次提交完成
  // 4. release 更新 commit_idx_
  size_t enqueue_bulk(const T *items, size_t count) {
    size_t write_idx = this->write_idx_.load(std::memory_order_relaxed);
    size_t read_idx = this->read_idx_.load(std::memory_order_acquire);

    // 计算可用空间
    size_t available = Capacity - (write_idx - read_idx);
    if (count > available)
      count = available;
    if (count == 0)
      return 0;

    // CAS 预留写槽位
    // 2. CAS 预留批量槽位：抢占 write_idx_，提前占用N个位置
    while (!this->write_idx_.compare_exchange_weak(write_idx, write_idx + count,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
      read_idx = this->read_idx_.load(std::memory_order_acquire);
      available = Capacity - (write_idx - read_idx);
      if (count > available)
        count = available;
      if (count == 0)
        return 0;
    }
    // CAS 成功后 write_idx 即为预留起始位置（旧值）

    // 顺序填充数据
    for (size_t i = 0; i < count; ++i) {
      this->buffer_[(write_idx + i) & (Capacity - 1)] = items[i];
    }

    // 等待前一批次提交（保证 commit_idx_ 单调递增且有序）
    while (commit_idx_.load(std::memory_order_acquire) != write_idx) {
      // 自旋等待
    }

    // release 提交本批次
    commit_idx_.store(write_idx + count, std::memory_order_release);
    return count;
  }

  // ---- 批量出队 ----
  // 1. 读取可用范围
  // 2. 先拷贝数据（此时 read_idx_ 未推进，保护槽位不被覆盖）
  // 3. CAS 更新 read_idx_，释放槽位给生产者
  size_t dequeue_bulk(T *out, size_t count) {
    // 读取已提交的数据边界 commit_idx
    size_t read_idx = this->read_idx_.load(std::memory_order_relaxed);
    size_t commit_idx = commit_idx_.load(std::memory_order_acquire);
    // 可读取元素总数
    size_t available = commit_idx - read_idx;
    size_t actual = (count <= available) ? count : available;
    if (actual == 0)
      return 0;

    // 先拷贝数据
    for (size_t i = 0; i < actual; ++i) {
      out[i] = this->buffer_[(read_idx + i) & (Capacity - 1)];
    }

    // 再 CAS 更新读索引（release 保证数据拷贝对生产者可见）
    size_t expected = read_idx;
    while (!this->read_idx_.compare_exchange_weak(expected, read_idx + actual,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
      // SPSC 下只可能因 spurious failure 进入此处，重置 expected 后重试
      expected = read_idx;
    }

    return actual;
  }

private:
  // 提交索引，独立缓存行
  alignas(64) std::atomic<size_t> commit_idx_{0};
};

// ============================================================================
// MPMCLockedQueue — 多生产者 / 多消费者加锁队列
// ============================================================================
template <typename T, size_t Capacity>
class MPMCLockedQueue : public RingBufferBase<T, Capacity> {
public:
  MPMCLockedQueue() = default;

  // ---- 单元素操作 ----

  bool enqueue(const T &item) { return enqueue_bulk(&item, 1) == 1; }

  bool dequeue(T &item) { return dequeue_bulk(&item, 1) == 1; }

  // ---- 批量入队（mutex 保护） ----

  size_t enqueue_bulk(const T *items, size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t write_idx = this->write_idx_.load(std::memory_order_relaxed);
    size_t read_idx = this->read_idx_.load(std::memory_order_acquire);

    size_t available = Capacity - (write_idx - read_idx);
    if (count > available)
      count = available;
    if (count == 0)
      return 0;

    for (size_t i = 0; i < count; ++i) {
      this->buffer_[(write_idx + i) & (Capacity - 1)] = items[i];
    }

    this->write_idx_.store(write_idx + count, std::memory_order_release);
    return count;
  }

  // ---- 批量出队（mutex 保护） ----

  size_t dequeue_bulk(T *out, size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t read_idx = this->read_idx_.load(std::memory_order_relaxed);
    size_t write_idx = this->write_idx_.load(std::memory_order_acquire);

    size_t available = write_idx - read_idx;
    if (count > available)
      count = available;
    if (count == 0)
      return 0;

    for (size_t i = 0; i < count; ++i) {
      out[i] = this->buffer_[(read_idx + i) & (Capacity - 1)];
    }

    this->read_idx_.store(read_idx + count, std::memory_order_release);
    return count;
  }

private:
  std::mutex mutex_;
};
