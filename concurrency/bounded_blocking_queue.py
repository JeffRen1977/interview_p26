"""Thread-safe bounded blocking queue (producer-consumer)."""

from __future__ import annotations

import threading
import time
from collections import deque
from typing import Deque, Generic, List, Optional, TypeVar

T = TypeVar("T")


class BoundedBlockingQueue(Generic[T]):
    """
    Classic producer-consumer queue.

    - One mutex protects the deque.
    - Two condition variables: not_empty / not_full.
    - put() blocks when full; get() blocks when empty.

    get/put amortized O(1).
    """

    def __init__(self, capacity: int):
        if capacity <= 0:
            raise ValueError("capacity must be positive")
        self._capacity = capacity
        self._queue: Deque[T] = deque()
        self._lock = threading.Lock()
        self._not_empty = threading.Condition(self._lock)
        self._not_full = threading.Condition(self._lock)

    def __len__(self) -> int:
        with self._lock:
            return len(self._queue)

    def put(self, item: T, timeout: Optional[float] = None) -> bool:
        with self._not_full:
            deadline = None if timeout is None else time.monotonic() + timeout
            while len(self._queue) >= self._capacity:
                if timeout is None:
                    self._not_full.wait()
                else:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        return False
                    self._not_full.wait(remaining)
            self._queue.append(item)
            self._not_empty.notify()
            return True

    def get(self, timeout: Optional[float] = None) -> Optional[T]:
        with self._not_empty:
            deadline = None if timeout is None else time.monotonic() + timeout
            while not self._queue:
                if timeout is None:
                    self._not_empty.wait()
                else:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        return None
                    self._not_empty.wait(remaining)
            item = self._queue.popleft()
            self._not_full.notify()
            return item

    def try_put(self, item: T) -> bool:
        with self._not_full:
            if len(self._queue) >= self._capacity:
                return False
            self._queue.append(item)
            self._not_empty.notify()
            return True

    def try_get(self) -> Optional[T]:
        with self._not_empty:
            if not self._queue:
                return None
            item = self._queue.popleft()
            self._not_full.notify()
            return item


def _test() -> None:
    q: BoundedBlockingQueue[int] = BoundedBlockingQueue(2)
    produced: List[int] = []
    consumed: List[int] = []

    def producer() -> None:
        for i in range(5):
            assert q.put(i)
            produced.append(i)

    def consumer() -> None:
        for _ in range(5):
            item = q.get()
            assert item is not None
            consumed.append(item)

    t1 = threading.Thread(target=producer)
    t2 = threading.Thread(target=consumer)
    t1.start()
    t2.start()
    t1.join()
    t2.join()
    assert produced == list(range(5))
    assert consumed == list(range(5))
    print("bounded_blocking_queue: ok")


if __name__ == "__main__":
    _test()
