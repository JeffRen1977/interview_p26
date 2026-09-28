from __future__ import annotations

from typing import Iterable, List

from wordlist import sanitize


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
# Phase 2: Backtracking
# ----------------------------------------------------------------------
def max_unique_length(words: Iterable[str]) -> int:
    """Longest all-distinct-characters concatenation reachable from `words`.

    The input is raw: sanitize it first. Returns 0 for an empty candidate list.
    """
    valid_masks: List[int] = []
    for w in sanitize(words):
        # sanitize 已经丢掉了自身含重复字符的词，这里的 popcount 只是复核
        m = word_mask(w)
        if popcount(m) == len(w):
            valid_masks.append(m)

    if not valid_masks:
        return 0

    max_len = 0
    n = len(valid_masks)

    def backtrack(idx: int, current_mask: int, current_len: int) -> None:
        nonlocal max_len
        if current_len > max_len:
            max_len = current_len

        for i in range(idx, n):
            mask = valid_masks[i]
            # 只有没有交集时才可以选取该单词
            if (current_mask & mask) == 0:
                backtrack(i + 1, current_mask | mask, current_len + popcount(mask))

    backtrack(0, 0, 0)
    return max_len


# ----------------------------------------------------------------------
# Phase 3: Fast State Deduplication (DP over reachable masks)
# ----------------------------------------------------------------------
def max_unique_length_fast(words: Iterable[str]) -> int:
    """Same answer as max_unique_length, but it must survive the stress sets.

    Uses bitmask state compression and deduplication over reachable states.
    """
    # 1. 过滤并去重相同的 mask
    unique_masks = set()
    for w in sanitize(words):
        m = word_mask(w)
        # 单词自身无重复字符
        if popcount(m) == len(w):
            unique_masks.add(m)

    if not unique_masks:
        return 0

    # 2. 状态转移：维护所有可达的无冲突掩码集合
    reachable = {0}
    max_len = 0

    for mask in unique_masks:
        new_states = []
        for state in reachable:
            if (state & mask) == 0:
                combined = state | mask
                new_states.append(combined)
                # 实时更新最大长度
                cnt = popcount(combined)
                if cnt > max_len:
                    max_len = cnt
                    # 达到英文字母上限 26，直接提前返回
                    if max_len == 26:
                        return 26

        reachable.update(new_states)

    return max_len
