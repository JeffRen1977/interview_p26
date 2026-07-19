class MemoryPool {
    public:
        MemoryPool(size_t blockSize, size_t blockCount) {
            this->blockSize = blockSize;
            this->blockCount = blockCount;
            for(int i = 0;i<blockCount;i++)
            {
                this->freeBlockList->next = new Block();
                this->freeBlockList = this->freeBlockList->next;
            }        
            this->freeBlockList->next = nullptr;
        }

        Block* allocate() {
            Block* block = freeBlockList.load(std::memory_order_acquire);
            if(block == nullptr) {
                return nullptr;
            }
            freeBlockList.store(block->next, std::memory_order_release);
            return block;
        }

        void free(Block* block) {
            block->next = freeBlockList.load(std::memory_order_acquire);
            freeBlockList.store(block, std::memory_order_release);
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