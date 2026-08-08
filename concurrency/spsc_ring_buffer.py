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
        self.size = capacity + 1
        self.buf: List[Optional[T]] = [None] * self.size
        self.head = 0
        self.tail = 0

    @property
    def capacity(self) -> int:
        return self.size - 1

    def __len__(self) -> int:
        if self.tail >= self.head:
            return self.tail - self.head
        return self.size - self.head + self.tail

    def empty(self) -> bool:
        return self.head == self.tail

    def full(self) -> bool:
        return (self.tail + 1) % self.size == self.head

    def push(self, item: T) -> bool:
        next_tail = (self.tail + 1) % self.size
        if next_tail == self.head:
            return False
        self.buf[self.tail] = item
        self.tail = next_tail
        return True

    def pop(self) -> Optional[T]:
        if self.head == self.tail:
            return None
        item = self.buf[self.head]
        self.buf[self.head] = None
        self.head = (self.head + 1) % self.size
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
