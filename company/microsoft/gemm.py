"""
GEMM 优化推演 — Naive / IKJ / Blocked（NumPy 对照）

运行
----
    cd microsoft
    python3 gemm.py
"""

from __future__ import annotations

import time

import numpy as np


def gemm_naive_ijk(A: np.ndarray, B: np.ndarray) -> np.ndarray:
    """Classic I-J-K: B accessed by column -> poor cache on row-major."""
    n = A.shape[0]
    C = np.zeros((n, n), dtype=np.float64)
    for i in range(n):
        for j in range(n):
            s = 0.0
            for k in range(n):
                s += A[i, k] * B[k, j]
            C[i, j] = s
    return C


def gemm_ikj(A: np.ndarray, B: np.ndarray) -> np.ndarray:
    """I-K-J: inner j loop is row-contiguous for B and C."""
    n = A.shape[0]
    C = np.zeros((n, n), dtype=np.float64)
    for i in range(n):
        for k in range(n):
            r = A[i, k]
            for j in range(n):
                C[i, j] += r * B[k, j]
    return C


def gemm_blocked(A: np.ndarray, B: np.ndarray, block_size: int = 64) -> np.ndarray:
    """Blocked GEMM: tiles fit in L1/L2 for temporal locality."""
    n = A.shape[0]
    C = np.zeros((n, n), dtype=np.float64)
    for sj in range(0, n, block_size):
        for sk in range(0, n, block_size):
            for si in range(0, n, block_size):
                i_end = min(si + block_size, n)
                k_end = min(sk + block_size, n)
                j_end = min(sj + block_size, n)
                for i in range(si, i_end):
                    for k in range(sk, k_end):
                        r = A[i, k]
                        for j in range(sj, j_end):
                            C[i, j] += r * B[k, j]
    return C


def reference(A: np.ndarray, B: np.ndarray) -> np.ndarray:
    return A @ B


def _max_diff(a: np.ndarray, b: np.ndarray) -> float:
    return float(np.max(np.abs(a - b)))


def _bench(fn, A: np.ndarray, B: np.ndarray, label: str) -> None:
    t0 = time.perf_counter()
    out = fn(A, B)
    elapsed = time.perf_counter() - t0
    print(f"  {label:12s}  {elapsed * 1000:8.2f} ms")
    return out


def demo(n: int = 256) -> None:
    rng = np.random.default_rng(0)
    A = rng.standard_normal((n, n), dtype=np.float64)
    B = rng.standard_normal((n, n), dtype=np.float64)

    print(f"=== GEMM N={n} (FP64) ===")
    ref = reference(A, B)

    out_naive = _bench(gemm_naive_ijk, A, B, "naive_ijk")
    print(f"    max diff vs @: {_max_diff(out_naive, ref):.2e}")

    out_ikj = _bench(gemm_ikj, A, B, "ikj")
    assert _max_diff(out_ikj, ref) < 1e-8

    out_blk = _bench(lambda a, b: gemm_blocked(a, b, 64), A, B, "blocked(64)")
    assert _max_diff(out_blk, ref) < 1e-8

    print("All GEMM variants match numpy matmul.")


if __name__ == "__main__":
    demo()
