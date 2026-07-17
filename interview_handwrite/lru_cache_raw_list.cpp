// LRU cache — custom doubly linked list + unordered_map (no std::list).
//
// Contrast with lru_cache_ds.cpp (uses std::list::splice).
// This version is preferred by systems / EC2-style interviews because it shows:
// - Raw pointer ownership and explicit delete (no list RAII hiding leaks)
// - Sentinel head/tail that eliminate empty/single-node edge cases
// - O(1) detach + reattach without moving payload bytes
//
// Whiteboard talking points:
// - map: key → Node* for O(1) lookup.
// - List order: MRU = head->next … LRU = tail->prev.
// - Get / Put on hit: moveToHead (removeNode + addToHead).
// - Put on miss when full: erase map + remove + delete the LRU node.
// - Production follow-up: allocate Nodes from a fixed object pool instead of new.

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

class LRUCache {
 private:
    // Compact node: key must live on the node so eviction can erase from map
    // without an extra reverse lookup.
    struct Node {
        int key;
        int value;
        Node* prev;
        Node* next;

        Node(int k, int v) : key(k), value(v), prev(nullptr), next(nullptr) {}
    };

 public:
    explicit LRUCache(int capacity) : capacity_(static_cast<size_t>(capacity)) {
        if (capacity <= 0) {
            throw std::invalid_argument("capacity must be positive");
        }

        // Sentinel pair: real nodes always sit strictly between head and tail.
        // Empty list is still head <-> tail, so remove/add never need
        // "if first/last" branches.
        head_ = new Node(-1, -1);
        tail_ = new Node(-1, -1);
        head_->next = tail_;
        tail_->prev = head_;
    }

    ~LRUCache() {
        // Walk the whole chain including sentinels and free every Node.
        Node* curr = head_;
        while (curr != nullptr) {
            Node* next = curr->next;
            delete curr;
            curr = next;
        }
    }

    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;

    // LeetCode-style API: miss → -1.
    int get(int key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return -1;
        }

        Node* node = it->second;
        moveToHead(node);  // mark as most recently used
        return node->value;
    }

    void put(int key, int value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            // Update in place; only the list order changes.
            Node* node = it->second;
            node->value = value;
            moveToHead(node);
            return;
        }

        if (map_.size() >= capacity_) {
            // Evict least recently used = real node immediately before tail.
            Node* evict = tail_->prev;
            map_.erase(evict->key);
            removeNode(evict);
            delete evict;  // ownership is ours; list no longer tracks it
        }

        // Hot path in production often uses an object pool instead of new.
        Node* node = new Node(key, value);
        map_[key] = node;
        addToHead(node);
    }

    size_t size() const { return map_.size(); }

    // MRU → LRU key order for assertions / whiteboard demos.
    std::vector<int> keys_mru_to_lru() const {
        std::vector<int> keys;
        for (Node* curr = head_->next; curr != tail_; curr = curr->next) {
            keys.push_back(curr->key);
        }
        return keys;
    }

 private:
    // Unlink node from neighbors. Node itself is left dangling until
    // addToHead / delete; callers must not leave it orphaned in the list.
    void removeNode(Node* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    // Insert immediately after head (= MRU position).
    void addToHead(Node* node) {
        node->next = head_->next;
        node->prev = head_;
        head_->next->prev = node;
        head_->next = node;
    }

    // True O(1) promotion: only four pointer writes, no payload copy.
    void moveToHead(Node* node) {
        removeNode(node);
        addToHead(node);
    }

    size_t capacity_;
    Node* head_ = nullptr;  // sentinel
    Node* tail_ = nullptr;  // sentinel
    std::unordered_map<int, Node*> map_;
};

bool test_lru_cache_raw_list() {
    LRUCache cache(2);

    cache.put(1, 10);
    cache.put(2, 20);
    assert(cache.get(1) == 10);  // 1 becomes MRU

    cache.put(3, 30);            // evicts key 2 (LRU)
    assert(cache.get(2) == -1);
    assert(cache.get(3) == 30);
    assert(cache.size() == 2);

    auto keys = cache.keys_mru_to_lru();
    assert(keys.size() == 2);
    assert(keys[0] == 3);
    assert(keys[1] == 1);

    cache.put(1, 11);  // update existing
    assert(cache.get(1) == 11);
    keys = cache.keys_mru_to_lru();
    assert(keys[0] == 1);
    assert(keys[1] == 3);

    return true;
}

int main() {
    assert(test_lru_cache_raw_list());
    std::cout << "lru_cache_raw_list: ok\n";
    return 0;
}
