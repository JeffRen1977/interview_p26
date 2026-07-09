// KV Cache — minimal multi-head self-attention with rolling K/V cache.
//
// Mirrors interview_handwrite/kv_cache.py using plain C++ (no PyTorch).
//
// Whiteboard talking points:
// - Without KV cache, each decode step recomputes K/V for all past tokens -> O(N^2).
// - Prefill processes the whole prompt once and seeds the cache.
// - Decode feeds one token at a time; only the new token's Q/K/V are projected.
// - concat along the sequence axis is the teaching version; production uses
//   PagedAttention to avoid realloc/copy (vLLM).
// - Prefill is compute-bound; decode becomes memory-bound as cache grows.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

struct Tensor4D {
    int batch = 0;
    int heads = 0;
    int seq = 0;
    int head_dim = 0;
    std::vector<float> data;

    size_t size() const {
        return static_cast<size_t>(batch) * heads * seq * head_dim;
    }

    float& at(int b, int h, int s, int d) {
        return data[((b * heads + h) * seq + s) * head_dim + d];
    }

    float at(int b, int h, int s, int d) const {
        return data[((b * heads + h) * seq + s) * head_dim + d];
    }
};

using KVCache = std::pair<Tensor4D, Tensor4D>;

static std::vector<float> linear(const std::vector<float>& x, int seq_len, int in_dim,
                                 const std::vector<float>& weight, int out_dim) {
    // x: [seq_len, in_dim], weight: [out_dim, in_dim], y = x @ W^T
    std::vector<float> out(static_cast<size_t>(seq_len) * out_dim, 0.0f);
    for (int s = 0; s < seq_len; ++s) {
        for (int o = 0; o < out_dim; ++o) {
            float sum = 0.0f;
            for (int i = 0; i < in_dim; ++i) {
                sum += x[static_cast<size_t>(s) * in_dim + i] *
                       weight[static_cast<size_t>(o) * in_dim + i];
            }
            out[static_cast<size_t>(s) * out_dim + o] = sum;
        }
    }
    return out;
}

static Tensor4D to_heads(const std::vector<float>& projected, int batch, int seq_len,
                         int n_heads, int head_dim) {
    Tensor4D t{batch, n_heads, seq_len, head_dim,
               std::vector<float>(static_cast<size_t>(batch) * n_heads * seq_len * head_dim)};
    const int d_model = n_heads * head_dim;
    for (int b = 0; b < batch; ++b) {
        for (int s = 0; s < seq_len; ++s) {
            for (int h = 0; h < n_heads; ++h) {
                for (int d = 0; d < head_dim; ++d) {
                    const int flat = (b * seq_len + s) * d_model + h * head_dim + d;
                    t.at(b, h, s, d) = projected[static_cast<size_t>(flat)];
                }
            }
        }
    }
    return t;
}

static Tensor4D concat_seq(const Tensor4D& past, const Tensor4D& curr) {
    if (past.batch != curr.batch || past.heads != curr.heads ||
        past.head_dim != curr.head_dim) {
        throw std::invalid_argument("KV cache shape mismatch");
    }
    Tensor4D out{past.batch, past.heads, past.seq + curr.seq, past.head_dim,
                 std::vector<float>(past.size() + curr.size())};
    for (int b = 0; b < out.batch; ++b) {
        for (int h = 0; h < out.heads; ++h) {
            for (int s = 0; s < past.seq; ++s) {
                for (int d = 0; d < out.head_dim; ++d) {
                    out.at(b, h, s, d) = past.at(b, h, s, d);
                }
            }
            for (int s = 0; s < curr.seq; ++s) {
                for (int d = 0; d < out.head_dim; ++d) {
                    out.at(b, h, past.seq + s, d) = curr.at(b, h, s, d);
                }
            }
        }
    }
    return out;
}

static std::vector<float> softmax_row(const std::vector<float>& row) {
    float max_v = *std::max_element(row.begin(), row.end());
    std::vector<float> exp_row(row.size());
    float sum = 0.0f;
    for (size_t i = 0; i < row.size(); ++i) {
        exp_row[i] = std::exp(row[i] - max_v);
        sum += exp_row[i];
    }
    for (float& v : exp_row) {
        v /= sum;
    }
    return exp_row;
}

static Tensor4D scaled_dot_product_attention(const Tensor4D& q, const Tensor4D& k,
                                             const Tensor4D& v) {
    if (q.batch != k.batch || q.heads != k.heads || k.batch != v.batch || k.heads != v.heads ||
        k.seq != v.seq || q.head_dim != k.head_dim || k.head_dim != v.head_dim) {
        throw std::invalid_argument("attention shape mismatch");
    }

    const float scale = 1.0f / std::sqrt(static_cast<float>(q.head_dim));
    Tensor4D context{q.batch, q.heads, q.seq, q.head_dim,
                     std::vector<float>(q.size(), 0.0f)};

    for (int b = 0; b < q.batch; ++b) {
        for (int h = 0; h < q.heads; ++h) {
            for (int sq = 0; sq < q.seq; ++sq) {
                std::vector<float> scores(static_cast<size_t>(k.seq), 0.0f);
                for (int sk = 0; sk < k.seq; ++sk) {
                    float dot = 0.0f;
                    for (int d = 0; d < q.head_dim; ++d) {
                        dot += q.at(b, h, sq, d) * k.at(b, h, sk, d);
                    }
                    scores[static_cast<size_t>(sk)] = dot * scale;
                }

                const std::vector<float> attn = softmax_row(scores);
                for (int d = 0; d < q.head_dim; ++d) {
                    float sum = 0.0f;
                    for (int sk = 0; sk < k.seq; ++sk) {
                        sum += attn[static_cast<size_t>(sk)] * v.at(b, h, sk, d);
                    }
                    context.at(b, h, sq, d) = sum;
                }
            }
        }
    }
    return context;
}

static std::vector<float> merge_heads(const Tensor4D& t) {
    const int d_model = t.heads * t.head_dim;
    std::vector<float> out(static_cast<size_t>(t.batch) * t.seq * d_model, 0.0f);
    for (int b = 0; b < t.batch; ++b) {
        for (int s = 0; s < t.seq; ++s) {
            for (int h = 0; h < t.heads; ++h) {
                for (int d = 0; d < t.head_dim; ++d) {
                    const int flat = (b * t.seq + s) * d_model + h * t.head_dim + d;
                    out[static_cast<size_t>(flat)] = t.at(b, h, s, d);
                }
            }
        }
    }
    return out;
}

class MicroAttentionWithKVCache {
 public:
    MicroAttentionWithKVCache(int d_model, int n_heads)
        : d_model_(d_model), n_heads_(n_heads), head_dim_(d_model / n_heads) {
        if (d_model % n_heads != 0) {
            throw std::invalid_argument("d_model must be divisible by n_heads");
        }
        init_random_weights();
    }

    std::pair<std::vector<float>, KVCache> forward(const std::vector<float>& x, int batch,
                                                   int seq_len,
                                                   const std::optional<KVCache>& kv_cache) const {
        Tensor4D q = project_heads(x, batch, seq_len, q_weight_);
        Tensor4D k = project_heads(x, batch, seq_len, k_weight_);
        Tensor4D v = project_heads(x, batch, seq_len, v_weight_);

        if (kv_cache.has_value()) {
            k = concat_seq(kv_cache->first, k);
            v = concat_seq(kv_cache->second, v);
        }

        const KVCache new_cache = {k, v};
        const Tensor4D context = scaled_dot_product_attention(q, k, v);
        const std::vector<float> merged = merge_heads(context);
        const std::vector<float> output = linear(merged, seq_len, d_model_, out_weight_, d_model_);
        return {output, new_cache};
    }

 private:
    int d_model_;
    int n_heads_;
    int head_dim_;
    std::vector<float> q_weight_;
    std::vector<float> k_weight_;
    std::vector<float> v_weight_;
    std::vector<float> out_weight_;

    void init_random_weights() {
        std::mt19937 rng(42);
        std::normal_distribution<float> dist(0.0f, 0.02f);
        const size_t n = static_cast<size_t>(d_model_) * d_model_;
        q_weight_.resize(n);
        k_weight_.resize(n);
        v_weight_.resize(n);
        out_weight_.resize(n);
        for (size_t i = 0; i < n; ++i) {
            q_weight_[i] = dist(rng);
            k_weight_[i] = dist(rng);
            v_weight_[i] = dist(rng);
            out_weight_[i] = dist(rng);
        }
    }

    Tensor4D project_heads(const std::vector<float>& x, int batch, int seq_len,
                           const std::vector<float>& weight) const {
        const std::vector<float> projected = linear(x, seq_len, d_model_, weight, d_model_);
        return to_heads(projected, batch, seq_len, n_heads_, head_dim_);
    }
};

static std::vector<float> random_input(int batch, int seq_len, int d_model, std::mt19937& rng) {
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> x(static_cast<size_t>(batch) * seq_len * d_model);
    for (float& v : x) {
        v = dist(rng);
    }
    return x;
}

static void print_cache_shape(const char* label, const Tensor4D& k) {
    std::cout << label << " KV Cache shape - K: [" << k.batch << ", " << k.heads << ", " << k.seq
              << ", " << k.head_dim << "]\n";
}

static void demo_autoregressive_decode() {
    constexpr int d_model = 256;
    constexpr int n_heads = 4;
    constexpr int prompt_len = 5;
    constexpr int next_steps = 3;
    constexpr int batch = 1;

    MicroAttentionWithKVCache model(d_model, n_heads);
    std::mt19937 rng(123);

    std::cout << "--- 1. Prefill (prompt_len=" << prompt_len << ") ---\n";
    const std::vector<float> prompt = random_input(batch, prompt_len, d_model, rng);
    auto [prefill_out, cache] = model.forward(prompt, batch, prompt_len, std::nullopt);
    (void)prefill_out;
    print_cache_shape("Prefill done.", cache.first);
    assert(cache.first.batch == 1);
    assert(cache.first.heads == n_heads);
    assert(cache.first.seq == prompt_len);
    assert(cache.first.head_dim == d_model / n_heads);

    std::cout << "\n--- 2. Decode (autoregressive) ---\n";
    std::vector<float> curr = random_input(batch, 1, d_model, rng);
    for (int step = 0; step < next_steps; ++step) {
        auto [out, new_cache] = model.forward(curr, batch, 1, cache);
        (void)out;
        cache = new_cache;
        std::cout << "Token " << (step + 1)
                  << ". Updated cache shape - K: [" << cache.first.batch << ", "
                  << cache.first.heads << ", " << cache.first.seq << ", "
                  << cache.first.head_dim << "]\n";
        assert(cache.first.seq == prompt_len + step + 1);
        curr = random_input(batch, 1, d_model, rng);
    }
}

static bool test_kv_cache_shapes() {
    MicroAttentionWithKVCache model(256, 4);
    std::mt19937 rng(7);
    const std::vector<float> prompt = random_input(1, 5, 256, rng);

    auto [out1, cache] = model.forward(prompt, 1, 5, std::nullopt);
    assert(out1.size() == 5 * 256);
    assert(cache.first.seq == 5);

    const std::vector<float> token = random_input(1, 1, 256, rng);
    auto [out2, cache2] = model.forward(token, 1, 1, cache);
    assert(out2.size() == 256);
    assert(cache2.first.seq == 6);
    assert(cache2.second.seq == 6);
    return true;
}

int main() {
    assert(test_kv_cache_shapes());
    demo_autoregressive_decode();
    std::cout << "kv_cache: all tests passed\n";
    return 0;
}
