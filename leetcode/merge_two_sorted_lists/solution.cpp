// LeetCode 21 - Merge Two Sorted Lists.

#include <cassert>
#include <iostream>
#include "../common/list_node.hpp"

ListNode* mergeTwoLists(ListNode* l1, ListNode* l2) {
    ListNode dummy;
    ListNode* tail = &dummy;
    while (l1 && l2) {
        if (l1->val <= l2->val) {
            tail->next = l1;
            l1 = l1->next;
        } else {
            tail->next = l2;
            l2 = l2->next;
        }
        tail = tail->next;
    }
    tail->next = l1 ? l1 : l2;
    return dummy.next;
}

int main() {
    ListNode* a = buildList({1, 2, 4});
    ListNode* b = buildList({1, 3, 4});
    ListNode* merged = mergeTwoLists(a, b);
    assert(listToVector(merged) == std::vector<int>({1, 1, 2, 3, 4, 4}));
    freeList(merged);
    std::cout << "merge_two_sorted_lists: ok\n";
    return 0;
}
