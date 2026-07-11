"""
PagedAttention — vLLM 风格离散物理块 KV 缓存与页表寻址 Demo。

核心思想
--------
KV Cache 按固定大小 Block 切分，逻辑连续、物理离散。
BlockManager 维护全局物理显存池；每个请求持有一张 block_table（逻辑块 -> 物理块 ID）。
写入时按需 allocate，Attention 时按页表寻址组合计算，释放时回收物理块。

运行
----
    cd LLM
    python3 paged_attention.py
"""

from __future__ import annotations

from typing import List

import torch
import torch.nn as nn
import torch.nn.functional as F


# ==========================================
# 1. 全局物理显存池与中央管理器 (Block Manager)
# ==========================================
class BlockManager:
    def __init__(self, num_blocks: int):
        self.num_blocks = num_blocks
        self.free_blocks: List[int] = list(range(num_blocks))
        self.ref_counts = {block_id: 0 for block_id in range(num_blocks)}

    def allocate_block(self) -> int:
        """物理显存分配：从空闲列表中弹出一个不连续的物理 Block ID。"""
        if not self.free_blocks:
            raise MemoryError("GPU HBM Out of Memory! 全局物理显存池已耗尽。")
        block_id = self.free_blocks.pop(0)
        self.ref_counts[block_id] = 1
        return block_id

    def free_block(self, block_id: int) -> None:
        """物理显存回收：将不再使用的物理 Block ID 放回空闲列表。"""
        if self.ref_counts[block_id] > 0:
            self.ref_counts[block_id] -= 1
            if self.ref_counts[block_id] == 0:
                self.free_blocks.append(block_id)
                self.free_blocks.sort()

    def get_num_free_blocks(self) -> int:
        return len(self.free_blocks)


# ==========================================
# 2. 具备动态分配功能的推理引擎
# ==========================================
class AdvancedPagedAttentionEngine(nn.Module):
    def __init__(self, block_manager: BlockManager, block_size: int,
                 num_heads: int, head_dim: int):
        super().__init__()
        self.block_manager = block_manager
        self.block_size = block_size
        self.num_heads = num_heads
        self.head_dim = head_dim

        # 静态分配全局 GPU HBM 显存池
        # [总Block数, 2(K/V), num_heads, block_size, head_dim]
        self.gpu_kv_buffer = torch.zeros(
            (block_manager.num_blocks, 2, num_heads, block_size, head_dim)
        )

    def append_and_manage_kv(
        self,
        k: torch.Tensor,
        v: torch.Tensor,
        block_table: List[int],
        global_seq_len: int,
        verbose: bool = True,
    ) -> None:
        """
        核心动态写入：检查是否需要新物理块，并离散写入 K/V。
        k/v: [batch, num_heads, 1, head_dim]
        """
        if global_seq_len % self.block_size == 0:
            new_physical_id = self.block_manager.allocate_block()
            block_table.append(new_physical_id)
            if verbose:
                print(
                    f"  [显存动态分配] 触发新块申请！指派物理 Block ID: {new_physical_id}. "
                    f"当前页表更新为: {block_table}"
                )

        logical_block_idx = global_seq_len // self.block_size
        block_offset = global_seq_len % self.block_size
        physical_block_id = block_table[logical_block_idx]

        # 原地离散写入（Zero Copy）
        self.gpu_kv_buffer[physical_block_id, 0, :, block_offset, :] = k[0, :, 0, :]
        self.gpu_kv_buffer[physical_block_id, 1, :, block_offset, :] = v[0, :, 0, :]

    def paged_attention_forward(
        self,
        q: torch.Tensor,
        block_table: List[int],
        current_seq_len: int,
    ) -> torch.Tensor:
        """根据离散 block_table 执行组合 Attention 计算。"""
        _, num_heads, _, head_dim = q.shape
        q_s = q.squeeze(0).squeeze(1)  # [num_heads, head_dim]

        global_context = torch.zeros((num_heads, 1, head_dim), device=q.device, dtype=q.dtype)
        all_token_logits: List[List[torch.Tensor]] = [[] for _ in range(num_heads)]

        # 步骤 A: 物理不连续寻址，计算 Logits
        token_counter = 0
        for physical_block_id in block_table:
            rem_tokens = current_seq_len - token_counter
            actual_tokens_in_block = min(self.block_size, rem_tokens)
            if actual_tokens_in_block <= 0:
                break

            block_k = self.gpu_kv_buffer[
                physical_block_id, 0, :, :actual_tokens_in_block, :
            ]

            for h in range(num_heads):
                block_logits = torch.matmul(
                    q_s[h].unsqueeze(0), block_k[h].transpose(-2, -1)
                ) / (head_dim ** 0.5)
                all_token_logits[h].append(block_logits.squeeze(0))
            token_counter += actual_tokens_in_block

        # 步骤 B: 跨离散块的全局 Softmax 与 Value 加权聚合
        token_counter = 0
        for physical_block_id in block_table:
            rem_tokens = current_seq_len - token_counter
            actual_tokens_in_block = min(self.block_size, rem_tokens)
            if actual_tokens_in_block <= 0:
                break

            block_v = self.gpu_kv_buffer[
                physical_block_id, 1, :, :actual_tokens_in_block, :
            ]

            for h in range(num_heads):
                full_logits_this_head = torch.cat(all_token_logits[h])
                attn_weights_this_head = F.softmax(full_logits_this_head, dim=-1)
                current_block_weights = attn_weights_this_head[
                    token_counter : token_counter + actual_tokens_in_block
                ]
                global_context[h] += torch.matmul(
                    current_block_weights.unsqueeze(0), block_v[h]
                )
            token_counter += actual_tokens_in_block

        return global_context.unsqueeze(0)  # [1, num_heads, 1, head_dim]


def demo() -> None:
    torch.manual_seed(0)

    # 全局共有 5 个物理 Blocks，单块存 2 个 Token；2 个头，维度 64
    global_block_manager = BlockManager(num_blocks=5)
    engine = AdvancedPagedAttentionEngine(
        global_block_manager, block_size=2, num_heads=2, head_dim=64
    )

    print(
        f"系统初始空闲物理块数量: {global_block_manager.get_num_free_blocks()} 个, "
        f"列表: {global_block_manager.free_blocks}"
    )

    # 场景一：用户 A 自回归生成 3 个 Token
    print("\n=== 场景一：用户 A 请求进入 ===")
    user_a_block_table: List[int] = []
    user_a_seq_len = 0

    for step in range(3):
        print(f"用户 A 生成第 {step + 1} 个 Token:")
        q = torch.randn(1, 2, 1, 64)
        k = torch.randn(1, 2, 1, 64)
        v = torch.randn(1, 2, 1, 64)

        engine.append_and_manage_kv(k, v, user_a_block_table, global_seq_len=user_a_seq_len)
        user_a_seq_len += 1
        out = engine.paged_attention_forward(q, user_a_block_table, current_seq_len=user_a_seq_len)
        assert out.shape == (1, 2, 1, 64)

    print(f"用户 A 结束。最终占用物理页表: {user_a_block_table}")
    print(
        f"此时系统剩余空闲物理块数量: {global_block_manager.get_num_free_blocks()} 个, "
        f"列表: {global_block_manager.free_blocks}"
    )
    assert len(user_a_block_table) == 2
    assert global_block_manager.get_num_free_blocks() == 3

    # 场景二：用户 B 并发进入，生成 2 个 Token
    print("\n=== 场景二：用户 B 并发请求进入 ===")
    user_b_block_table: List[int] = []
    user_b_seq_len = 0

    for step in range(2):
        print(f"用户 B 生成第 {step + 1} 个 Token:")
        q = torch.randn(1, 2, 1, 64)
        k = torch.randn(1, 2, 1, 64)
        v = torch.randn(1, 2, 1, 64)
        engine.append_and_manage_kv(k, v, user_b_block_table, global_seq_len=user_b_seq_len)
        user_b_seq_len += 1
        engine.paged_attention_forward(q, user_b_block_table, current_seq_len=user_b_seq_len)

    print(f"用户 B 结束。最终占用物理页表: {user_b_block_table}")
    print(f"此时系统剩余空闲物理块数量: {global_block_manager.get_num_free_blocks()} 个")
    assert len(user_b_block_table) == 1
    assert global_block_manager.get_num_free_blocks() == 2

    # 场景三：用户 A 结束，显存回收
    print("\n=== 场景三：用户 A 彻底结束，触发显存动态回收 ===")
    print(f"回收前系统空闲列表: {global_block_manager.free_blocks}")

    for block_id in user_a_block_table:
        global_block_manager.free_block(block_id)
    user_a_block_table = []

    print(f"回收后系统空闲列表: {global_block_manager.free_blocks}")
    print(
        f"此时系统剩余空闲物理块数量: {global_block_manager.get_num_free_blocks()} 个"
        f"（用户 A 释放的块已能无缝被新请求复用）"
    )
    assert global_block_manager.get_num_free_blocks() == 4
    print("\nPagedAttention demo passed.")


if __name__ == "__main__":
    demo()
