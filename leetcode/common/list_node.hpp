#pragma once

#include <vector>

struct ListNode {
    int val;
    ListNode* next;
    ListNode(int x = 0, ListNode* n = nullptr) : val(x), next(n) {}
};

inline ListNode* buildList(const std::vector<int>& values) {
    if (values.empty()) return nullptr;
    ListNode* head = new ListNode(values[0]);
    ListNode* cur = head;
    for (size_t i = 1; i < values.size(); ++i) {
        cur->next = new ListNode(values[i]);
        cur = cur->next;
    }
    return head;
}

inline std::vector<int> listToVector(ListNode* head) {
    std::vector<int> out;
    while (head) {
        out.push_back(head->val);
        head = head->next;
    }
    return out;
}

inline void freeList(ListNode* head) {
    while (head) {
        ListNode* nxt = head->next;
        delete head;
        head = nxt;
    }
}
