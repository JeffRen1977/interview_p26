from typing import Any, Optional
import threading


class StoreError(RuntimeError):
    """Raised when the backing store cannot complete a read or write."""


class MemoryStore:
    """Process-local KV stand-in for Redis / Memcached.

    The limiter must not keep authoritative quota in its own fields.
    Swap this class for a Redis client later; the engine API stays the same.

    get/set/delete are mutex-protected so concurrent tenants can share one store.
    Lists are copied so callers never mutate the dict's stored value.
    """

    def __init__(self) -> None:
        self._kv: dict[str, Any] = {}
        self._lock = threading.Lock()
        self.fail: bool = False

    def get(self, key: str) -> Optional[Any]:
        with self._lock:
            if self.fail:
                raise StoreError("store unavailable")
            value = self._kv.get(key)
            if isinstance(value, list):
                return list(value)
            return value

    def set(self, key: str, value: Any) -> None:
        with self._lock:
            if self.fail:
                raise StoreError("store unavailable")
            if isinstance(value, list):
                value = list(value)
            self._kv[key] = value

    def delete(self, key: str) -> None:
        with self._lock:
            if self.fail:
                raise StoreError("store unavailable")
            self._kv.pop(key, None)
