"""LeetCode 71 - Simplify Path.
Split on '/', skip '' and '.', pop on '..', join remaining with '/'.
"""


def simplify_path(path: str) -> str:
    stk: list[str] = []
    for token in path.split("/"):
        if token == "" or token == ".":
            continue
        if token == "..":
            if stk:
                stk.pop()
        else:
            stk.append(token)
    return "/" + "/".join(stk) if stk else "/"


if __name__ == "__main__":
    assert simplify_path("/home/") == "/home"
    assert simplify_path("/../") == "/"
    assert simplify_path("/home//foo/") == "/home/foo"
    assert simplify_path("/a/./b/../../c/") == "/c"
    print("simplify_path: ok")
