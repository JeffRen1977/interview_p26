"""People-You-May-Know: validation, ranking, evaluation.

Phase 1 lives in `is_valid_recommendations` — it lets through candidate lists
the docstring says are illegal. Find it from the failing assertions.

Phase 2: recommend_baseline — scanning every user is fine.
Phase 3: evaluate — offline quality metrics for a recommender.
Phase 4: recommend_fast — same answers, but the stress set has 40k users.
"""

from __future__ import annotations

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
    seen: Set[str] = set()
    for candidate in candidates:
        if candidate == user:
            return False
        if candidate in graph.friends(candidate):
            return False
        if candidate in seen:
            return False
        seen.add(candidate)
    return True


# ----------------------------------------------------------------------
# Phase 2 — baseline ranking
# ----------------------------------------------------------------------
def recommend_baseline(graph: SocialGraph, user: str, k: int = 5) -> List[str]:
    """Top-`k` people-you-may-know for `user`, ranked by mutual friend count.

    Score of a candidate `c` is |friends(user) & friends(c)|.
    Only candidates with a score >= 1 are eligible, and the result must always
    satisfy is_valid_recommendations (no self, no existing friends, no dupes).

    Order: score descending, then user id ascending as the tie-break.
    Unknown `user` or k <= 0 returns [].

    TODO(Phase 2): scanning graph.users() is the expected answer here.
    """
    raise NotImplementedError("Phase 2: implement recommend_baseline")


# ----------------------------------------------------------------------
# Phase 3 — offline evaluation
# ----------------------------------------------------------------------
def evaluate(
    graph: SocialGraph,
    recommend_fn: Recommender,
    holdout: Mapping[str, Iterable[str]],
    k: int = 3,
) -> Dict[str, float]:
    """Score a recommender against held-out edges.

    `holdout` maps a user to the friendships that were hidden from `graph`.
    For every user in `holdout`, call `recommend_fn(graph, user, k)` and let
    `hits` be how many of the returned ids are in that user's holdout set.

    Returns the mean over holdout users of:
      precision — hits / k          (always divide by k, not by len(recs))
      recall    — hits / len(holdout[user])   (0.0 when the holdout set is empty)
      coverage  — 1.0 if the recommender returned anything at all, else 0.0

    An empty `holdout` scores 0.0 on all three. k <= 0 raises ValueError.

    TODO(Phase 3): implement the three metrics.
    """
    raise NotImplementedError("Phase 3: implement evaluate")


# ----------------------------------------------------------------------
# Phase 4 — scale
# ----------------------------------------------------------------------
def recommend_fast(graph: SocialGraph, user: str, k: int = 5) -> List[str]:
    """Identical output to recommend_baseline, but it has to survive the stress set.

    TODO(Phase 4): the stress graph has 40k users and the loop queries it
    thousands of times. Do not touch users who cannot possibly score.
    """
    raise NotImplementedError("Phase 4: implement recommend_fast")
