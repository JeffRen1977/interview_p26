"""Run all engineering data structure tests."""

from bounded_blocking_queue import _test as test_queue
from lru_cache_ds import _test as test_lru
from object_pool import _test as test_pool
from spsc_ring_buffer import _test as test_spsc
from thread_safe_ring_buffer import _test as test_thread_ring


def main() -> None:
    test_queue()
    test_spsc()
    test_thread_ring()
    test_lru()
    test_pool()
    print("all engineering_ds tests passed")


if __name__ == "__main__":
    main()
