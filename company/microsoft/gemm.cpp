// GEMM optimization ladder: naive IJK -> IKJ -> blocked -> optional AVX-512 FMA.
//
// Whiteboard talking points:
// - Row-major B[k][j] with I-J-K inner k loop jumps by N -> cache misses.
// - IKJ hoists A[i][k] to a register; inner j scans contiguous B[k][*] and C[i][*].
// - Blocked GEMM keeps tiles in L1/L2 for temporal reuse.
// - AVX-512 processes 16 floats per FMA when compiled with -DGEMM_AVX512 -mavx512f.

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>

#if defined(GEMM_AVX512)
#include <immintrin.h>
#endif

using Clock = std::chrono::steady_clock;

static double elapsed_ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

static void gemm_naive_ijk(const std::vector<float>& A, const std::vector<float>& B,
                           std::vector<float>& C, int n) {
    std::fill(C.begin(), C.end(), 0.0f);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < n; ++k) {
                sum += A[static_cast<size_t>(i) * n + k] * B[static_cast<size_t>(k) * n + j];
            }
            C[static_cast<size_t>(i) * n + j] = sum;
        }
    }
}

static void gemm_ikj(const std::vector<float>& A, const std::vector<float>& B,
                     std::vector<float>& C, int n) {
    std::fill(C.begin(), C.end(), 0.0f);
    for (int i = 0; i < n; ++i) {
        for (int k = 0; k < n; ++k) {
            const float r = A[static_cast<size_t>(i) * n + k];
            for (int j = 0; j < n; ++j) {
                C[static_cast<size_t>(i) * n + j] +=
                    r * B[static_cast<size_t>(k) * n + j];
            }
        }
    }
}

static void gemm_blocked_scalar(const std::vector<float>& A, const std::vector<float>& B,
                                std::vector<float>& C, int n, int block_size) {
    std::fill(C.begin(), C.end(), 0.0f);
    for (int sj = 0; sj < n; sj += block_size) {
        for (int sk = 0; sk < n; sk += block_size) {
            for (int si = 0; si < n; si += block_size) {
                const int i_end = std::min(si + block_size, n);
                const int k_end = std::min(sk + block_size, n);
                const int j_end = std::min(sj + block_size, n);
                for (int i = si; i < i_end; ++i) {
                    for (int k = sk; k < k_end; ++k) {
                        const float r = A[static_cast<size_t>(i) * n + k];
                        for (int j = sj; j < j_end; ++j) {
                            C[static_cast<size_t>(i) * n + j] +=
                                r * B[static_cast<size_t>(k) * n + j];
                        }
                    }
                }
            }
        }
    }
}

#if defined(GEMM_AVX512)

static void gemm_blocked_avx512(const std::vector<float>& A, const std::vector<float>& B,
                                std::vector<float>& C, int n, int block_size) {
    std::fill(C.begin(), C.end(), 0.0f);
    for (int sj = 0; sj < n; sj += block_size) {
        for (int sk = 0; sk < n; sk += block_size) {
            for (int si = 0; si < n; si += block_size) {
                const int i_end = std::min(si + block_size, n);
                const int k_end = std::min(sk + block_size, n);
                const int j_end = std::min(sj + block_size, n);
                for (int i = si; i < i_end; ++i) {
                    for (int k = sk; k < k_end; ++k) {
                        const __m512 vec_a = _mm512_set1_ps(A[static_cast<size_t>(i) * n + k]);
                        int j = sj;
                        for (; j + 16 <= j_end; j += 16) {
                            float* c_ptr = &C[static_cast<size_t>(i) * n + j];
                            float* b_ptr = &B[static_cast<size_t>(k) * n + j];
                            __m512 vec_c = _mm512_loadu_ps(c_ptr);
                            __m512 vec_b = _mm512_loadu_ps(b_ptr);
                            vec_c = _mm512_fmadd_ps(vec_a, vec_b, vec_c);
                            _mm512_storeu_ps(c_ptr, vec_c);
                        }
                        for (; j < j_end; ++j) {
                            C[static_cast<size_t>(i) * n + j] +=
                                A[static_cast<size_t>(i) * n + k] *
                                B[static_cast<size_t>(k) * n + j];
                        }
                    }
                }
            }
        }
    }
}

#endif

static float max_abs_diff(const std::vector<float>& a, const std::vector<float>& b) {
    float m = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        m = std::max(m, std::abs(a[i] - b[i]));
    }
    return m;
}

static std::vector<float> random_matrix(int n, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> m(static_cast<size_t>(n) * n);
    for (float& v : m) {
        v = dist(rng);
    }
    return m;
}

static void run_case(const std::vector<float>& A, const std::vector<float>& B, int n) {
    std::vector<float> ref(static_cast<size_t>(n) * n, 0.0f);
    gemm_ikj(A, B, ref, n);

    auto bench = [&](auto fn, const char* name) {
        std::vector<float> C(static_cast<size_t>(n) * n);
        const auto t0 = Clock::now();
        fn(A, B, C, n);
        const double ms = elapsed_ms(t0);
        const float diff = max_abs_diff(C, ref);
        std::cout << "  " << name;
        for (int pad = static_cast<int>(strlen(name)); pad < 18; ++pad) {
            std::cout << ' ';
        }
        std::cout << ms << " ms, max diff " << diff << "\n";
        assert(diff < 1e-3f);
    };

    std::cout << "N=" << n << ":\n";
    bench([&](const std::vector<float>& a, const std::vector<float>& b, std::vector<float>& c,
              int size) { gemm_naive_ijk(a, b, c, size); },
          "naive_ijk");
    bench([&](const std::vector<float>& a, const std::vector<float>& b, std::vector<float>& c,
              int size) { gemm_ikj(a, b, c, size); },
          "ikj");
    bench([&](const std::vector<float>& a, const std::vector<float>& b, std::vector<float>& c,
              int size) { gemm_blocked_scalar(a, b, c, size, 64); },
          "blocked(64)");
#if defined(GEMM_AVX512)
    bench([&](const std::vector<float>& a, const std::vector<float>& b, std::vector<float>& c,
              int size) { gemm_blocked_avx512(a, b, c, size, 64); },
          "blocked_avx512");
#endif
}

int main() {
    std::mt19937 rng(0);
    const std::vector<float> A = random_matrix(256, rng);
    const std::vector<float> B = random_matrix(256, rng);

    std::cout << "=== GEMM optimization ladder ===\n";
#if defined(GEMM_AVX512)
    std::cout << "AVX-512 FMA enabled\n";
#else
    std::cout << "Scalar only (build with -DGEMM_AVX512 -mavx512f for SIMD path)\n";
#endif

    run_case(A, B, 256);
    std::cout << "gemm: all tests passed\n";
    return 0;
}
