"""Reference solution — in-memory KV with TTL and a tag inverted index."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Iterable, List, Optional, Set

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
        self._by_tag: Dict[str, Set[str]] = {}

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
        expires_at = None if ttl is None else self.clock.now() + ttl
        new_tags = frozenset(tags or [])
        old = self._data.get(key)
        if old is not None:
            self._index_remove(key, old.tags)
        self._data[key] = Entry(value, expires_at, new_tags)
        self._index_add(key, new_tags)

    def get(self, key: str) -> Any:
        entry = self._touch(key)
        return None if entry is None else entry.value

    def delete(self, key: str) -> bool:
        entry = self._data.get(key)
        if entry is None:
            return False
        self._purge(key, entry)
        return True

    def exists(self, key: str) -> bool:
        return self._touch(key) is not None

    def size(self) -> int:
        live = 0
        for key in list(self._data):
            if self._touch(key) is not None:
                live += 1
        return live

    def _is_expired(self, entry: Entry) -> bool:
        if entry.expires_at is None:
            return False
        return self.clock.now() >= entry.expires_at

    def _touch(self, key: str) -> Optional[Entry]:
        entry = self._data.get(key)
        if entry is None:
            return None
        if self._is_expired(entry):
            self._purge(key, entry)
            return None
        return entry

    def _purge(self, key: str, entry: Entry) -> None:
        del self._data[key]
        self._index_remove(key, entry.tags)

    def _index_add(self, key: str, tags: frozenset) -> None:
        for tag in tags:
            self._by_tag.setdefault(tag, set()).add(key)

    def _index_remove(self, key: str, tags: frozenset) -> None:
        for tag in tags:
            posting = self._by_tag.get(tag)
            if posting is None:
                continue
            posting.discard(key)
            if not posting:
                del self._by_tag[tag]

    # ------------------------------------------------------------------
    # Phase 2 — scan
    # ------------------------------------------------------------------
    def query(self, tags: Iterable[str], match: str = "all") -> List[str]:
        needed = list(tags)
        found: List[str] = []
        for key in list(self._data):
            entry = self._touch(key)
            if entry is None:
                continue
            if _matches(entry.tags, needed, match):
                found.append(key)
        return sorted(found)

    # ------------------------------------------------------------------
    # Phase 3 — inverted index
    # ------------------------------------------------------------------
    def query_fast(self, tags: Iterable[str], match: str = "all") -> List[str]:
        needed = list(tags)
        if match == "all" and not needed:
            return self.query(needed, match)
        if match == "any" and not needed:
            return []

        if match == "all":
            candidates: Optional[Set[str]] = None
            for tag in needed:
                posting = self._by_tag.get(tag)
                if not posting:
                    return []
                candidates = set(posting) if candidates is None else candidates & posting
                if not candidates:
                    return []
        elif match == "any":
            candidates = set()
            for tag in needed:
                posting = self._by_tag.get(tag)
                if posting:
                    candidates |= posting
        else:
            raise ValueError(f"unknown match mode: {match!r}")

        found: List[str] = []
        for key in list(candidates or ()):
            if self._touch(key) is not None:
                found.append(key)
        return sorted(found)


def _matches(have: frozenset, needed: List[str], match: str) -> bool:
    if match == "all":
        return all(tag in have for tag in needed)
    if match == "any":
        return any(tag in have for tag in needed)
    raise ValueError(f"unknown match mode: {match!r}")
