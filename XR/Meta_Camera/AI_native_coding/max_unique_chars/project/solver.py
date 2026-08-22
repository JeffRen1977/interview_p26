"""Maximise the number of unique characters in a concatenation of chosen words.

Phase 2: max_unique_length — plain backtracking is fine.
Phase 3: max_unique_length_fast — the suite throws two adversarial shapes at it,
         "many short words" and "a few very long words", and the Phase 2 search
         dies on the first one.
"""

from __future__ import annotations

from typing import Iterable, List


def word_mask(word: str) -> int:
    """26-bit set of the letters in `word`. Given — do not reimplement."""
    mask = 0
    for char in word:
        mask |= 1 << (ord(char) - ord("a"))
    return mask


def popcount(mask: int) -> int:
    """Given — number of bits set."""
    return bin(mask).count("1")


# ----------------------------------------------------------------------
# Phase 2
# ----------------------------------------------------------------------
def max_unique_length(words: Iterable[str]) -> int:
    """Longest all-distinct-characters concatenation reachable from `words`.

    The input is raw: sanitize it first. Returns 0 for an empty candidate list.

    TODO(Phase 2): backtracking over "take this word / skip this word".
    """
    raise NotImplementedError("Phase 2: implement max_unique_length")


# ----------------------------------------------------------------------
# Phase 3
# ----------------------------------------------------------------------
def max_unique_length_fast(words: Iterable[str]) -> int:
    """Same answer as max_unique_length, but it must survive the stress sets.

    TODO(Phase 3): work on 26-bit masks and deduplicate reachable states — the
    number of distinct letter sets you can build is far smaller than the number
    of word subsets.
    """
    raise NotImplementedError("Phase 3: implement max_unique_length_fast")
