// IR LED blob detection: threshold -> connected components -> sub-pixel
// centroid. This is the controller-tracking / constellation drill.
//
// Two labelling implementations, because interviewers ask for both:
//   1. BFS flood fill  — trivial to write, needs a queue, random access.
//   2. Two-pass + union-find — one streaming pass over rows, what an
//      embedded line-buffered implementation actually does.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <queue>
#include <vector>

struct Blob {
    int label;
    int area;              // pixel count
    double cx, cy;         // intensity-weighted centroid (sub-pixel)
    int min_x, min_y, max_x, max_y;
    uint32_t sum_intensity;
};

// ---------------------------------------------------------------------------
// 1. BFS flood fill. labels[] is 0 for background, >=1 per component.
// ---------------------------------------------------------------------------

int label_blobs_bfs(const uint8_t* img, int width, int height, int stride,
                    uint8_t threshold, std::vector<int>* labels,
                    bool connectivity8 = true) {
    labels->assign(static_cast<size_t>(width) * height, 0);
    int next_label = 0;

    static const int d4x[] = {1, -1, 0, 0};
    static const int d4y[] = {0, 0, 1, -1};
    static const int d8x[] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int d8y[] = {0, 0, 1, -1, 1, -1, 1, -1};
    const int* dx = connectivity8 ? d8x : d4x;
    const int* dy = connectivity8 ? d8y : d4y;
    const int nd = connectivity8 ? 8 : 4;

    std::queue<std::pair<int, int>> q;
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            if (img[static_cast<size_t>(r) * stride + c] < threshold) continue;
            if ((*labels)[static_cast<size_t>(r) * width + c] != 0) continue;

            ++next_label;
            (*labels)[static_cast<size_t>(r) * width + c] = next_label;
            q.push({r, c});
            while (!q.empty()) {
                const auto [cr, cc] = q.front();
                q.pop();
                for (int k = 0; k < nd; ++k) {
                    const int nr = cr + dy[k], nc = cc + dx[k];
                    if (nr < 0 || nr >= height || nc < 0 || nc >= width) continue;
                    if (img[static_cast<size_t>(nr) * stride + nc] < threshold) continue;
                    int& lab = (*labels)[static_cast<size_t>(nr) * width + nc];
                    if (lab != 0) continue;
                    lab = next_label;
                    q.push({nr, nc});
                }
            }
        }
    }
    return next_label;
}

// ---------------------------------------------------------------------------
// 2. Two-pass with union-find. Only the previous row is consulted, so this
//    version works against a 2-row line buffer streamed out of the ISP.
// ---------------------------------------------------------------------------

class UnionFind {
public:
    int make() { parent_.push_back(static_cast<int>(parent_.size())); return static_cast<int>(parent_.size()) - 1; }
    int find(int x) {
        while (parent_[x] != x) { parent_[x] = parent_[parent_[x]]; x = parent_[x]; }  // path halving
        return x;
    }
    void unite(int a, int b) {
        a = find(a); b = find(b);
        if (a != b) parent_[std::max(a, b)] = std::min(a, b);   // keep the smaller root
    }
    size_t size() const { return parent_.size(); }
private:
    std::vector<int> parent_;
};

int label_blobs_union_find(const uint8_t* img, int width, int height, int stride,
                           uint8_t threshold, std::vector<int>* labels) {
    labels->assign(static_cast<size_t>(width) * height, 0);
    UnionFind uf;
    uf.make();   // index 0 reserved for background

    // Pass 1: assign provisional labels, record equivalences.
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            if (img[static_cast<size_t>(r) * stride + c] < threshold) continue;
            // 8-connected causal neighbours: W, NW, N, NE.
            int neigh[4], n = 0;
            const auto get = [&](int rr, int cc) -> int {
                if (rr < 0 || cc < 0 || cc >= width) return 0;
                return (*labels)[static_cast<size_t>(rr) * width + cc];
            };
            for (int lab : {get(r, c - 1), get(r - 1, c - 1), get(r - 1, c), get(r - 1, c + 1)})
                if (lab != 0) neigh[n++] = lab;

            int& self = (*labels)[static_cast<size_t>(r) * width + c];
            if (n == 0) {
                self = uf.make();
            } else {
                self = *std::min_element(neigh, neigh + n);
                for (int k = 0; k < n; ++k) uf.unite(self, neigh[k]);
            }
        }
    }

    // Pass 2: resolve to roots and compact to 1..count.
    std::vector<int> remap(uf.size(), 0);
    int count = 0;
    for (size_t i = 0; i < labels->size(); ++i) {
        int lab = (*labels)[i];
        if (lab == 0) continue;
        const int root = uf.find(lab);
        if (remap[root] == 0) remap[root] = ++count;
        (*labels)[i] = remap[root];
    }
    return count;
}

// ---------------------------------------------------------------------------
// 3. Sub-pixel centroid. Weighting by (intensity - threshold) rather than by a
//    binary mask is what gets you below one pixel of error; the offset removes
//    the pedestal that would otherwise pull every centroid toward the ROI
//    centre.
// ---------------------------------------------------------------------------

std::vector<Blob> measure_blobs(const uint8_t* img, int width, int height, int stride,
                                const std::vector<int>& labels, int n_labels,
                                uint8_t threshold, int min_area, int max_area) {
    std::vector<Blob> blobs(static_cast<size_t>(n_labels));
    std::vector<double> sx(n_labels, 0.0), sy(n_labels, 0.0), sw(n_labels, 0.0);
    for (int i = 0; i < n_labels; ++i) {
        blobs[i] = Blob{i + 1, 0, 0, 0, width, height, -1, -1, 0};
    }

    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            const int lab = labels[static_cast<size_t>(r) * width + c];
            if (lab == 0) continue;
            Blob& b = blobs[static_cast<size_t>(lab) - 1];
            const int v = img[static_cast<size_t>(r) * stride + c];
            const double w = static_cast<double>(v - threshold + 1);
            ++b.area;
            b.sum_intensity += static_cast<uint32_t>(v);
            b.min_x = std::min(b.min_x, c); b.max_x = std::max(b.max_x, c);
            b.min_y = std::min(b.min_y, r); b.max_y = std::max(b.max_y, r);
            sx[static_cast<size_t>(lab) - 1] += w * c;
            sy[static_cast<size_t>(lab) - 1] += w * r;
            sw[static_cast<size_t>(lab) - 1] += w;
        }
    }
    for (int i = 0; i < n_labels; ++i) {
        if (sw[i] > 0) { blobs[i].cx = sx[i] / sw[i]; blobs[i].cy = sy[i] / sw[i]; }
    }

    // Reject noise specks and saturated blooms / ambient IR floods.
    std::vector<Blob> kept;
    for (const Blob& b : blobs)
        if (b.area >= min_area && b.area <= max_area) kept.push_back(b);
    // Brightest first: the tracker wants a deterministic order.
    std::sort(kept.begin(), kept.end(),
              [](const Blob& a, const Blob& b) { return a.sum_intensity > b.sum_intensity; });
    return kept;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void set_px(std::vector<uint8_t>& img, int stride, int r, int c, uint8_t v) {
    img[static_cast<size_t>(r) * stride + c] = v;
}

static void test_two_separate_blobs() {
    const int w = 16, h = 8, stride = 20;
    std::vector<uint8_t> img(static_cast<size_t>(stride) * h, 5);
    // Blob A: 2x2 at (1,1)..(2,2). Blob B: single pixel at (6,12).
    for (int r = 1; r <= 2; ++r) for (int c = 1; c <= 2; ++c) set_px(img, stride, r, c, 250);
    set_px(img, stride, 6, 12, 200);

    std::vector<int> labels;
    const int n = label_blobs_bfs(img.data(), w, h, stride, 128, &labels);
    assert(n == 2);
    auto blobs = measure_blobs(img.data(), w, h, stride, labels, n, 128, 1, 1000);
    assert(blobs.size() == 2);
    // Brightest (larger sum) first.
    assert(blobs[0].area == 4);
    assert(std::abs(blobs[0].cx - 1.5) < 1e-9 && std::abs(blobs[0].cy - 1.5) < 1e-9);
    assert(blobs[1].area == 1);
    assert(std::abs(blobs[1].cx - 12.0) < 1e-9 && std::abs(blobs[1].cy - 6.0) < 1e-9);
}

static void test_connectivity_matters() {
    // Two pixels touching only at a corner: one blob under 8-conn, two under 4.
    const int w = 4, h = 4, stride = 4;
    std::vector<uint8_t> img(static_cast<size_t>(stride) * h, 0);
    set_px(img, stride, 1, 1, 255);
    set_px(img, stride, 2, 2, 255);
    std::vector<int> labels;
    assert(label_blobs_bfs(img.data(), w, h, stride, 128, &labels, true) == 1);
    assert(label_blobs_bfs(img.data(), w, h, stride, 128, &labels, false) == 2);
}

static void test_union_find_matches_bfs_on_u_shape() {
    // A U shape: the two arms get different provisional labels on the first
    // rows and must be merged when the bottom connects them. This is the case
    // a naive one-pass labeller gets wrong.
    const int w = 7, h = 5, stride = 9;
    std::vector<uint8_t> img(static_cast<size_t>(stride) * h, 0);
    for (int r = 0; r < 4; ++r) { set_px(img, stride, r, 1, 255); set_px(img, stride, r, 5, 255); }
    for (int c = 1; c <= 5; ++c) set_px(img, stride, 4, c, 255);

    std::vector<int> a, b;
    const int na = label_blobs_bfs(img.data(), w, h, stride, 128, &a, true);
    const int nb = label_blobs_union_find(img.data(), w, h, stride, 128, &b);
    assert(na == 1 && nb == 1);
    assert(a == b);
}

static void test_union_find_random_agrees_with_bfs() {
    const int w = 24, h = 18, stride = 32;
    uint32_t s = 12345;
    auto rnd = [&s]() { s = s * 1664525u + 1013904223u; return (s >> 16) & 0xFF; };
    for (int trial = 0; trial < 20; ++trial) {
        std::vector<uint8_t> img(static_cast<size_t>(stride) * h, 0);
        for (int r = 0; r < h; ++r)
            for (int c = 0; c < w; ++c)
                set_px(img, stride, r, c, static_cast<uint8_t>(rnd() > 150 ? 255 : 0));
        std::vector<int> a, b;
        const int na = label_blobs_bfs(img.data(), w, h, stride, 128, &a, true);
        const int nb = label_blobs_union_find(img.data(), w, h, stride, 128, &b);
        assert(na == nb);
        // Labels may be numbered differently; compare the partitions.
        std::vector<int> map_ab(static_cast<size_t>(na) + 1, -1);
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] == 0) { assert(b[i] == 0); continue; }
            int& m = map_ab[static_cast<size_t>(a[i])];
            if (m == -1) m = b[i]; else assert(m == b[i]);
        }
    }
}

static void test_subpixel_beats_binary_centroid() {
    // A Gaussian-ish spot centred at x = 4.30. Intensity weighting must land
    // much closer than the binary (bounding-box) centre at 4.0.
    const int w = 9, h = 9, stride = 9;
    std::vector<uint8_t> img(static_cast<size_t>(stride) * h, 0);
    const double truth_x = 4.30, truth_y = 4.0;
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c) {
            const double d2 = (c - truth_x) * (c - truth_x) + (r - truth_y) * (r - truth_y);
            const double v = 255.0 * std::exp(-d2 / 2.0);
            set_px(img, stride, r, c, static_cast<uint8_t>(v));
        }
    std::vector<int> labels;
    const int n = label_blobs_bfs(img.data(), w, h, stride, 30, &labels);
    assert(n == 1);
    auto blobs = measure_blobs(img.data(), w, h, stride, labels, n, 30, 1, 10000);
    assert(blobs.size() == 1);
    const double bbox_cx = 0.5 * (blobs[0].min_x + blobs[0].max_x);
    assert(std::abs(blobs[0].cx - truth_x) < 0.1);
    assert(std::abs(blobs[0].cx - truth_x) < std::abs(bbox_cx - truth_x));
    assert(std::abs(blobs[0].cy - truth_y) < 1e-6);
}

static void test_area_filter_rejects_speck_and_flood() {
    const int w = 12, h = 12, stride = 12;
    std::vector<uint8_t> img(static_cast<size_t>(stride) * h, 0);
    set_px(img, stride, 0, 0, 255);                                   // 1 px speck
    for (int r = 2; r <= 3; ++r) for (int c = 2; c <= 3; ++c) set_px(img, stride, r, c, 255);
    for (int r = 6; r < 12; ++r) for (int c = 6; c < 12; ++c) set_px(img, stride, r, c, 255);  // flood
    std::vector<int> labels;
    const int n = label_blobs_bfs(img.data(), w, h, stride, 128, &labels);
    assert(n == 3);
    auto kept = measure_blobs(img.data(), w, h, stride, labels, n, 128, 2, 16);
    assert(kept.size() == 1 && kept[0].area == 4);
}

int main() {
    test_two_separate_blobs();
    test_connectivity_matters();
    test_union_find_matches_bfs_on_u_shape();
    test_union_find_random_agrees_with_bfs();
    test_subpixel_beats_binary_centroid();
    test_area_filter_rejects_speck_and_flood();
    std::cout << "blob_centroid: ok\n";
    return 0;
}
