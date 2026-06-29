cmake_minimum_required(VERSION 3.14)
project(leetcode_classic CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROBLEMS
    two_sum
    three_sum
    merge_intervals
    binary_search
    search_rotated_array
    reverse_linked_list
    merge_two_sorted_lists
    lru_cache
    number_of_islands
    lowest_common_ancestor
    top_k_frequent
    k_closest_points
    longest_substring_without_repeating
    permutations
    binary_tree_level_order
)

foreach(name IN LISTS PROBLEMS)
    add_executable(${name} ${name}/solution.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
    add_test(NAME ${name} COMMAND ${name})
endforeach()

enable_testing()
