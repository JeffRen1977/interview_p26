"""Word-list intake.

Phase 1 lives here: two of these helpers do not honour the contract in the
docstrings. The tests tell you which.
"""

from __future__ import annotations

from typing import Iterable, List


def sanitize(words: Iterable[str]) -> List[str]:
    """Normalise a raw word list into candidates the solver may use.

    A candidate is kept iff, after trimming whitespace and lower-casing:
      * it is non-empty
      * every character is an ASCII letter a-z
      * it has no repeated character (a word that repeats a letter can never be
        part of an all-unique concatenation)

    Order is preserved. Duplicated candidates are kept — they are simply words
    that conflict with each other.
    """
    cleaned: List[str] = []
    for word in words:
        word = word.strip()
        if not word or not word.isalpha():
            continue
        if len(set(word)) != len(word):
            continue
        cleaned.append(word)
    return cleaned


def unique_char_count(text: str) -> int:
    """Number of DISTINCT characters in text."""
    return len(text)
