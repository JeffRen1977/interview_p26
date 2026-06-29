"""LRU cache — doubly linked list + hash map."""

from __future__ import annotations

import threading
from typing import Dict, Generic, List, Optional, TypeVar

K = TypeVar("K")
V = TypeVar("V")


class _Node(Generic[K, V]):
    def __init__(self, key: K, value: V):
        self.key = key
        self.value = value
        self.prev: Optional["_Node[K, V]"] = None
        self.next: Optional["_Node[K, V]"] = None


class LRUCache(Generic[K, V]):
    """get/put are O(1)."""

    def __init__(self, capacity: int):
        if capacity <= 0:
            raise ValueError("capacity must be positive")
        self.capacity = capacity
        self._map: Dict[K, _Node[K, V]] = {}

        self._head = _Node(None, None)  # type: ignore[arg-type]
        self._tail = _Node(None, None)  # type: ignore[arg-type]
        self._head.next = self._tail
        self._tail.prev = self._head

    def _remove(self, node: _Node[K, V]) -> None:
        assert node.prev and node.next
        node.prev.next = node.next
        node.next.prev = node.prev

    def _insert_front(self, node: _Node[K, V]) -> None:
        node.next = self._head.next
        node.prev = self._head
        assert self._head.next
        self._head.next.prev = node
        self._head.next = node

    def _touch(self, node: _Node[K, V]) -> None:
        self._remove(node)
        self._insert_front(node)

    def get(self, key: K, default: Optional[V] = None) -> Optional[V]:
        if key not in self._map:
            return default
        node = self._map[key]
        self._touch(node)
        return node.value

    def put(self, key: K, value: V) -> None:
        if key in self._map:
            node = self._map[key]
            node.value = value
            self._touch(node)
            return

        node = _Node(key, value)
        self._map[key] = node
        self._insert_front(node)

        if len(self._map) > self.capacity:
            lru = self._tail.prev
            assert lru is not None and lru is not self._head
            self._remove(lru)
            del self._map[lru.key]

    def __len__(self) -> int:
        return len(self._map)

    def keys_from_mru_to_lru(self) -> List[K]:
        keys: List[K] = []
        cur = self._head.next
        while cur and cur is not self._tail:
            keys.append(cur.key)
            cur = cur.next
        return keys


class ThreadSafeLRUCache(Generic[K, V]):
    def __init__(self, capacity: int):
        self._cache = LRUCache[K, V](capacity)
        self._lock = threading.Lock()

    def get(self, key: K, default: Optional[V] = None) -> Optional[V]:
        with self._lock:
            return self._cache.get(key, default)

    def put(self, key: K, value: V) -> None:
        with self._lock:
            self._cache.put(key, value)

    def __len__(self) -> int:
        with self._lock:
            return len(self._cache)


def _test() -> None:
    cache = LRUCache[int, str](2)
    cache.put(1, "a")
    cache.put(2, "b")
    assert cache.get(1) == "a"
    cache.put(3, "c")
    assert cache.get(2) is None
    assert cache.get(3) == "c"
    assert cache.keys_from_mru_to_lru() == [3, 1]

    safe = ThreadSafeLRUCache[int, int](2)
    safe.put(1, 10)
    assert safe.get(1) == 10
    print("lru_cache_ds: ok")


if __name__ == "__main__":
    _test()
