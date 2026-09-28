"""People-You-May-Know: validation, ranking, evaluation. Reference implementation."""

from __future__ import annotations

from collections import Counter
from typing import Callable, Dict, Iterable, List, Mapping, Sequence, Set

from social_graph import SocialGraph

Recommender = Callable[[SocialGraph, str, int], List[str]]


# ----------------------------------------------------------------------
# Phase 1 — validation
# ----------------------------------------------------------------------
def is_valid_recommendations(
    graph: SocialGraph,
    user: str,
    candidates: Sequence[str],
) -> bool:
    """True iff `candidates` is a legal recommendation list for `user`.

    All five rules must hold:
      1. `user` must exist in the graph.
      2. `user` must not be recommended to themselves.
      3. Nobody who is already a friend of `user` may be recommended.
      4. No candidate may appear twice.
      5. Every candidate must exist in the graph.

    An empty candidate list is always valid (for a user who exists).
    """
    if not graph.has_user(user):
        return False
    own = graph.friends(user)
    seen: Set[str] = set()
    for candidate in candidates:
        if candidate == user:
            return False
        if candidate in own:              # fix: the *user's* friends, not the candidate's
            return False
        if candidate in seen:
            return False
        if not graph.has_user(candidate):  # fix: rule 5 was never implemented
            return False
        seen.add(candidate)
    return True


# ----------------------------------------------------------------------
# Phase 2 — baseline ranking
# ----------------------------------------------------------------------
def recommend_baseline(graph: SocialGraph, user: str, k: int = 5) -> List[str]:
    """Top-`k` people-you-may-know for `user`, ranked by mutual friend count.

    Score of a candidate `c` is |friends(user) & friends(c)|.
    Order: score descending, then user id ascending. O(N * deg) per call.
    """
    if k <= 0 or not graph.has_user(user):
        return []
    own = graph.friends(user)
    scored = []
    for other in graph.users():
        if other == user or other in own:
            continue
        mutual = len(own & graph.friends(other))
        if mutual:
            scored.append((-mutual, other))
    scored.sort()
    return [uid for _, uid in scored[:k]]


# ----------------------------------------------------------------------
# Phase 3 — offline evaluation
# ----------------------------------------------------------------------
def evaluate(
    graph: SocialGraph,
    recommend_fn: Recommender,
    holdout: Mapping[str, Iterable[str]],
    k: int = 3,
) -> Dict[str, float]:
    """Mean precision@k / recall@k / coverage of `recommend_fn` over `holdout`."""
    if k <= 0:
        raise ValueError("k must be >= 1")
    users = sorted(holdout)
    if not users:
        return {"precision": 0.0, "recall": 0.0, "coverage": 0.0}

    precision = recall = coverage = 0.0
    for user in users:
        actual = set(holdout[user])
        recs = recommend_fn(graph, user, k)
        hits = len(set(recs) & actual)
        precision += hits / k
        recall += hits / len(actual) if actual else 0.0
        coverage += 1.0 if recs else 0.0

    n = float(len(users))
    return {
        "precision": precision / n,
        "recall": recall / n,
        "coverage": coverage / n,
    }


# ----------------------------------------------------------------------
# Phase 4 — scale
# ----------------------------------------------------------------------
def recommend_fast(graph: SocialGraph, user: str, k: int = 5) -> List[str]:
    """Same answers as recommend_baseline, computed from the 2-hop neighbourhood.

    A candidate can only score if it shares a friend with `user`, so the only
    nodes worth visiting are friends-of-friends. Counting how many friends of
    `user` point at each friend-of-friend *is* the mutual-friend count:
    f in friends(user) and c in friends(f)  <=>  f in friends(user) & friends(c).

    Cost is O(sum of deg(f) for f in friends(user)) instead of O(N * deg).
    """
    if k <= 0 or not graph.has_user(user):
        return []
    own = graph.friends(user)
    counts: Counter = Counter()
    for friend in own:
        for candidate in graph.friends(friend):
            if candidate == user or candidate in own:
                continue
            counts[candidate] += 1
    ranked = sorted(counts.items(), key=lambda item: (-item[1], item[0]))
    return [uid for uid, _ in ranked[:k]]
