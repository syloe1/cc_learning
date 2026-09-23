# timer — 有序双链表定时器

用「按到期时间排序的双向链表」实现的定时器管理器，`O(1)` 删除、`O(n)` 插入。
被动式设计：自己不跑任何线程，靠外部循环周期性调用 `tick()` 检查过期——
和 Redis 的 `serverCron` 是同一个思路。

## 项目结构

```
timer/
├── Makefile
├── README.md
├── main.cpp    # 演示：乱序插入 / 触发 / 删除 / 重排
└── timer.cc    # 核心实现（TimerNode + TimerListMgr）
```

`timer.cc` 是单文件写法（声明和实现写在一起、没有拆头文件），所以 `main.cpp`
直接 `#include "timer.cc"`。项目若继续长大，应拆成 `timer.h` + `timer.cc`。

## 构建与运行

```bash
make        # 编译，生成可执行文件 timer
make run    # 编译并运行演示
make asan   # 用 AddressSanitizer 跑一遍，检查泄漏和 use-after-free
make clean  # 清理
```

演示程序约需 3 秒（中间会 `sleep` 等待定时器到期）。

## 接口

| 方法 | 说明 |
| --- | --- |
| `add_timer(TimerNode*)` | 按 `expire` 有序插入 |
| `del_timer(TimerNode*)` | 从链表摘除并 `delete` 节点 |
| `adjust_timer(TimerNode*)` | 到期时间改变后重排位置 |
| `tick()` | 触发所有已到期节点，回调在**锁外**执行 |

## 所有权约定（重要）

- **`TimerNode` 归 `TimerListMgr` 所有**：`del_timer` / `tick` / 析构函数都会
  `delete` 它。所以节点必须 `new` 出来，且一旦交出去就**不能再持有该指针**。
- **`client_data` 归业务侧所有**：`TimerListMgr` 只会把 `user_data->timer` 置空，
  不会 `delete` 它，需要自己释放。
- 因此删除一个定时器的正确顺序是：

  ```cpp
  client_data *d = node->user_data;  // 先取出业务数据
  mgr.del_timer(node);               // 再删节点（此 node 指针随即失效）
  delete d;                          // 最后释放业务数据
  ```

  反过来先 `del_timer` 再读 `node->user_data` 就是 use-after-free。

- `tick()` 在锁外执行回调，所以回调里再调 `del_timer` 是安全的
  （`std::mutex` 不可重入，持锁回调会自己把自己锁死）。

## 与 reactor_net 里那套的区别

`reactor_net/` 里也有一套定时器（`Timer.h` / `TimerQueue.cpp`），用的是
**最小堆 + `timerfd`**：由内核在到期时刻唤醒 `epoll`，事件驱动、不需要外部轮询。
这里的实现是**有序链表 + `mutex`**，需要外部主动 `tick()`。

两者是同一个问题的两种解法：链表插入 `O(n)`、取最小 `O(1)`；堆插入 `O(log n)`、
取最小 `O(log n)`。定时器数量少时链表更简单，数量大时堆更划算。
