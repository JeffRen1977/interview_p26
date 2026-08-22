"""Reference solution — word-list intake."""

from __future__ import annotations

from typing import Iterable, List


def sanitize(words: Iterable[str]) -> List[str]:
    """Phase 1 fix — two defects in the original:

    1. It never lower-cased, so `"aA"` looked like two distinct characters and
       survived the duplicate check, and `"Ab"` came out with its capital
       intact. Every downstream mask uses `ord(c) - ord('a')`, so an uppercase
       letter would index a negative bit.
    2. `str.isalpha()` is True for `"café"` and `"日本"`. The contract says ASCII
       a-z, so the check needs `isascii()` as well.
    """
    cleaned: List[str] = []
    for word in words:
        word = word.strip().lower()
        if not word or not word.isascii() or not word.isalpha():
            continue
        if len(set(word)) != len(word):
            continue
        cleaned.append(word)
    return cleaned


def unique_char_count(text: str) -> int:
    """Phase 1 fix: distinct characters, not length."""
    return len(set(text))
