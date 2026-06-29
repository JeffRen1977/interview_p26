// LeetCode 146 - LRU Cache.

#include <cassert>
#include <iostream>
#include <unordered_map>

struct DNode {
    int key = 0;
    int val = 0;
    DNode* prev = nullptr;
    DNode* next = nullptr;
    DNode(int k = 0, int v = 0) : key(k), val(v) {}
};

class LRUCache {
 public:
    explicit LRUCache(int capacity) : capacity_(capacity) {
        head_ = new DNode();
        tail_ = new DNode();
        head_->next = tail_;
        tail_->prev = head_;
    }

    ~LRUCache() {
        DNode* cur = head_;
        while (cur) {
            DNode* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
    }

    int get(int key) {
        auto it = map_.find(key);
        if (it == map_.end()) return -1;
        touch(it->second);
        return it->second->val;
    }

    void put(int key, int value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->val = value;
            touch(it->second);
            return;
        }
        auto* node = new DNode(key, value);
        map_[key] = node;
        insertFront(node);
        if (static_cast<int>(map_.size()) > capacity_) {
            DNode* lru = tail_->prev;
            remove(lru);
            map_.erase(lru->key);
            delete lru;
        }
    }

 private:
    int capacity_;
    DNode *head_, *tail_;
    std::unordered_map<int, DNode*> map_;

    void remove(DNode* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void insertFront(DNode* node) {
        node->next = head_->next;
        node->prev = head_;
        head_->next->prev = node;
        head_->next = node;
    }

    void touch(DNode* node) {
        remove(node);
        insertFront(node);
    }
};

int main() {
    LRUCache cache(2);
    cache.put(1, 1);
    cache.put(2, 2);
    assert(cache.get(1) == 1);
    cache.put(3, 3);
    assert(cache.get(2) == -1);
    cache.put(4, 4);
    assert(cache.get(1) == -1);
    assert(cache.get(3) == 3);
    assert(cache.get(4) == 4);
    std::cout << "lru_cache: ok\n";
    return 0;
}
