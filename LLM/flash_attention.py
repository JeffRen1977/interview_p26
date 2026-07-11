"""
FlashAttention-V1 分块流模拟 — 纯手工模拟 GPU SRAM 分块与 Online Softmax。

核心思想
--------
标准 Attention 物化完整 N×N 得分矩阵，显存 O(N²)。
FlashAttention 将 Q/K/V 切成小块在 SRAM 中计算，用 Online Softmax 跨块融合，
避免写出完整 attention matrix，显存 O(N)。

运行
----
    cd LLM
    python3 flash_attention.py
"""

from __future__ import annotations

import math

import torch
import torch.nn as nn
import torch.nn.functional as F


class FlashAttentionSimulator(nn.Module):
    """纯手工模拟 GPU SRAM 分块流的 FlashAttention-V1。"""

    def __init__(self, d_model: int, n_heads: int):
        super().__init__()
        self.n_heads = n_heads
        self.head_dim = d_model // n_heads

    def forward(self, Q: torch.Tensor, K: torch.Tensor, V: torch.Tensor,
                Br: int = 2, Bc: int = 2) -> torch.Tensor:
        """
        输入维度均为标准大模型格式：[Batch, num_heads, SeqLen, head_dim]

        Br: Q 行块大小（Query block rows）
        Bc: K/V 列块大小（Key/Value block columns）

        为了方便白板肉眼 Debug，默认将块大小设得极小 (Br=2, Bc=2)。
        """
        B, H, q_len, d = Q.shape
        _, _, kv_len, _ = K.shape

        # 1. 【静态开辟 HBM 空间】最终写回全局显存（HBM）的输出矩阵 O
        O = torch.zeros_like(Q)

        # 2. 【外层：遍历 Q 的行块】每个 Q_i 独立维护 Online Softmax 状态
        for i in range(0, q_len, Br):
            # 模拟搬运当前 Q_i 到 SRAM
            Q_i = Q[:, :, i : i + Br, :]
            O_i = torch.zeros_like(Q_i)

            # 模拟在线局部寄存器状态 (Per-row-block Stats)
            # 在真实 CUDA 中，m_old / d_old 常驻 GPU 寄存器，跨 K/V 列块滚动更新
            m_old = torch.full((B, H, Br, 1), float("-inf"), device=Q.device, dtype=Q.dtype)
            d_old = torch.zeros((B, H, Br, 1), device=Q.device, dtype=Q.dtype)

            # 3. 【内层：遍历 K, V 的列块】模拟 HBM -> SRAM 搬运
            for j in range(0, kv_len, Bc):
                K_j = K[:, :, j : j + Bc, :]  # [B, H, Bc, d]
                V_j = V[:, :, j : j + Bc, :]

                # SRAM 内部寄存器级乘法: S_ij [B, H, Br, Bc]
                S_ij = torch.matmul(Q_i, K_j.transpose(-2, -1)) / math.sqrt(d)

                # --- Online Softmax 核心 ---
                m_block, _ = torch.max(S_ij, dim=-1, keepdim=True)
                exp_S = torch.exp(S_ij - m_block)
                d_block = torch.sum(exp_S, dim=-1, keepdim=True)

                # -------------------------------------------------------------
                # 【工业级高度融合：Online Softmax 跨块对齐公式】
                # -------------------------------------------------------------
                # 1. 动态融合、更新最新的全局最大值
                m_new = torch.max(m_old, m_block)

                # 2. 动态缩放并累加分母
                d_new = d_old * torch.exp(m_old - m_new) + d_block * torch.exp(m_block - m_new)

                # 3. 校正旧 Output 并融入新 V_j 块的贡献
                O_i_corrected = O_i * (d_old * torch.exp(m_old - m_new) / (d_new + 1e-9))
                O_i = O_i_corrected + torch.matmul(
                    exp_S * torch.exp(m_block - m_new), V_j
                ) / (d_new + 1e-9)

                m_old = m_new
                d_old = d_new

            # 4. 将最终 O_i 刷写回 HBM
            O[:, :, i : i + Br, :] = O_i

        return O


def naive_attention(Q: torch.Tensor, K: torch.Tensor, V: torch.Tensor) -> torch.Tensor:
    """标准 Scaled Dot-Product Attention，用于数值对照。"""
    d = Q.shape[-1]
    scores = torch.matmul(Q, K.transpose(-2, -1)) / math.sqrt(d)
    attn = F.softmax(scores, dim=-1)
    return torch.matmul(attn, V)


def demo() -> None:
    torch.manual_seed(0)
    device = "cuda" if torch.cuda.is_available() else "cpu"

    B, H, seq_len, d = 1, 2, 6, 4
    Br, Bc = 2, 2

    Q = torch.randn(B, H, seq_len, d, device=device)
    K = torch.randn(B, H, seq_len, d, device=device)
    V = torch.randn(B, H, seq_len, d, device=device)

    model = FlashAttentionSimulator(d_model=H * d, n_heads=H).to(device)

    print(f"--- FlashAttention Simulator (Br={Br}, Bc={Bc}, seq={seq_len}) ---")
    with torch.no_grad():
        O_flash = model(Q, K, V, Br=Br, Bc=Bc)
        O_naive = naive_attention(Q, K, V)

    max_diff = (O_flash - O_naive).abs().max().item()
    print(f"Output shape: {tuple(O_flash.shape)}")
    print(f"Max |flash - naive|: {max_diff:.2e}")
    assert max_diff < 1e-4, f"numerical mismatch: {max_diff}"
    print("FlashAttention matches naive attention.")


if __name__ == "__main__":
    demo()
