"""SPSC ring buffer — single producer, single consumer."""

from __future__ import annotations

import threading
import time
from typing import Generic, List, Optional, TypeVar

T = TypeVar("T")


class SPSCRingBuffer(Generic[T]):
    """
    Lock-free ring buffer for ONE producer and ONE consumer.

    Producer only touches tail; consumer only touches head.
    push/pop are O(1).
    """

    def __init__(self, capacity: int):
        if capacity <= 0:
            raise ValueError("capacity must be positive")
        self._size = capacity + 1
        self._buf: List[Optional[T]] = [None] * self._size
        self._head = 0
        self._tail = 0

    @property
    def capacity(self) -> int:
        return self._size - 1

    def __len__(self) -> int:
        if self._tail >= self._head:
            return self._tail - self._head
        return self._size - self._head + self._tail

    def empty(self) -> bool:
        return self._head == self._tail

    def full(self) -> bool:
        return (self._tail + 1) % self._size == self._head

    def push(self, item: T) -> bool:
        next_tail = (self._tail + 1) % self._size
        if next_tail == self._head:
            return False
        self._buf[self._tail] = item
        self._tail = next_tail
        return True

    def pop(self) -> Optional[T]:
        if self._head == self._tail:
            return None
        item = self._buf[self._head]
        self._buf[self._head] = None
        self._head = (self._head + 1) % self._size
        return item


def _test() -> None:
    ring: SPSCRingBuffer[int] = SPSCRingBuffer(4)
    consumed: List[int] = []

    def producer() -> None:
        i = 0
        while i < 20:
            if ring.push(i):
                i += 1
            else:
                time.sleep(0.001)

    def consumer() -> None:
        while len(consumed) < 20:
            item = ring.pop()
            if item is not None:
                consumed.append(item)
            else:
                time.sleep(0.001)

    t1 = threading.Thread(target=producer)
    t2 = threading.Thread(target=consumer)
    t1.start()
    t2.start()
    t1.join()
    t2.join()
    assert consumed == list(range(20))
    print("spsc_ring_buffer: ok")


if __name__ == "__main__":
    _test()
