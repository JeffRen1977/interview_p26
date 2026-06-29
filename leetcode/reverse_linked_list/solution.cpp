// LeetCode 206 - Reverse Linked List.

#include <cassert>
#include <iostream>
#include "../common/list_node.hpp"

ListNode* reverseList(ListNode* head) {
    ListNode* prev = nullptr;
    ListNode* cur = head;
    while (cur) {
        ListNode* nxt = cur->next;
        cur->next = prev;
        prev = cur;
        cur = nxt;
    }
    return prev;
}

int main() {
    ListNode* head = buildList({1, 2, 3, 4, 5});
    ListNode* rev = reverseList(head);
    assert(listToVector(rev) == std::vector<int>({5, 4, 3, 2, 1}));
    freeList(rev);
    std::cout << "reverse_linked_list: ok\n";
    return 0;
}
