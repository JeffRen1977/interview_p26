// Topological sort of an ISP / CV node graph, with cycle detection and a
// buffer-lifetime pass. "Compile this pipeline graph into an execution order"
// is Meta's way of asking for a topo sort without saying "topo sort".

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

struct Node {
    std::string name;
    std::vector<int> inputs;     // indices of producer nodes
};

// ---------------------------------------------------------------------------
// 1. Kahn's algorithm. Iterative, so a 500-node graph cannot blow the stack,
//    and it detects a cycle for free: if fewer than N nodes come out, the
//    remainder are exactly the nodes on or downstream of a cycle.
// ---------------------------------------------------------------------------

bool topo_sort(const std::vector<Node>& nodes, std::vector<int>* order) {
    const int n = static_cast<int>(nodes.size());
    std::vector<int> indeg(n, 0);
    std::vector<std::vector<int>> succ(n);
    for (int v = 0; v < n; ++v)
        for (int u : nodes[v].inputs) {
            succ[u].push_back(v);
            ++indeg[v];
        }

    // Min-heap on index keeps the output deterministic. A camera pipeline that
    // reorders itself between builds is a debugging nightmare — same graph must
    // always produce the same schedule.
    std::priority_queue<int, std::vector<int>, std::greater<int>> ready;
    for (int v = 0; v < n; ++v) if (indeg[v] == 0) ready.push(v);

    order->clear();
    order->reserve(static_cast<size_t>(n));
    while (!ready.empty()) {
        const int v = ready.top();
        ready.pop();
        order->push_back(v);
        for (int w : succ[v]) if (--indeg[w] == 0) ready.push(w);
    }
    return static_cast<int>(order->size()) == n;   // false => cycle
}

// ---------------------------------------------------------------------------
// 2. Name the cycle. "It has a cycle" is not a useful error message when a
//    tuning engineer wires IPE back into IFE by accident — you must print the
//    actual loop. DFS with white/grey/black colours, iterative.
// ---------------------------------------------------------------------------

bool find_cycle(const std::vector<Node>& nodes, std::vector<int>* cycle) {
    const int n = static_cast<int>(nodes.size());
    std::vector<std::vector<int>> succ(n);
    for (int v = 0; v < n; ++v)
        for (int u : nodes[v].inputs) succ[u].push_back(v);

    enum { White = 0, Grey = 1, Black = 2 };
    std::vector<int> color(n, White), parent(n, -1);
    cycle->clear();

    for (int start = 0; start < n; ++start) {
        if (color[start] != White) continue;
        std::vector<std::pair<int, size_t>> stack{{start, 0}};
        color[start] = Grey;
        while (!stack.empty()) {
            auto& [v, i] = stack.back();
            if (i < succ[v].size()) {
                const int w = succ[v][i++];
                if (color[w] == Grey) {
                    // Walk the parent chain from v back to w.
                    for (int x = v; x != w; x = parent[x]) cycle->push_back(x);
                    cycle->push_back(w);
                    std::reverse(cycle->begin(), cycle->end());
                    return true;
                }
                if (color[w] == White) {
                    color[w] = Grey;
                    parent[w] = v;
                    stack.push_back({w, 0});
                }
            } else {
                color[v] = Black;
                stack.pop_back();
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// 3. The part that makes it a camera question rather than a CS-101 question:
//    peak buffer count. Given the schedule, a node's output buffer can be
//    recycled once every consumer has run. Walk the order, refcount by
//    consumer count, and track the high-water mark — that number is what you
//    take to the memory budget review.
// ---------------------------------------------------------------------------

struct BufferPlan {
    int peak_live;                 // max simultaneously-held output buffers
    std::vector<int> live_after;   // live count after each scheduled node
};

BufferPlan plan_buffers(const std::vector<Node>& nodes, const std::vector<int>& order) {
    const int n = static_cast<int>(nodes.size());
    std::vector<int> consumers(n, 0);
    for (int v = 0; v < n; ++v)
        for (int u : nodes[v].inputs) ++consumers[u];

    // A node with no consumers is a sink (encoder, display); its buffer is
    // handed out of the graph, so count it as live for the rest of the frame.
    std::vector<int> remaining = consumers;
    BufferPlan plan{0, {}};
    int live = 0;
    for (int v : order) {
        ++live;                                   // v allocates its output...
        // ...while every input it reads is still held, so the peak is here.
        plan.peak_live = std::max(plan.peak_live, live);
        for (int u : nodes[v].inputs)
            if (--remaining[u] == 0) --live;      // u's buffer can be recycled
        plan.live_after.push_back(live);
    }
    return plan;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static bool is_valid_order(const std::vector<Node>& nodes, const std::vector<int>& order) {
    std::vector<int> pos(nodes.size(), -1);
    for (size_t i = 0; i < order.size(); ++i) pos[static_cast<size_t>(order[i])] = static_cast<int>(i);
    for (size_t v = 0; v < nodes.size(); ++v)
        for (int u : nodes[v].inputs)
            if (pos[static_cast<size_t>(u)] < 0 || pos[static_cast<size_t>(u)] > pos[v]) return false;
    return true;
}

// A realistic graph:
//   0 sensor -> 1 IFE -> {2 stats, 3 BPS}
//   3 BPS -> 4 IPE -> {5 display, 6 encoder}
//   2 stats -> 7 3A   (a leaf that must still be scheduled)
static std::vector<Node> pipeline() {
    return {
        {"sensor",  {}},
        {"ife",     {0}},
        {"stats",   {1}},
        {"bps",     {1}},
        {"ipe",     {3}},
        {"display", {4}},
        {"encoder", {4}},
        {"aaa",     {2}},
    };
}

static void test_pipeline_order() {
    auto g = pipeline();
    std::vector<int> order;
    assert(topo_sort(g, &order));
    assert(order.size() == g.size());
    assert(is_valid_order(g, order));
    assert(order[0] == 0 && order[1] == 1);
}

static void test_determinism() {
    auto g = pipeline();
    std::vector<int> a, b;
    assert(topo_sort(g, &a) && topo_sort(g, &b));
    assert(a == b);
}

static void test_disconnected_components() {
    // Two independent pipelines on one SoC (tracking + media). Both must appear.
    std::vector<Node> g = {
        {"cam0", {}}, {"fe0", {0}},
        {"cam1", {}}, {"fe1", {2}}, {"cv", {3}},
    };
    std::vector<int> order;
    assert(topo_sort(g, &order));
    assert(order.size() == 5 && is_valid_order(g, order));
}

static void test_cycle_detected_and_named() {
    // ipe feeds back into bps: 3 <- 4 <- 3
    std::vector<Node> g = {
        {"sensor", {}}, {"ife", {0}}, {"stats", {1}},
        {"bps", {1, 4}}, {"ipe", {3}},
    };
    std::vector<int> order;
    assert(!topo_sort(g, &order));
    assert(order.size() < g.size());

    std::vector<int> cyc;
    assert(find_cycle(g, &cyc));
    assert(cyc.size() == 2);
    assert((cyc[0] == 3 && cyc[1] == 4) || (cyc[0] == 4 && cyc[1] == 3));
}

static void test_self_loop() {
    std::vector<Node> g = {{"a", {}}, {"tnr", {0, 1}}};   // TNR reading its own output
    std::vector<int> order, cyc;
    assert(!topo_sort(g, &order));
    assert(find_cycle(g, &cyc));
    assert(cyc.size() == 1 && cyc[0] == 1);
}

static void test_no_cycle_reports_none() {
    auto g = pipeline();
    std::vector<int> cyc;
    assert(!find_cycle(g, &cyc));
}

static void test_diamond_is_not_a_cycle() {
    // 0 -> {1,2} -> 3. Shared producer, two consumers, one merge.
    std::vector<Node> g = {{"src", {}}, {"a", {0}}, {"b", {0}}, {"merge", {1, 2}}};
    std::vector<int> order, cyc;
    assert(topo_sort(g, &order) && is_valid_order(g, order));
    assert(!find_cycle(g, &cyc));
}

static void test_buffer_peak() {
    // Straight chain 0->1->2->3: at most 2 buffers live at once.
    std::vector<Node> chain = {{"a", {}}, {"b", {0}}, {"c", {1}}, {"d", {2}}};
    std::vector<int> order;
    assert(topo_sort(chain, &order));
    auto plan = plan_buffers(chain, order);
    assert(plan.peak_live == 2);
    assert(plan.live_after.back() == 1);   // only the sink's output survives

    // The diamond needs 3: src's buffer stays live while a and b both exist.
    std::vector<Node> diamond = {{"src", {}}, {"a", {0}}, {"b", {0}}, {"m", {1, 2}}};
    assert(topo_sort(diamond, &order));
    assert(plan_buffers(diamond, order).peak_live == 3);

    // The full pipeline: two sinks survive to the end.
    auto g = pipeline();
    assert(topo_sort(g, &order));
    auto p = plan_buffers(g, order);
    assert(p.peak_live >= 3);
    assert(p.live_after.back() == 3);      // display, encoder, aaa
}

int main() {
    test_pipeline_order();
    test_determinism();
    test_disconnected_components();
    test_cycle_detected_and_named();
    test_self_loop();
    test_no_cycle_reports_none();
    test_diamond_is_not_a_cycle();
    test_buffer_peak();
    std::cout << "isp_graph_topo: ok\n";
    return 0;
}
