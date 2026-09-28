// Word-list intake.
//
// Phase 1 lives in wordlist.cpp: three of these helpers do not honour the
// contract below. The tests tell you which.
#pragma once

#include <string>
#include <vector>

// Normalise a raw word list into candidates the solver may use.
//
// A candidate is kept iff, after trimming whitespace from BOTH ends and
// lower-casing:
//   * it is non-empty
//   * every byte is an ASCII letter a-z
//   * it has no repeated character (a word that repeats a letter can never be
//     part of an all-unique concatenation)
//
// Order is preserved. Duplicated candidates are kept -- they are simply words
// that conflict with each other.
std::vector<std::string> sanitize(const std::vector<std::string>& words);

// Number of DISTINCT characters in `text`.
int unique_char_count(const std::string& text);
