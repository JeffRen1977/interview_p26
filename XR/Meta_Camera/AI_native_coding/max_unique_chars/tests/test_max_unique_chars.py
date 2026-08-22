"""Staged spec for the Maximise-Unique-Characters task. These tests are the contract.

Run against the interview skeleton (project/, starts red):
    python3 -m unittest discover -s tests -v

Run against the reference implementation (solution/, must be green):
    AINC_IMPL=solution python3 -m unittest discover -s tests -v
"""

from __future__ import annotations

import os
import random
import string
import sys
import time
import unittest
from itertools import combinations
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMPL = ROOT / os.environ.get("AINC_IMPL", "project")
if str(IMPL) not in sys.path:
    sys.path.insert(0, str(IMPL))

from solver import max_unique_length, max_unique_length_fast, popcount, word_mask  # noqa: E402
from wordlist import sanitize, unique_char_count  # noqa: E402


def brute_force(words):
    """Exhaustive reference for small inputs only."""
    candidates = sanitize(words)
    best = 0
    for size in range(len(candidates) + 1):
        for combo in combinations(candidates, size):
            joined = "".join(combo)
            if len(set(joined)) == len(joined):
                best = max(best, len(joined))
    return best


def random_words(rng, count, max_len=4):
    return [
        "".join(rng.choice(string.ascii_lowercase[:8]) for _ in range(rng.randint(1, max_len)))
        for _ in range(count)
    ]


# ----------------------------------------------------------------------
# Phase 1 — intake bugs
# ----------------------------------------------------------------------
class Phase1Sanitize(unittest.TestCase):
    def test_trims_lowercases_and_drops_invalid_words(self) -> None:
        raw = [" Ab ", "aA", "b1b", "cd", "", "Hello", "\tXYZ\n"]
        self.assertEqual(sanitize(raw), ["ab", "cd", "xyz"])

    def test_rejects_letters_outside_ascii_a_to_z(self) -> None:
        """str.isalpha() is True for 'café' — the contract says ASCII a-z."""
        self.assertEqual(sanitize(["café", "日本", "naive", "ok"]), ["naive", "ok"])

    def test_keeps_order_and_keeps_duplicate_candidates(self) -> None:
        self.assertEqual(sanitize(["dog", "cat", "dog"]), ["dog", "cat", "dog"])

    def test_empty_input(self) -> None:
        self.assertEqual(sanitize([]), [])
        self.assertEqual(sanitize(["", "  ", "123"]), [])

    def test_unique_char_count_counts_distinct_characters(self) -> None:
        self.assertEqual(unique_char_count(""), 0)
        self.assertEqual(unique_char_count("abc"), 3)
        self.assertEqual(unique_char_count("aab"), 2)
        self.assertEqual(unique_char_count("aaaa"), 1)


# ----------------------------------------------------------------------
# Phase 2 — baseline backtracking
# ----------------------------------------------------------------------
class Phase2Backtracking(unittest.TestCase):
    def test_classic_examples(self) -> None:
        self.assertEqual(max_unique_length(["un", "iq", "ue"]), 4)
        self.assertEqual(max_unique_length(["cha", "r", "act", "ers"]), 6)
        self.assertEqual(max_unique_length(["abcdefghijklmnopqrstuvwxyz"]), 26)

    def test_it_sanitizes_its_own_input(self) -> None:
        self.assertEqual(max_unique_length([" UN ", "iq", "ue", "aa", "x1"]), 4)

    def test_no_usable_word(self) -> None:
        self.assertEqual(max_unique_length([]), 0)
        self.assertEqual(max_unique_length(["aa", "bb", "123"]), 0)

    def test_taking_the_longest_word_first_is_not_optimal(self) -> None:
        # greedy takes "abcd" (4); the optimum skips it for "ab" + "cef" (5)
        self.assertEqual(max_unique_length(["abcd", "ab", "cef"]), 5)
        self.assertEqual(max_unique_length(["abcd", "efg"]), 7)

    def test_matches_brute_force_on_random_small_inputs(self) -> None:
        rng = random.Random(17)
        for _ in range(30):
            words = random_words(rng, rng.randint(0, 8))
            self.assertEqual(max_unique_length(words), brute_force(words))


# ----------------------------------------------------------------------
# Phase 3 — adversarial shapes
# ----------------------------------------------------------------------
class Phase3Scale(unittest.TestCase):
    def test_same_answers_as_the_baseline(self) -> None:
        self.assertEqual(max_unique_length_fast(["un", "iq", "ue"]), 4)
        self.assertEqual(max_unique_length_fast(["cha", "r", "act", "ers"]), 6)
        self.assertEqual(max_unique_length_fast([]), 0)
        self.assertEqual(max_unique_length_fast(["aa", "bb"]), 0)

    def test_differential_against_the_baseline(self) -> None:
        rng = random.Random(23)
        for _ in range(40):
            words = random_words(rng, rng.randint(0, 10))
            self.assertEqual(max_unique_length_fast(words), max_unique_length(words))

    def test_helpers(self) -> None:
        self.assertEqual(word_mask(""), 0)
        self.assertEqual(word_mask("a"), 1)
        self.assertEqual(word_mask("ab"), 0b11)
        self.assertEqual(popcount(word_mask("abcdef")), 6)

    def test_many_short_words(self) -> None:
        """Every 2-letter word over a 12-letter alphabet: 66 candidates.

        2^66 subsets, but only 2^12 reachable letter sets. Anything that
        enumerates subsets never returns.
        """
        alphabet = string.ascii_lowercase[:12]
        words = ["".join(pair) for pair in combinations(alphabet, 2)]
        start = time.perf_counter()
        best = max_unique_length_fast(words)
        elapsed = time.perf_counter() - start
        self.assertEqual(best, 12)
        self.assertLess(elapsed, 3.0, f"many-short-words took {elapsed:.2f}s")

    def test_a_few_very_long_words_buried_in_junk(self) -> None:
        """3,000 long words, almost all rejected because they repeat letters."""
        junk = ["abcdefghij" * 20 for _ in range(3000)]
        words = junk + ["abcdefghijklm", "nopqrstuvwxyz"]
        start = time.perf_counter()
        best = max_unique_length_fast(words)
        elapsed = time.perf_counter() - start
        self.assertEqual(best, 26)
        self.assertLess(elapsed, 3.0, f"long-words set took {elapsed:.2f}s")

    def test_duplicate_words_do_not_multiply_the_work(self) -> None:
        words = ["abc"] * 5000 + ["def"] * 5000
        start = time.perf_counter()
        self.assertEqual(max_unique_length_fast(words), 6)
        self.assertLess(time.perf_counter() - start, 3.0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
