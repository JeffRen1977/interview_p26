"""LeetCode 146 - LRU Cache. deque + hash map (simpler; remove is O(n))."""

from collections import deque


class LRUCache:
    def __init__(self, capacity: int) -> None:
        self.capacity = capacity
        self.list = deque(maxlen=capacity)
        self.items: dict[int, int] = {}

    def get(self, key: int) -> int:
        if key not in self.items:
            return -1

        self.list.remove(key)  # O(n) worst case
        self.list.append(key)

        return self.items[key]

    def put(self, key: int, value: int) -> None:
        if key in self.items:
            self.list.remove(key)  # O(n) worst case
            self.list.append(key)
            self.items[key] = value
            return

        if len(self.items) == self.capacity:
            del self.items[self.list.popleft()]

        self.list.append(key)
        self.items[key] = value


if __name__ == "__main__":
    cache = LRUCache(2)
    cache.put(1, 1)
    cache.put(2, 2)
    assert cache.get(1) == 1
    cache.put(3, 3)
    assert cache.get(2) == -1
    cache.put(4, 4)
    assert cache.get(1) == -1
    assert cache.get(3) == 3
    assert cache.get(4) == 4
    print("lru_cache: ok")
