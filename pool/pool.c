#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h> //ptrdiff_t
struct Node {
    struct Node* next; 
};
typedef struct {
    size_t block_size; //块大小
    size_t total_blocks; //总块数
    size_t used_count; //已用块数
    uint8_t *pool_start; //池起始地址
    uint8_t *next_free; //下一个空闲块地址
    struct Node* free_list_head; //指向第一个可用的空闲块
}MemoryPool;

MemoryPool* pool_init(size_t block_size, size_t num_blocks) {
    //确保 block_size 至少能塞下一个指针
    if (block_size < sizeof(struct Node)) {
        block_size = sizeof(struct Node);
    }
    //malloc(大小) sizeof(MemoryPool)单个结构体多少字节
    MemoryPool* pool = (MemoryPool*)malloc(sizeof(MemoryPool));
    //申请失败
    if (!pool) return NULL; 
    pool->block_size = block_size; 
    pool->total_blocks = num_blocks;
    pool->used_count = 0; 
    //申请一块大连续空间，uint8_t正好一个字节
    //p + 1往后跳一个字节
    //int是4字节，没法逐个字节精细管理内存块
    pool->pool_start = (uint8_t *)malloc(block_size * num_blocks);
    pool->next_free = pool->pool_start;
    pool->free_list_head = NULL; //初始化时没有回收的块
    //申请失败，释放内存
    if (!pool->pool_start) {
        free(pool);
        return NULL;
    }
    return pool;
}
// 功能：从内存池 pool 里申请一块空闲内存
// 返回：分配到的内存地址（失败返回 NULL）
void* pool_alloc(MemoryPool *pool) {
    if (pool->free_list_head != NULL) {
        struct Node *allocated_node = pool->free_list_head;
        pool->free_list_head = allocated_node->next;
        printf("Allocated from Free List\n");
        return (void*)allocated_node;
    }
    size_t linear_used = (pool->next_free - pool->pool_start) / pool->block_size;
    if (linear_used < pool->total_blocks) {
        //记录当前空闲位置作为返回地址
        void *allocated_addr = pool->next_free;
        //将next_free向后移动了一个block_size的偏移量
        //uint8_t* 确保加法是以字节为单位的
        //+ 是为了下次分配
        pool->next_free += pool->block_size;
        pool->used_count++; 
        printf("Allocated new block from Arena\n");
        return allocated_addr;
    }
    printf("Error: Memory pool exhausted!\n");
    return NULL; 
}
//释放内存块
void pool_free(MemoryPool *pool, void *ptr) {
    if (!ptr) return ;
    struct Node* node = (struct Node*)ptr; 

    node->next = pool->free_list_head;
    pool->free_list_head = node;
    pool->used_count--;
    printf("Block freed and added to Free List\n");
}
int main() {
    MemoryPool* my_pool = pool_init(64, 10);
    printf("Initial: Used %zu/%zu\n", my_pool->used_count, my_pool->total_blocks);

    void* ptr1 = pool_alloc(my_pool);
    void* ptr2 = pool_alloc(my_pool);
    printf("After 2 allocs: Used %zu/%zu\n", my_pool->used_count, my_pool->total_blocks);
    printf("Ptr1 address: %p\n", ptr1); 
    //指针相减的结果类型是 ptrdiff_t，用 %td 打印才是可移植的写法。
    //不能写死 %lld 或 %ld：Windows(LLP64) 下 ptrdiff_t 是 long long，Linux/macOS(LP64) 下是 long，
    //写死任意一个都会在另一个平台上产生 -Wformat 警告甚至打印错值。
    printf("Ptr2 address: %p (Diff: %td bytes)\n", ptr2, (uint8_t*)ptr2 - (uint8_t*)ptr1);
    //pool_init里面两次malloc, 只释放my_pool，会内存泄露
    // free(my_pool->pool_start);
    // free(my_pool);



    // 1. 释放 ptr1，它应该被挂到 free_list_head 上
    pool_free(my_pool, ptr1);
    printf("After free ptr1: Used %zu/%zu\n", my_pool->used_count, my_pool->total_blocks);

    // 2. 再次申请，这次应该从 Free List 拿回刚才 ptr1 的地址，而不是新开辟地址
    void* ptr3 = pool_alloc(my_pool);
    printf("Ptr3 address: %p (Should be same as Ptr1)\n", ptr3);

    if (ptr1 == ptr3) {
        printf("SUCCESS: Free List reuse works!\n");
    }
    return 0;

}