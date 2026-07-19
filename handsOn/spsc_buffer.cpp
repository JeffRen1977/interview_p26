class SPSCBuffer {
    private:
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
    std::vector<int> buffer;

    public:
    SPSCBuffer(size_t size) : buffer(size) {
        head.store(0);
        tail.store(0);
    }

    void push(int value) {
        size_t current_tail = tail.load(std::memory_order_acquire);
        if((current_tail+1)%buffer.size()== head.load(std::memory_order_acquire)) {
            return;// full.
        buffer[(current_tail)%buffer.size()] = value;
        tail.store(current_tail + 1,std::memory_order_release);
    }

    int pop() {
        size_t current_head = head.load(std::memory_order_acquire);
        if(current_head == tail.load(std::memory_order_acquire)) {
            return -1;// empty.
        }
        int value = buffer[current_head%buffer.size()];
        head.store(current_head + 1,std::memory_order_release);
        return value;
    }

    bool is_empty() {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }

    bool is_full() {
        return (tail.load(std::memory_order_acquire) - head.load(std::memory_order_acquire)) == buffer.size();
    }
};