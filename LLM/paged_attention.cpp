// PagedAttention — discrete physical KV blocks with per-request block tables.
//
// Mirrors LLM/paged_attention.py using plain C++ (no PyTorch).
//
// Whiteboard talking points:
// - Logical KV sequence is split into fixed-size blocks; block_table maps
//   logical block index -> physical block id in a global HBM pool.
// - append_and_manage_kv allocates non-contiguous blocks on demand (no torch.cat).
// - paged_attention_forward gathers K/V via the page table, then global softmax.
// - free_block returns physical ids to the pool for reuse (vLLM-style serving).

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

class BlockManager {
 public:
    explicit BlockManager(int num_blocks) : ref_counts_(num_blocks, 0) {
        free_blocks_.reserve(num_blocks);
        for (int i = 0; i < num_blocks; ++i) {
            free_blocks_.push_back(i);
        }
    }

    int allocate_block() {
        if (free_blocks_.empty()) {
            throw std::runtime_error("GPU HBM Out of Memory! global block pool exhausted");
        }
        const int block_id = free_blocks_.front();
        free_blocks_.erase(free_blocks_.begin());
        ref_counts_[static_cast<size_t>(block_id)] = 1;
        return block_id;
    }

    void free_block(int block_id) {
        if (ref_counts_[static_cast<size_t>(block_id)] > 0) {
            --ref_counts_[static_cast<size_t>(block_id)];
            if (ref_counts_[static_cast<size_t>(block_id)] == 0) {
                free_blocks_.push_back(block_id);
                std::sort(free_blocks_.begin(), free_blocks_.end());
            }
        }
    }

    int num_free_blocks() const { return static_cast<int>(free_blocks_.size()); }

    const std::vector<int>& free_blocks() const { return free_blocks_; }

 private:
    std::vector<int> free_blocks_;
    std::vector<int> ref_counts_;
};

class GpuKvBuffer {
 public:
    GpuKvBuffer(int num_blocks, int block_size, int num_heads, int head_dim)
        : block_size_(block_size),
          num_heads_(num_heads),
          head_dim_(head_dim),
          data_(static_cast<size_t>(num_blocks) * 2 * num_heads * block_size * head_dim,
                0.0f) {}

    float& at(int block, int kv, int head, int offset, int dim) {
        return data_[index(block, kv, head, offset, dim)];
    }

    float at(int block, int kv, int head, int offset, int dim) const {
        return data_[index(block, kv, head, offset, dim)];
    }

    int block_size() const { return block_size_; }
    int num_heads() const { return num_heads_; }
    int head_dim() const { return head_dim_; }

 private:
    int block_size_;
    int num_heads_;
    int head_dim_;
    std::vector<float> data_;

    size_t index(int block, int kv, int head, int offset, int dim) const {
        return (((static_cast<size_t>(block) * 2 + kv) * num_heads_ + head) * block_size_ +
                offset) *
                   head_dim_ +
               dim;
    }
};

class AdvancedPagedAttentionEngine {
 public:
    AdvancedPagedAttentionEngine(BlockManager& manager, int num_blocks, int block_size,
                                 int num_heads, int head_dim)
        : manager_(manager),
          block_size_(block_size),
          num_heads_(num_heads),
          head_dim_(head_dim),
          buffer_(num_blocks, block_size, num_heads, head_dim) {}

    void append_and_manage_kv(const std::vector<float>& k, const std::vector<float>& v,
                              std::vector<int>& block_table, int global_seq_len,
                              bool verbose = true) {
        if (global_seq_len % block_size_ == 0) {
            const int new_id = manager_.allocate_block();
            block_table.push_back(new_id);
            if (verbose) {
                std::cout << "  [alloc] new physical block id: " << new_id << ", table: [";
                for (size_t i = 0; i < block_table.size(); ++i) {
                    if (i > 0) {
                        std::cout << ", ";
                    }
                    std::cout << block_table[i];
                }
                std::cout << "]\n";
            }
        }

        const int logical_idx = global_seq_len / block_size_;
        const int offset = global_seq_len % block_size_;
        const int physical_id = block_table[static_cast<size_t>(logical_idx)];

        for (int h = 0; h < num_heads_; ++h) {
            for (int d = 0; d < head_dim_; ++d) {
                buffer_.at(physical_id, 0, h, offset, d) =
                    k[static_cast<size_t>(h) * head_dim_ + d];
                buffer_.at(physical_id, 1, h, offset, d) =
                    v[static_cast<size_t>(h) * head_dim_ + d];
            }
        }
    }

    std::vector<float> paged_attention_forward(const std::vector<float>& q,
                                               const std::vector<int>& block_table,
                                               int current_seq_len) const {
        const float scale = 1.0f / std::sqrt(static_cast<float>(head_dim_));
        std::vector<std::vector<float>> all_logits(static_cast<size_t>(num_heads_));

        int token_counter = 0;
        for (int physical_id : block_table) {
            const int rem = current_seq_len - token_counter;
            const int actual = std::min(block_size_, rem);
            if (actual <= 0) {
                break;
            }

            for (int h = 0; h < num_heads_; ++h) {
                for (int t = 0; t < actual; ++t) {
                    float dot = 0.0f;
                    for (int d = 0; d < head_dim_; ++d) {
                        dot += q[static_cast<size_t>(h) * head_dim_ + d] *
                               buffer_.at(physical_id, 0, h, t, d);
                    }
                    all_logits[static_cast<size_t>(h)].push_back(dot * scale);
                }
            }
            token_counter += actual;
        }

        std::vector<std::vector<float>> attn_weights(static_cast<size_t>(num_heads_));
        for (int h = 0; h < num_heads_; ++h) {
            const auto& logits = all_logits[static_cast<size_t>(h)];
            float max_v = logits.empty() ? 0.0f : logits[0];
            for (float v : logits) {
                max_v = std::max(max_v, v);
            }
            attn_weights[static_cast<size_t>(h)].resize(logits.size());
            float sum = 0.0f;
            for (size_t i = 0; i < logits.size(); ++i) {
                attn_weights[static_cast<size_t>(h)][i] = std::exp(logits[i] - max_v);
                sum += attn_weights[static_cast<size_t>(h)][i];
            }
            for (float& w : attn_weights[static_cast<size_t>(h)]) {
                w /= sum;
            }
        }

        std::vector<float> context(static_cast<size_t>(num_heads_) * head_dim_, 0.0f);
        token_counter = 0;
        for (int physical_id : block_table) {
            const int rem = current_seq_len - token_counter;
            const int actual = std::min(block_size_, rem);
            if (actual <= 0) {
                break;
            }

            for (int h = 0; h < num_heads_; ++h) {
                for (int t = 0; t < actual; ++t) {
                    const float w =
                        attn_weights[static_cast<size_t>(h)][static_cast<size_t>(token_counter + t)];
                    for (int d = 0; d < head_dim_; ++d) {
                        context[static_cast<size_t>(h) * head_dim_ + d] +=
                            w * buffer_.at(physical_id, 1, h, t, d);
                    }
                }
            }
            token_counter += actual;
        }

        return context;
    }

 private:
    BlockManager& manager_;
    int block_size_;
    int num_heads_;
    int head_dim_;
    GpuKvBuffer buffer_;
};

static std::vector<float> random_kv(int num_heads, int head_dim, std::mt19937& rng) {
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> x(static_cast<size_t>(num_heads) * head_dim);
    for (float& v : x) {
        v = dist(rng);
    }
    return x;
}

static void print_free_blocks(const BlockManager& manager) {
    std::cout << "free blocks (" << manager.num_free_blocks() << "): [";
    const auto& blocks = manager.free_blocks();
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (i > 0) {
            std::cout << ", ";
        }
        std::cout << blocks[i];
    }
    std::cout << "]\n";
}

static void demo() {
    constexpr int kNumBlocks = 5;
    constexpr int kBlockSize = 2;
    constexpr int kNumHeads = 2;
    constexpr int kHeadDim = 64;

    BlockManager manager(kNumBlocks);
    AdvancedPagedAttentionEngine engine(manager, kNumBlocks, kBlockSize, kNumHeads, kHeadDim);
    std::mt19937 rng(0);

    std::cout << "Initial ";
    print_free_blocks(manager);

    std::cout << "\n=== User A ===\n";
    std::vector<int> user_a_table;
    int user_a_len = 0;
    for (int step = 0; step < 3; ++step) {
        std::cout << "User A token " << (step + 1) << ":\n";
        const std::vector<float> q = random_kv(kNumHeads, kHeadDim, rng);
        const std::vector<float> k = random_kv(kNumHeads, kHeadDim, rng);
        const std::vector<float> v = random_kv(kNumHeads, kHeadDim, rng);
        engine.append_and_manage_kv(k, v, user_a_table, user_a_len);
        ++user_a_len;
        const std::vector<float> out =
            engine.paged_attention_forward(q, user_a_table, user_a_len);
        assert(static_cast<int>(out.size()) == kNumHeads * kHeadDim);
    }
    std::cout << "User A table: [";
    for (size_t i = 0; i < user_a_table.size(); ++i) {
        if (i > 0) {
            std::cout << ", ";
        }
        std::cout << user_a_table[i];
    }
    std::cout << "], free=" << manager.num_free_blocks() << "\n";
    assert(user_a_table.size() == 2);
    assert(manager.num_free_blocks() == 3);

    std::cout << "\n=== User B ===\n";
    std::vector<int> user_b_table;
    int user_b_len = 0;
    for (int step = 0; step < 2; ++step) {
        std::cout << "User B token " << (step + 1) << ":\n";
        const std::vector<float> q = random_kv(kNumHeads, kHeadDim, rng);
        const std::vector<float> k = random_kv(kNumHeads, kHeadDim, rng);
        const std::vector<float> v = random_kv(kNumHeads, kHeadDim, rng);
        engine.append_and_manage_kv(k, v, user_b_table, user_b_len);
        ++user_b_len;
        engine.paged_attention_forward(q, user_b_table, user_b_len);
    }
    std::cout << "User B table size=" << user_b_table.size()
              << ", free=" << manager.num_free_blocks() << "\n";
    assert(user_b_table.size() == 1);
    assert(manager.num_free_blocks() == 2);

    std::cout << "\n=== Release User A ===\n";
    print_free_blocks(manager);
    for (int block_id : user_a_table) {
        manager.free_block(block_id);
    }
    user_a_table.clear();
    print_free_blocks(manager);
    assert(manager.num_free_blocks() == 4);
}

int main() {
    demo();
    std::cout << "paged_attention: all tests passed\n";
    return 0;
}
