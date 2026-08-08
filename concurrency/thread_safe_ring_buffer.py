"""Thread-safe ring buffer — multiple producers/consumers with mutex."""

from __future__ import annotations

import threading
import time
from typing import Generic, List, Optional, TypeVar

T = TypeVar("T")


class ThreadSafeRingBuffer(Generic[T]):
    """
    Mutex-protected bounded ring buffer.

    Non-blocking push/pop: returns False / None when full / empty.
    """

    def __init__(self, capacity: int):
        if capacity <= 0:
            raise ValueError("capacity must be positive")
        self.capacity = capacity + 1
        self.buffer: List[Optional[T]] = [None] * self.capacity
        self.head = 0
        self.tail = 0
        self.lock = threading.Lock()

    def is_empty(self) -> bool:
        return self.head == self.tail

    def is_full(self) -> bool:
        return (self.tail + 1) % self.capacity == self.head

    def push(self, item: T) -> bool:
        with self.lock:
            if self.is_full():
                return False
            self.buffer[self.tail] = item
            self.tail = (self.tail + 1) % self.capacity
            return True

    def pop(self) -> Optional[T]:
        with self.lock:
            if self.is_empty():
                return None
            item = self.buffer[self.head]
            self.buffer[self.head] = None
            self.head = (self.head + 1) % self.capacity
            return item

    def __len__(self) -> int:
        with self.lock:
            if self.tail >= self.head:
                return self.tail - self.head
            return self.capacity - self.head + self.tail


def _test() -> None:
    ring: ThreadSafeRingBuffer[int] = ThreadSafeRingBuffer(8)
    consumed: List[int] = []
    lock = threading.Lock()

    def producer(start: int) -> None:
        i = start
        while i < start + 10:
            if ring.push(i):
                i += 1
            else:
                time.sleep(0.001)

    def consumer() -> None:
        while len(consumed) < 20:
            item = ring.pop()
            if item is not None:
                with lock:
                    consumed.append(item)
            else:
                time.sleep(0.001)

    threads = [
        threading.Thread(target=producer, args=(0,)),
        threading.Thread(target=producer, args=(10,)),
        threading.Thread(target=consumer),
    ]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert sorted(consumed) == list(range(20))
    print("thread_safe_ring_buffer: ok")


if __name__ == "__main__":
    _test()
