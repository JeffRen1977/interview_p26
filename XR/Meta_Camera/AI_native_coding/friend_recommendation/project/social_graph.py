"""Undirected social graph. Given — do not modify.

Friendship is symmetric and irreflexive. `friends()` hands back the live
adjacency set for speed; treat it as read-only.
"""

from __future__ import annotations

from typing import AbstractSet, Dict, Iterable, List, Set

_EMPTY: AbstractSet[str] = frozenset()


class SocialGraph:
    def __init__(self, users: Iterable[str] = ()) -> None:
        self._adj: Dict[str, Set[str]] = {}
        for user in users:
            self.add_user(user)

    def add_user(self, user_id: str) -> None:
        self._adj.setdefault(user_id, set())

    def add_friendship(self, a: str, b: str) -> None:
        """Symmetric. Auto-creates both users. Self-loops are rejected."""
        if a == b:
            raise ValueError(f"{a!r} cannot befriend itself")
        self.add_user(a)
        self.add_user(b)
        self._adj[a].add(b)
        self._adj[b].add(a)

    def has_user(self, user_id: str) -> bool:
        return user_id in self._adj

    def users(self) -> List[str]:
        """All user ids, sorted."""
        return sorted(self._adj)

    def friends(self, user_id: str) -> AbstractSet[str]:
        """Direct friends of `user_id`; empty set for an unknown user.

        Read-only view of internal state. Do not mutate the returned set.
        """
        return self._adj.get(user_id, _EMPTY)

    def degree(self, user_id: str) -> int:
        return len(self._adj.get(user_id, _EMPTY))

    def __len__(self) -> int:
        return len(self._adj)
