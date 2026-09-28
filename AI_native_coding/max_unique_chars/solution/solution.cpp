#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_set>
#include <algorithm>
#include <bit> // C++20 for std::popcount (若是 C++17 可用 __builtin_popcount)

// ----------------------------------------------------------------------
// 基础位运算辅助函数
// ----------------------------------------------------------------------
inline uint32_t word_mask(std::string_view word) noexcept {
    uint32_t mask = 0;
    for (char c : word) {
        mask |= (1u << (c - 'a'));
    }
    return mask;
}

inline int popcount(uint32_t mask) noexcept {
#if __cplusplus >= 202002L
    return std::popcount(mask);
#else
    return __builtin_popcount(mask);
#endif
}

// ----------------------------------------------------------------------
// Phase 2: max_unique_length (标准回溯 / Backtracking)
// ----------------------------------------------------------------------
int max_unique_length(const std::vector<std::string>& words) {
    std::vector<uint32_t> valid_masks;
    valid_masks.reserve(words.size());

    // 1. Sanitize: 过滤自身有重复字符的无效词
    for (const auto& w : words) {
        uint32_t m = word_mask(w);
        if (popcount(m) == static_cast<int>(w.length())) {
            valid_masks.push_back(m);
        }
    }

    if (valid_masks.empty()) {
        return 0;
    }

    int max_len = 0;
    const size_t n = valid_masks.size();

    auto backtrack = [&](auto& self, size_t index, uint32_t current_mask) -> void {
        if (index == n) {
            max_len = std::max(max_len, popcount(current_mask));
            return;
        }

        // Branch 1: Skip 当前词
        self(self, index + 1, current_mask);

        // Branch 2: Take 当前词 (若无重叠字符)
        uint32_t m = valid_masks[index];
        if ((current_mask & m) == 0) {
            self(self, index + 1, current_mask | m);
        }
    };

    backtrack(backtrack, 0, 0);
    return max_len;
}

// ----------------------------------------------------------------------
// Phase 3: max_unique_length_fast (应对对抗形态的 DP/状态集合优化)
// ----------------------------------------------------------------------
int max_unique_length_fast(const std::vector<std::string>& words) {
    // 1. Sanitize + 掩码去重 (直接用 unordered_set 压缩海量重复短词)
    std::unordered_set<uint32_t> unique_masks;
    for (const auto& w : words) {
        uint32_t m = word_mask(w);
        if (popcount(m) == static_cast<int>(w.length())) {
            unique_masks.insert(m);
        }
    }

    if (unique_masks.empty()) {
        return 0;
    }

    // 2. 启发式贪心排序：按 bit 数量从大到小排序，优先逼近理论上限
    std::vector<uint32_t> sorted_masks(unique_masks.begin(), unique_masks.end());
    std::sort(sorted_masks.begin(), sorted_masks.end(), [](uint32_t a, uint32_t b) {
        return popcount(a) > popcount(b);
    });

    // 3. Reachable Mask Set DP: 维护所有可达的互斥掩码组合
    std::vector<uint32_t> reachable_masks;
    reachable_masks.reserve(1024);
    reachable_masks.push_back(0); // 初始包含空集

    int best_len = 0;

    for (uint32_t m : sorted_masks) {
        const size_t curr_size = reachable_masks.size();
        for (size_t i = 0; i < curr_size; ++i) {
            uint32_t curr = reachable_masks[i];
            
            // 无冲突则产生新组合
            if ((curr & m) == 0) {
                uint32_t combined = curr | m;
                reachable_masks.push_back(combined);

                int c_len = popcount(combined);
                if (c_len > best_len) {
                    best_len = c_len;
                    // 26 个字母已占满，直接剪枝退出
                    if (best_len == 26) {
                        return 26;
                    }
                }
            }
        }
    }

    return best_len;
}
