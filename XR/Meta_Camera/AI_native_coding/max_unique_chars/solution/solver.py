"""Reference solution — maximise unique characters in a concatenation."""

from __future__ import annotations

from typing import Dict, Iterable, List

from wordlist import sanitize


def word_mask(word: str) -> int:
    """26-bit set of the letters in `word`."""
    mask = 0
    for char in word:
        mask |= 1 << (ord(char) - ord("a"))
    return mask


def popcount(mask: int) -> int:
    return bin(mask).count("1")


# ----------------------------------------------------------------------
# Phase 2
# ----------------------------------------------------------------------
def max_unique_length(words: Iterable[str]) -> int:
    """Plain take/skip backtracking over sets.

    O(2^n) with a set union per node. Correct, readable, and dead on arrival
    once the candidate list grows past ~20 words — which is exactly what the
    Phase 3 stress set does.
    """
    candidates = sanitize(words)

    def search(index: int, used: set) -> int:
        if index == len(candidates):
            return len(used)
        best = search(index + 1, used)
        letters = set(candidates[index])
        if not (letters & used):
            best = max(best, search(index + 1, used | letters))
        return best

    return search(0, set())


# ----------------------------------------------------------------------
# Phase 3
# ----------------------------------------------------------------------
def max_unique_length_fast(words: Iterable[str]) -> int:
    """Bitmask DP over *reachable letter sets*, not over word subsets.

    The insight the stress set is testing: two different subsets that cover the
    same letters are the same state. There are at most 2^26 letter sets and in
    practice far fewer, whereas there are 2^n subsets. Deduplicating on the mask
    turns "many short words" from 2^66 into a few thousand states.

    Cost: O(#words * #distinct reachable masks), with O(1) compatibility tests
    (`mask & other`) instead of set unions.
    """
    candidates = sanitize(words)
    if not candidates:
        return 0

    # Collapse duplicate words and words that are subsets of nothing new.
    masks: Dict[int, int] = {}
    for word in candidates:
        mask = word_mask(word)
        masks[mask] = popcount(mask)

    reachable = {0}
    best = 0
    for mask in masks:
        grown: List[int] = []
        for state in reachable:
            if state & mask:
                continue
            combined = state | mask
            if combined not in reachable:
                grown.append(combined)
                if popcount(combined) > best:
                    best = popcount(combined)
        reachable.update(grown)
    return best
