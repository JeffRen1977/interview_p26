"""Object pool — pre-allocated buffer reuse."""

from __future__ import annotations

import time
from dataclasses import dataclass
from typing import Generic, List, Optional, TypeVar

import threading

T = TypeVar("T")


@dataclass
class PooledObject(Generic[T]):
    value: T
    in_use: bool = False


class ObjectPool(Generic[T]):
    def __init__(self, factory, size: int):
        self._items: List[PooledObject[T]] = [PooledObject(factory()) for _ in range(size)]
        self._lock = threading.Lock()

    def acquire(self, block: bool = True, timeout: Optional[float] = None) -> Optional[T]:
        deadline = None if timeout is None else time.monotonic() + timeout
        while True:
            with self._lock:
                for item in self._items:
                    if not item.in_use:
                        item.in_use = True
                        return item.value
            if not block:
                return None
            if timeout is not None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                time.sleep(min(0.001, remaining))
            else:
                time.sleep(0.001)

    def release(self, obj: T) -> None:
        with self._lock:
            for item in self._items:
                if item.value is obj:
                    item.in_use = False
                    return
            raise ValueError("object does not belong to this pool")


def _test() -> None:
    pool = ObjectPool(lambda: bytearray(1024), size=2)
    b1 = pool.acquire()
    b2 = pool.acquire()
    assert pool.acquire(block=False) is None
    pool.release(b1)
    b3 = pool.acquire()
    assert b3 is b1
    pool.release(b2)
    pool.release(b3)
    print("object_pool: ok")


if __name__ == "__main__":
    _test()
