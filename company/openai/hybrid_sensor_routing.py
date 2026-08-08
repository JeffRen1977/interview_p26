"""
Hybrid edge-cloud routing for large camera fleets (educational).

Interview talking points:
- 100k cameras cannot upload full video — route by confidence
- On-device 1B–3B INT4 + PagedAttention-style KV budget
- Cascade: low confidence or complex reasoning → upload embeddings (not frames)

Run
----
    cd openai
    python3 hybrid_sensor_routing.py
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import List, Optional, Tuple


class RouteDecision(Enum):
    LOCAL_CLOSE = "local_close"  # high confidence — stay on device
    CASCADE_EMBED = "cascade_embed"  # mid / complex — upload embedding
    DROP_OR_BUFFER = "drop_or_buffer"  # very low confidence


@dataclass
class EdgeObservation:
    camera_id: str
    confidence: float
    needs_reasoning: bool = False  # long OCR / multi-object / open QA
    embedding_dim: int = 1024
    frame_jpeg_bytes: int = 120_000  # ~crop size if we naively uploaded image


@dataclass
class RouteResult:
    decision: RouteDecision
    uplink_bytes: int
    cloud_invoked: bool
    reason: str


@dataclass
class HybridRouter:
    """Confidence-calibrated cascade policy."""

    theta_high: float = 0.90
    theta_low: float = 0.55
    bytes_per_embedding_dim: int = 2  # float16

    def route(self, obs: EdgeObservation) -> RouteResult:
        emb_bytes = obs.embedding_dim * self.bytes_per_embedding_dim

        if obs.needs_reasoning:
            return RouteResult(
                RouteDecision.CASCADE_EMBED,
                uplink_bytes=emb_bytes,
                cloud_invoked=True,
                reason="complex_reasoning_forced_cascade",
            )

        if obs.confidence >= self.theta_high:
            # Metadata-only optional; demo counts 256B event JSON
            return RouteResult(
                RouteDecision.LOCAL_CLOSE,
                uplink_bytes=256,
                cloud_invoked=False,
                reason="high_confidence_local",
            )

        if obs.confidence >= self.theta_low:
            return RouteResult(
                RouteDecision.CASCADE_EMBED,
                uplink_bytes=emb_bytes,
                cloud_invoked=True,
                reason="mid_confidence_cascade",
            )

        return RouteResult(
            RouteDecision.DROP_OR_BUFFER,
            uplink_bytes=0,
            cloud_invoked=False,
            reason="low_confidence_drop",
        )


@dataclass
class EdgeModelConfig:
    """Stand-in for 1B–3B INT4 + paged KV budget on device."""

    params_b: float = 2.0
    quant: str = "INT4"
    kv_blocks: int = 8
    kv_block_tokens: int = 16

    def kv_token_capacity(self) -> int:
        return self.kv_blocks * self.kv_block_tokens

    def describe(self) -> str:
        return (
            f"{self.params_b}B {self.quant}, "
            f"paged KV capacity≈{self.kv_token_capacity()} tokens "
            f"({self.kv_blocks} blocks×{self.kv_block_tokens})"
        )


@dataclass
class FleetStats:
    local: int = 0
    cascade: int = 0
    drop: int = 0
    uplink_bytes: int = 0
    cloud_calls: int = 0
    naive_video_bytes: int = 0  # if every cascade uploaded JPEG instead

    def observe(self, obs: EdgeObservation, result: RouteResult) -> None:
        if result.decision == RouteDecision.LOCAL_CLOSE:
            self.local += 1
        elif result.decision == RouteDecision.CASCADE_EMBED:
            self.cascade += 1
            self.naive_video_bytes += obs.frame_jpeg_bytes
        else:
            self.drop += 1
        self.uplink_bytes += result.uplink_bytes
        if result.cloud_invoked:
            self.cloud_calls += 1

    @property
    def total(self) -> int:
        return self.local + self.cascade + self.drop

    def cascade_rate(self) -> float:
        return self.cascade / self.total if self.total else 0.0

    def bandwidth_saving_vs_jpeg(self) -> float:
        if self.naive_video_bytes == 0:
            return 1.0
        # Only compare cascade path: embedding uplink vs jpeg uplink
        # Approximate embedding share of uplink among cascades
        return 1.0 - (self.uplink_bytes / max(self.naive_video_bytes, 1))


def simulate_fleet(
    router: HybridRouter,
    observations: List[EdgeObservation],
) -> FleetStats:
    stats = FleetStats()
    for obs in observations:
        stats.observe(obs, router.route(obs))
    return stats


def demo() -> None:
    print("=== 1. Edge model (INT4 + paged KV) ===")
    edge = EdgeModelConfig()
    print(f"  {edge.describe()}")
    assert edge.kv_token_capacity() == 128

    router = HybridRouter(theta_high=0.90, theta_low=0.55)

    print("\n=== 2. Per-frame routing decisions ===")
    cases: List[Tuple[str, EdgeObservation]] = [
        ("clear_intrusion", EdgeObservation("cam-1", 0.96)),
        ("blurry_person", EdgeObservation("cam-2", 0.72)),
        ("noise", EdgeObservation("cam-3", 0.30)),
        ("long_ocr_sign", EdgeObservation("cam-4", 0.93, needs_reasoning=True)),
    ]
    for name, obs in cases:
        r = router.route(obs)
        print(
            f"  {name:16s} C={obs.confidence:.2f} "
            f"→ {r.decision.value:16s} uplink={r.uplink_bytes}B ({r.reason})"
        )

    assert router.route(cases[0][1]).decision == RouteDecision.LOCAL_CLOSE
    assert router.route(cases[1][1]).decision == RouteDecision.CASCADE_EMBED
    assert router.route(cases[2][1]).decision == RouteDecision.DROP_OR_BUFFER
    assert router.route(cases[3][1]).cloud_invoked is True  # forced despite high C

    print("\n=== 3. Fleet mix (10k frames toy) ===")
    # 80% high conf, 12% mid, 6% low, 2% complex
    obs_list: List[EdgeObservation] = []
    for i in range(8000):
        obs_list.append(EdgeObservation(f"c{i}", 0.95))
    for i in range(1200):
        obs_list.append(EdgeObservation(f"m{i}", 0.70))
    for i in range(600):
        obs_list.append(EdgeObservation(f"l{i}", 0.20))
    for i in range(200):
        obs_list.append(EdgeObservation(f"r{i}", 0.92, needs_reasoning=True))

    stats = simulate_fleet(router, obs_list)
    print(
        f"  local={stats.local} cascade={stats.cascade} drop={stats.drop} "
        f"cascade_rate={stats.cascade_rate():.1%}"
    )
    print(
        f"  embedding uplink={stats.uplink_bytes/1e6:.2f}MB; "
        f"if JPEG instead≈{stats.naive_video_bytes/1e6:.1f}MB"
    )
    assert stats.cascade_rate() < 0.20
    assert stats.uplink_bytes < stats.naive_video_bytes

    print("\n=== 4. Scale sketch: 100k cameras ===")
    # Assume 15 fps effective decisions/s with same mix cascade_rate
    cams = 100_000
    fps = 15
    cascade_qps = cams * fps * stats.cascade_rate()
    print(f"  ~{cascade_qps:,.0f} embedding/s to cloud at this cascade rate")
    print("  → admit with tenant quotas; never default-upload 1080p video")

    print("\nhybrid_sensor_routing: all demos passed.")


if __name__ == "__main__":
    demo()
