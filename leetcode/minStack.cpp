#include <stack>
#include <utility>
#include <algorithm>
#include <stdexcept>

class MinStack {
private:
    // 存储 pair<当前元素值, 截至当前位置的最小值>
    std::stack<std::pair<int, int>> stk;

public:
    MinStack() {}

    void push(int x) {
        if (!stk.empty()) {
            stk.push({x, std::min(x, stk.top().second)});
        } else {
            stk.push({x, x});
        }
    }

    void pop() {
        if (stk.empty()) {
            throw std::runtime_error("Stack is empty!");
        }
        stk.pop();
    }

    int top() {
        if (stk.empty()) {
            throw std::runtime_error("Stack is empty!");
        }
        return stk.top().first;
    }

    int getMin() {
        if (stk.empty()) {
            throw std::runtime_error("Stack is empty!");
        }
        return stk.top().second;
    }
};
  
