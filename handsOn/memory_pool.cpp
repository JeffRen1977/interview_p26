class MemoryPool {
    public:
        MemoryPool(size_t blockSize, size_t blockCount) {
            this->blockSize = blockSize;
            this->blockCount = blockCount;
            // this is the memory locations for the assigned blocks. 
            void* raw_memory = malloc(blockSize * blockCount);
            auto* blocks = static_cast<Block*>(raw_memory);
            for(int i = 0;i+1<blockCount;i++)
            {
                blocks[i].next = &blocks[i + 1];
            }
            blocks[blockCount - 1].next = nullptr;
            freeBlockList.store(blocks[0], std::memory_order_relaxed);
        }

        Block* allocate() {
            Block* block = freeBlockList.load(std::memory_order_acquire);
            /*
            这段代码的本意是从一个无锁单向链表（freeBlockList）的头部弹出一个可用的 Block：读取头节点：拿到当前链表头指针 block。
            CAS 抢占：使用 compare_exchange_weak 原子的比较并交换。如果 freeBlockList == block，说明期间没有其他线程打扰，将其更新为 block->next。
            返回节点：成功拿到节点并返回。*/
            while(block && 
                !freeBlockList.compare_exchange_weak(block, block->next, 
                    std::memory_order_acquire, 
                    std::memory_order_acquire))
            {
            }
            freeBlockList.store(block->next, std::memory_order_release);
            return block;
        }

       void free(Block* block) {
          // 第一步：获取当前链表头指针
         Block* old = freeBlockList.load(std::memory_order_acquire);
    
         // 第二步：循环尝试插入，直到 CAS 成功
        do {
        // 1. 先把待插入节点的 next 指向我们认为的当前头节点 old
        block->next = old;
        
       // 2. CAS 比较并交换：
       //    检查 freeBlockList 是否仍然等于 old：
       //    - 如果等于 old：说明没有其他线程干扰，将 freeBlockList 更新为 block，返回 true，结束循环。
       //    - 如果不等于 old：说明有其他线程抢先插入了新节点，CAS 返回 false。
       //      此时 CAS 内部会自动把 old 更新为最新的 freeBlockList 头指针，进入下一次循环重试。
       } while (!freeBlockList.compare_exchange_weak(
                 old, block,
                 std::memory_order_release,
                 std::memory_order_relaxed));
}

    private:
        size_t blockSize;
        size_t blockCount;
        std::atomic<Block*> freeBlockList;

        struct Block {
            void* ptr;
            size_t size;
            Block* next;
        };
};

int main() {
    MemoryPool pool(1024, 10);
    Block* block = pool.allocate();
    pool.free(block);
    return 0;
}
