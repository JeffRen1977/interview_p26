// LeetCode 71 - Simplify Path.
// Split on '/', skip "" and ".", pop on "..", join remaining with '/'.

#include <cassert>
#include <iostream>
#include <sstream>
#include <stack>
#include <string>

std::string simplifyPath(const std::string& path) {
    std::istringstream is(path);
    std::string token;
    std::stack<std::string> stk;
    while (std::getline(is, token, '/')) {
        if (token.empty() || token == ".") {
            continue;
        }
        if (token == "..") {
            if (!stk.empty()) {
                stk.pop();
            }
        } else {
            stk.push(token);
        }
    }

    std::string res;
    while (!stk.empty()) {
        res = "/" + stk.top() + res;
        stk.pop();
    }
    return res.empty() ? "/" : res;
}

int main() {
    assert(simplifyPath("/home/") == "/home");
    assert(simplifyPath("/../") == "/");
    assert(simplifyPath("/home//foo/") == "/home/foo");
    assert(simplifyPath("/a/./b/../../c/") == "/c");
    std::cout << "simplify_path: ok\n";
    return 0;
}
