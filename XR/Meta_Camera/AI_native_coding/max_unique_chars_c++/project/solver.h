// Maximise the number of unique characters in a concatenation of chosen words.
//
// Phase 2: max_unique_length      -- plain backtracking is fine
// Phase 3: max_unique_length_fast -- the suite throws two adversarial shapes at
//          it, "many short words" and "a few very long words", and the Phase 2
//          search dies on the first one.
#pragma once

#include <string>
#include <vector>

// 26-bit set of the letters in `word`. Given -- do not reimplement.
//
// Bytes outside 'a'..'z' are ignored on purpose. In Python the equivalent
// `1 << (ord(c) - ord('a'))` raises ValueError on an uppercase letter and the
// bug surfaces immediately; in C++ a negative shift is undefined behaviour and
// would quietly hand you a garbage mask. So this helper is defensive, and the
// job of rejecting non-letters belongs to sanitize().
int word_mask(const std::string& word);

// Given -- number of bits set.
int popcount(int mask);

// Longest all-distinct-characters concatenation reachable from `words`.
// The input is RAW: sanitize it first. Returns 0 for an empty candidate list.
int max_unique_length(const std::vector<std::string>& words);

// Same answer as max_unique_length, but it must survive the stress sets.
int max_unique_length_fast(const std::vector<std::string>& words);
