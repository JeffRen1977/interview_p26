"""
KV Cache — 大模型 Decode 阶段「空间换时间」极简手写 Demo (PyTorch)

背景
----
在大模型自回归（Autoregressive）推理的 Decode 阶段，每生成一个新 Token，
模型都需要关注（Attention）之前所有生成过的 Token。

如果没有 KV Cache（键值缓存），模型每次为了预测下一个字，都不得不把历史
所有的 Token 重新计算一遍 Q, K, V 矩阵。随着文本变长，这种重复计算会导致
计算量呈平方级 O(N^2) 暴涨，速度越来越慢。

核心思想
--------
历史已经生成过的 Token，它们的 Key 和 Value 矩阵已经是固定不变的了。
我们把它们存在显存里，每次新 Token 进来时，只计算当前这一个 Token 的 Q, K, V，
然后和历史存好的 K、V 拼接起来做 Attention。

两阶段推理
----------
1. Prefill（预填充）：一次性处理整个 Prompt，x 形状 [B, prompt_len, d_model]，
   建立初始 KV Cache。
2. Decode（解码）：每次只输入 1 个新 Token，x 形状 [B, 1, d_model]，
   滚动更新 KV Cache，避免重复计算历史 Token 的 K/V。

面试加分：Principal 级别权衡 (Trade-offs)
-----------------------------------------
1. 显存的动态搬运瓶颈（Memory-bound）
   torch.cat([past_k, k], dim=2) 会随着生成长度不断开辟新连续空间并拷贝数据。
   工业级引擎（如 vLLM）不用 torch.cat，而用 PagedAttention——预先申请离散
   固定大小内存块（Blocks），通过 CUDA 算子指针直接写入页表槽位。

2. 计算密度的断层
   Prefill 阶段矩阵乘吞吐量大，走 Compute-bound；
   Decode 阶段每次只算 1 个 Token，GPU Tensor Cores 饥饿，瓶颈转为
   Memory-bound（反复读取 HBM 中的 past_k / past_v）。
   因此需要 GQA（分组查询注意力）和低比特量化（AWQ）来削减 KV Cache 搬运密度。

运行
----
    cd interview_handwrite
    python3 kv_cache.py
"""

from __future__ import annotations

from typing import Optional, Tuple

import torch
import torch.nn as nn
import torch.nn.functional as F

KVCache = Tuple[torch.Tensor, torch.Tensor]


class MicroAttentionWithKVCache(nn.Module):
    """极简多头自注意力 + KV Cache，展示 Decode 阶段如何复用历史 K/V。"""

    def __init__(self, d_model: int, n_heads: int):
        super().__init__()
        if d_model % n_heads != 0:
            raise ValueError("d_model must be divisible by n_heads")
        self.d_model = d_model
        self.n_heads = n_heads
        self.head_dim = d_model // n_heads

        self.q_proj = nn.Linear(d_model, d_model, bias=False)
        self.k_proj = nn.Linear(d_model, d_model, bias=False)
        self.v_proj = nn.Linear(d_model, d_model, bias=False)
        self.out_proj = nn.Linear(d_model, d_model, bias=False)

    def forward(
        self,
        x: torch.Tensor,
        kv_cache: Optional[KVCache] = None,
    ) -> Tuple[torch.Tensor, KVCache]:
        """
        x: 当前输入
           - Prefill: [B, seq_len_prompt, d_model]
           - Decode:  [B, 1, d_model]（每次只进来 1 个 Token）
        kv_cache: (past_k, past_v)，历史缓存的 Key / Value
        """
        batch_size, seq_len, _ = x.shape

        # [B, S, D] -> [B, num_heads, S, head_dim]
        q = self.q_proj(x).view(batch_size, seq_len, self.n_heads, self.head_dim).transpose(1, 2)
        k = self.k_proj(x).view(batch_size, seq_len, self.n_heads, self.head_dim).transpose(1, 2)
        v = self.v_proj(x).view(batch_size, seq_len, self.n_heads, self.head_dim).transpose(1, 2)

        # 核心优化：拼接历史 K/V，只对新 Token 做投影
        if kv_cache is not None:
            past_k, past_v = kv_cache
            k = torch.cat([past_k, k], dim=2)
            v = torch.cat([past_v, v], dim=2)

        new_kv_cache = (k, v)

        # Scaled Dot-Product Attention
        # q 的 seq_len 在 Decode 时为 1；k 的 seq_len 为全局总长度
        scores = torch.matmul(q, k.transpose(-2, -1)) / (self.head_dim ** 0.5)
        attn_weights = F.softmax(scores, dim=-1)
        context = torch.matmul(attn_weights, v)

        context = context.transpose(1, 2).contiguous().view(batch_size, seq_len, self.d_model)
        output = self.out_proj(context)
        return output, new_kv_cache


def demo_autoregressive_decode(
    d_model: int = 256,
    n_heads: int = 4,
    prompt_len: int = 5,
    next_steps: int = 3,
) -> None:
    """模拟 Prefill + Decode 两阶段自回归推理。"""
    device = "cuda" if torch.cuda.is_available() else "cpu"
    model = MicroAttentionWithKVCache(d_model=d_model, n_heads=n_heads).to(device)
    model.eval()

    batch_size = 1

    # === 阶段一：Prefill ===
    prompt_input = torch.randn(batch_size, prompt_len, d_model, device=device)
    print(f"--- 1. 开始 Prefill 阶段 (Prompt 长度: {prompt_len}) ---")
    with torch.no_grad():
        _, cache = model(prompt_input, kv_cache=None)
    print(f"Prefill 完成。当前 KV Cache 形状 - K: {cache[0].shape}, V: {cache[1].shape}")
    # 期望: [1, n_heads, prompt_len, head_dim] -> [1, 4, 5, 64]

    # === 阶段二：Decode ===
    curr_input = torch.randn(batch_size, 1, d_model, device=device)
    print(f"\n--- 2. 开始 Decode 阶段 (逐字生成) ---")
    for step in range(next_steps):
        with torch.no_grad():
            _, cache = model(curr_input, kv_cache=cache)
        print(f"生成第 {step + 1} 个 Token。滚动更新后 KV Cache 形状 - K: {cache[0].shape}")
        # 真实场景：out -> Linear -> Softmax 采样 -> Embedding 查表
        curr_input = torch.randn(batch_size, 1, d_model, device=device)


if __name__ == "__main__":
    demo_autoregressive_decode()
