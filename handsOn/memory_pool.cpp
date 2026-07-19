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
            while(!block && 
                !freeBlockList.compare_exchange_weak(block, block->next, 
                    std::memory_order_acquire, 
                    std::memory_order_acquire))
            {
            }
            freeBlockList.store(block->next, std::memory_order_release);
            return block;
        }

        void free(Block* block) {
            Block* old = freeBlockList.load(std::memory_order_acquire);
            do {
                block->next = old;
            } while (!freeBlockList.compare_exchange_weak(old, block,
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