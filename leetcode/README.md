# LeetCode Classic — Python & C++

每题一个目录，含 `solution.py` 与 `solution.cpp` 两种实现。

## 题目索引

| # | 题目 | 目录 |
|---|------|------|
| 1 | Two Sum | [two_sum](./two_sum/) |
| 2 | Three Sum | [three_sum](./three_sum/) |
| 3 | Merge Intervals | [merge_intervals](./merge_intervals/) |
| 4 | Binary Search | [binary_search](./binary_search/) |
| 5 | Search Rotated Array | [search_rotated_array](./search_rotated_array/) |
| 6 | Reverse Linked List | [reverse_linked_list](./reverse_linked_list/) |
| 7 | Merge Two Sorted Lists | [merge_two_sorted_lists](./merge_two_sorted_lists/) |
| 8 | LRU Cache | [lru_cache](./lru_cache/) |
| 9 | Number of Islands | [number_of_islands](./number_of_islands/) |
| 10 | Lowest Common Ancestor | [lowest_common_ancestor](./lowest_common_ancestor/) |
| 11 | Top K Frequent | [top_k_frequent](./top_k_frequent/) |
| 12 | K Closest Points | [k_closest_points](./k_closest_points/) |
| 13 | Longest Substring Without Repeating | [longest_substring_without_repeating](./longest_substring_without_repeating/) |
| 14 | Permutations | [permutations](./permutations/) |
| 15 | Binary Tree Level Order | [binary_tree_level_order](./binary_tree_level_order/) |

## 运行 Python

```bash
cd leetcode/two_sum && python3 solution.py
# 或批量：
for d in leetcode/*/; do python3 "$d/solution.py" || break; done
```

## 编译 C++

单题编译（C++17）：

```bash
cd leetcode/two_sum
g++ -std=c++17 -O2 -Wall -o solution solution.cpp && ./solution
```

使用 CMake 编译全部：

```bash
cd leetcode
cmake -B build && cmake --build build
ctest --test-dir build
```

## 公共头文件

- [`common/list_node.hpp`](./common/list_node.hpp)
- [`common/tree_node.hpp`](./common/tree_node.hpp)
