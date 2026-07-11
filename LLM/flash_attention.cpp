// FlashAttention-V1 block-tile simulator — plain C++ (no PyTorch).
//
// Mirrors LLM/flash_attention.py: tile Q/K/V into SRAM-sized blocks and fuse
// Online Softmax across K/V column blocks without materializing N×N scores.
//
// Whiteboard talking points:
// - Naive attention writes full score matrix -> O(N²) HBM traffic.
// - FlashAttention keeps m/d accumulators in registers per Q row block.
// - Outer loop over Q rows, inner loop over K/V columns (standard V1 tiling).
// - Production kernels fuse even further and use warp-level primitives.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

struct Tensor4D {
    int batch = 0;
    int heads = 0;
    int seq = 0;
    int head_dim = 0;
    std::vector<float> data;

    float& at(int b, int h, int s, int d) {
        return data[((b * heads + h) * seq + s) * head_dim + d];
    }

    float at(int b, int h, int s, int d) const {
        return data[((b * heads + h) * seq + s) * head_dim + d];
    }
};

static Tensor4D zeros_like(const Tensor4D& ref) {
    return Tensor4D{ref.batch, ref.heads, ref.seq, ref.head_dim,
                    std::vector<float>(ref.data.size(), 0.0f)};
}

static Tensor4D flash_attention(const Tensor4D& Q, const Tensor4D& K, const Tensor4D& V,
                                int Br, int Bc) {
    if (Q.batch != K.batch || Q.heads != K.heads || Q.head_dim != K.head_dim ||
        K.batch != V.batch || K.heads != V.heads || K.head_dim != V.head_dim) {
        throw std::invalid_argument("Q/K/V shape mismatch");
    }

    const int B = Q.batch;
    const int H = Q.heads;
    const int q_len = Q.seq;
    const int kv_len = K.seq;
    const int d = Q.head_dim;
    const float scale = 1.0f / std::sqrt(static_cast<float>(d));
    const float neg_inf = -std::numeric_limits<float>::infinity();

    Tensor4D O = zeros_like(Q);

    for (int i = 0; i < q_len; i += Br) {
        const int br = std::min(Br, q_len - i);

        std::vector<float> O_i(static_cast<size_t>(B) * H * br * d, 0.0f);
        std::vector<float> m_old(static_cast<size_t>(B) * H * br, neg_inf);
        std::vector<float> d_old(static_cast<size_t>(B) * H * br, 0.0f);

        auto o_idx = [&](int b, int h, int r, int dd) {
            return ((b * H + h) * br + r) * d + dd;
        };
        auto stat_idx = [&](int b, int h, int r) { return (b * H + h) * br + r; };

        for (int j = 0; j < kv_len; j += Bc) {
            const int bc = std::min(Bc, kv_len - j);

            for (int b = 0; b < B; ++b) {
                for (int h = 0; h < H; ++h) {
                    for (int r = 0; r < br; ++r) {
                        const int si = stat_idx(b, h, r);

                        std::vector<float> scores(static_cast<size_t>(bc), 0.0f);
                        for (int c = 0; c < bc; ++c) {
                            float dot = 0.0f;
                            for (int dd = 0; dd < d; ++dd) {
                                dot += Q.at(b, h, i + r, dd) * K.at(b, h, j + c, dd);
                            }
                            scores[static_cast<size_t>(c)] = dot * scale;
                        }

                        const float m_block =
                            *std::max_element(scores.begin(), scores.end());
                        std::vector<float> exp_s(static_cast<size_t>(bc), 0.0f);
                        float d_block = 0.0f;
                        for (int c = 0; c < bc; ++c) {
                            exp_s[static_cast<size_t>(c)] =
                                std::exp(scores[static_cast<size_t>(c)] - m_block);
                            d_block += exp_s[static_cast<size_t>(c)];
                        }

                        const float m_new = std::max(m_old[static_cast<size_t>(si)], m_block);
                        const float alpha = std::exp(m_old[static_cast<size_t>(si)] - m_new);
                        const float beta = std::exp(m_block - m_new);
                        const float d_new =
                            d_old[static_cast<size_t>(si)] * alpha + d_block * beta;
                        const float corr =
                            (d_old[static_cast<size_t>(si)] * alpha) / (d_new + 1e-9f);

                        for (int dd = 0; dd < d; ++dd) {
                            float sum = O_i[o_idx(b, h, r, dd)] * corr;
                            for (int c = 0; c < bc; ++c) {
                                sum += (exp_s[static_cast<size_t>(c)] * beta) *
                                       V.at(b, h, j + c, dd) / (d_new + 1e-9f);
                            }
                            O_i[o_idx(b, h, r, dd)] = sum;
                        }

                        m_old[static_cast<size_t>(si)] = m_new;
                        d_old[static_cast<size_t>(si)] = d_new;
                    }
                }
            }
        }

        for (int b = 0; b < B; ++b) {
            for (int h = 0; h < H; ++h) {
                for (int r = 0; r < br; ++r) {
                    for (int dd = 0; dd < d; ++dd) {
                        O.at(b, h, i + r, dd) = O_i[o_idx(b, h, r, dd)];
                    }
                }
            }
        }
    }

    return O;
}

static std::vector<float> softmax_row(const std::vector<float>& row) {
    const float max_v = *std::max_element(row.begin(), row.end());
    std::vector<float> out(row.size());
    float sum = 0.0f;
    for (size_t i = 0; i < row.size(); ++i) {
        out[i] = std::exp(row[i] - max_v);
        sum += out[i];
    }
    for (float& v : out) {
        v /= sum;
    }
    return out;
}

static Tensor4D naive_attention(const Tensor4D& Q, const Tensor4D& K, const Tensor4D& V) {
    const int B = Q.batch;
    const int H = Q.heads;
    const int q_len = Q.seq;
    const int kv_len = K.seq;
    const int d = Q.head_dim;
    const float scale = 1.0f / std::sqrt(static_cast<float>(d));

    Tensor4D O = zeros_like(Q);

    for (int b = 0; b < B; ++b) {
        for (int h = 0; h < H; ++h) {
            for (int i = 0; i < q_len; ++i) {
                std::vector<float> scores(static_cast<size_t>(kv_len), 0.0f);
                for (int j = 0; j < kv_len; ++j) {
                    float dot = 0.0f;
                    for (int dd = 0; dd < d; ++dd) {
                        dot += Q.at(b, h, i, dd) * K.at(b, h, j, dd);
                    }
                    scores[static_cast<size_t>(j)] = dot * scale;
                }
                const std::vector<float> attn = softmax_row(scores);
                for (int dd = 0; dd < d; ++dd) {
                    float sum = 0.0f;
                    for (int j = 0; j < kv_len; ++j) {
                        sum += attn[static_cast<size_t>(j)] * V.at(b, h, j, dd);
                    }
                    O.at(b, h, i, dd) = sum;
                }
            }
        }
    }
    return O;
}

static float max_abs_diff(const Tensor4D& a, const Tensor4D& b) {
    float max_diff = 0.0f;
    for (size_t i = 0; i < a.data.size(); ++i) {
        max_diff = std::max(max_diff, std::abs(a.data[i] - b.data[i]));
    }
    return max_diff;
}

static Tensor4D random_tensor(int batch, int heads, int seq, int head_dim, std::mt19937& rng) {
    std::normal_distribution<float> dist(0.0f, 1.0f);
    Tensor4D t{batch, heads, seq, head_dim,
               std::vector<float>(static_cast<size_t>(batch) * heads * seq * head_dim)};
    for (float& v : t.data) {
        v = dist(rng);
    }
    return t;
}

int main() {
    constexpr int Br = 2;
    constexpr int Bc = 2;

    std::mt19937 rng(0);
    const Tensor4D Q = random_tensor(1, 2, 6, 4, rng);
    const Tensor4D K = random_tensor(1, 2, 6, 4, rng);
    const Tensor4D V = random_tensor(1, 2, 6, 4, rng);

    const Tensor4D O_flash = flash_attention(Q, K, V, Br, Bc);
    const Tensor4D O_naive = naive_attention(Q, K, V);

    const float max_diff = max_abs_diff(O_flash, O_naive);
    std::cout << "--- FlashAttention Simulator (Br=" << Br << ", Bc=" << Bc << ", seq=6) ---\n";
    std::cout << "Max |flash - naive|: " << max_diff << "\n";

    assert(max_diff < 1e-3f);
    std::cout << "flash_attention: all tests passed\n";
    return 0;
}
