"""In-memory KV store with TTL and tag queries.

Phase 1 lives in get / set / exists: two of these helpers do not honour the
contract in the docstrings. The tests tell you which.

Phase 2: query — scan is fine.
Phase 3: query_fast — the suite hammers a rare tag in a tight loop; a scan dies.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Iterable, List, Optional

from clock import WallClock


@dataclass
class Entry:
    value: Any
    expires_at: Optional[float]
    tags: frozenset


class Store:
    def __init__(self, clock: Optional[WallClock] = None) -> None:
        self.clock = clock if clock is not None else WallClock()
        self._data: Dict[str, Entry] = {}

    # ------------------------------------------------------------------
    # Phase 1 — KV + TTL
    # ------------------------------------------------------------------
    def set(
        self,
        key: str,
        value: Any,
        ttl: Optional[float] = None,
        tags: Optional[Iterable[str]] = None,
    ) -> None:
        """Insert or replace `key`.

        `ttl` is seconds from *now* (this clock). None means never expire.
        `tags` is a set of strings used by query(); missing tags means no tags.
        Overwriting a key replaces value, ttl, and tags.
        """
        # BUG: stores the duration as if it were an absolute timestamp.
        # A 10-second TTL at t=1000 becomes expires_at=10, which is already
        # in the past relative to the clock.
        self._data[key] = Entry(value, ttl, frozenset(tags or []))

    def get(self, key: str) -> Any:
        """Return the value, or None if missing or expired.

        Expired keys must be treated as a miss (lazy eviction is allowed).
        A key expires at the instant `clock.now() >= expires_at`.
        """
        entry = self._data.get(key)
        if entry is None:
            return None
        # BUG: never consults TTL, so expired keys still return a value.
        return entry.value

    def delete(self, key: str) -> bool:
        """Remove `key`. True iff it was present (even if already expired)."""
        if key not in self._data:
            return False
        del self._data[key]
        return True

    def exists(self, key: str) -> bool:
        """True iff the key is present and not expired."""
        entry = self._data.get(key)
        if entry is None:
            return False
        return not self._is_expired(entry)

    def size(self) -> int:
        """Number of keys that are present and not expired."""
        return sum(1 for entry in self._data.values() if not self._is_expired(entry))

    def _is_expired(self, entry: Entry) -> bool:
        if entry.expires_at is None:
            return False
        return self.clock.now() >= entry.expires_at

    # ------------------------------------------------------------------
    # Phase 2
    # ------------------------------------------------------------------
    def query(self, tags: Iterable[str], match: str = "all") -> List[str]:
        """Return live keys whose tags match, sorted.

        match='all': key must contain every requested tag (empty tags → every live key).
        match='any': key must contain at least one requested tag (empty tags → []).
        Skip expired keys. Scanning is fine.
        """
        raise NotImplementedError("Phase 2: implement query")

    # ------------------------------------------------------------------
    # Phase 3
    # ------------------------------------------------------------------
    def query_fast(self, tags: Iterable[str], match: str = "all") -> List[str]:
        """Same answers as query, but it must survive the tight-loop stress set.

        TODO(Phase 3): maintain an inverted index tag -> keys on set/delete/expire
        and answer queries from posting-list intersection / union. Do not scan
        every key.
        """
        raise NotImplementedError("Phase 3: implement query_fast")
