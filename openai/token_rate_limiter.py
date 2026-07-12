"""
Multi-tenant LLM rate limiter: dual Token Buckets (RPM + TPM).

Covers interview topics:
- Dynamic-weight TPM consume (estimate tokens, then reconcile)
- Local shard cache + async flush to a shared store (mock Redis)

Run
----
    cd openai
    python3 token_rate_limiter.py
"""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from typing import Dict, Optional, Tuple


# ---------------------------------------------------------------------------
# Core: Token Bucket
# ---------------------------------------------------------------------------

@dataclass
class TokenBucket:
    capacity: float
    refill_per_sec: float
    tokens: float = field(init=False)
    last_refill: float = field(default_factory=time.monotonic)

    def __post_init__(self) -> None:
        self.tokens = self.capacity

    def _refill(self, now: Optional[float] = None) -> None:
        now = time.monotonic() if now is None else now
        elapsed = now - self.last_refill
        if elapsed <= 0:
            return
        self.tokens = min(self.capacity, self.tokens + elapsed * self.refill_per_sec)
        self.last_refill = now

    def try_consume(self, amount: float = 1.0) -> bool:
        self._refill()
        if self.tokens >= amount:
            self.tokens -= amount
            return True
        return False

    def refund(self, amount: float) -> None:
        self._refill()
        self.tokens = min(self.capacity, self.tokens + amount)


# ---------------------------------------------------------------------------
# Single-process dual limiter (authoritative algorithm)
# ---------------------------------------------------------------------------

@dataclass
class TenantLimiter:
    """RPM bucket cost=1; TPM bucket cost=dynamic token weight."""

    rpm_limit: int
    tpm_limit: int
    rpm_bucket: TokenBucket = field(init=False)
    tpm_bucket: TokenBucket = field(init=False)

    def __post_init__(self) -> None:
        self.rpm_bucket = TokenBucket(
            capacity=float(self.rpm_limit),
            refill_per_sec=self.rpm_limit / 60.0,
        )
        self.tpm_bucket = TokenBucket(
            capacity=float(self.tpm_limit),
            refill_per_sec=self.tpm_limit / 60.0,
        )

    def allow(self, estimated_tokens: int) -> Tuple[bool, str]:
        if estimated_tokens < 0:
            raise ValueError("estimated_tokens must be non-negative")
        if not self.rpm_bucket.try_consume(1.0):
            return False, "rpm_exceeded"
        if not self.tpm_bucket.try_consume(float(estimated_tokens)):
            self.rpm_bucket.refund(1.0)
            return False, "tpm_exceeded"
        return True, "ok"

    def reconcile(self, estimated_tokens: int, actual_tokens: int) -> None:
        """After generation: refund over-estimate or charge under-estimate."""
        delta = estimated_tokens - actual_tokens
        if delta > 0:
            self.tpm_bucket.refund(float(delta))
        elif delta < 0:
            # Best-effort; may fail silently and rely on next allow() to throttle.
            self.tpm_bucket.try_consume(float(-delta))


# ---------------------------------------------------------------------------
# Distributed path: mock Redis + local shard + async flush
# ---------------------------------------------------------------------------

class MockRedisStore:
    """Process-wide authoritative counters (stand-in for Redis Cluster + Lua)."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        # tenant -> TenantLimiter (global truth)
        self._tenants: Dict[str, TenantLimiter] = {}

    def get_or_create(self, tenant: str, rpm: int, tpm: int) -> TenantLimiter:
        with self._lock:
            if tenant not in self._tenants:
                self._tenants[tenant] = TenantLimiter(rpm_limit=rpm, tpm_limit=tpm)
            return self._tenants[tenant]

    def flush_consume(self, tenant: str, rpm_delta: float, tpm_delta: float) -> bool:
        """Atomically apply batched local usage to global buckets."""
        with self._lock:
            lim = self._tenants[tenant]
            # Peek without consuming for deny-all if insufficient.
            lim.rpm_bucket._refill()
            lim.tpm_bucket._refill()
            if lim.rpm_bucket.tokens < rpm_delta or lim.tpm_bucket.tokens < tpm_delta:
                return False
            assert lim.rpm_bucket.try_consume(rpm_delta)
            assert lim.tpm_bucket.try_consume(tpm_delta)
            return True


@dataclass
class LocalShardLimiter:
    """
    Gateway-local quota slice.

    Hot path: memory only.
    Background: flush pending usage to MockRedisStore and refill local slice.
    """

    tenant: str
    global_rpm: int
    global_tpm: int
    store: MockRedisStore
    num_gateways: int = 4
    safety_factor: float = 0.85
    flush_interval_s: float = 0.05

    def __post_init__(self) -> None:
        self.store.get_or_create(self.tenant, self.global_rpm, self.global_tpm)
        slice_rpm = max(1.0, (self.global_rpm / self.num_gateways) * self.safety_factor)
        slice_tpm = max(1.0, (self.global_tpm / self.num_gateways) * self.safety_factor)
        self.local = TenantLimiter(rpm_limit=int(slice_rpm), tpm_limit=int(slice_tpm))
        # Override capacities to fractional-friendly floats
        self.local.rpm_bucket.capacity = slice_rpm
        self.local.rpm_bucket.tokens = slice_rpm
        self.local.rpm_bucket.refill_per_sec = slice_rpm / 60.0
        self.local.tpm_bucket.capacity = slice_tpm
        self.local.tpm_bucket.tokens = slice_tpm
        self.local.tpm_bucket.refill_per_sec = slice_tpm / 60.0

        self._pending_rpm = 0.0
        self._pending_tpm = 0.0
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._flusher = threading.Thread(target=self._flush_loop, daemon=True)
        self._flusher.start()

    def allow(self, estimated_tokens: int) -> Tuple[bool, str]:
        with self._lock:
            ok, reason = self.local.allow(estimated_tokens)
            if ok:
                self._pending_rpm += 1.0
                self._pending_tpm += float(estimated_tokens)
            return ok, reason

    def reconcile(self, estimated_tokens: int, actual_tokens: int) -> None:
        with self._lock:
            self.local.reconcile(estimated_tokens, actual_tokens)
            self._pending_tpm += float(actual_tokens - estimated_tokens)

    def _flush_loop(self) -> None:
        while not self._stop.wait(self.flush_interval_s):
            self.flush_once()

    def flush_once(self) -> bool:
        with self._lock:
            rpm_d, tpm_d = self._pending_rpm, self._pending_tpm
            self._pending_rpm = 0.0
            self._pending_tpm = 0.0
        if rpm_d == 0 and tpm_d == 0:
            return True
        # Only flush positive consumption; negative reconcile is local refund.
        rpm_d = max(0.0, rpm_d)
        tpm_d = max(0.0, tpm_d)
        if rpm_d == 0 and tpm_d == 0:
            return True
        ok = self.store.flush_consume(self.tenant, rpm_d, tpm_d)
        if not ok:
            # Global exhausted: empty local buckets (cooldown).
            with self._lock:
                self.local.rpm_bucket.tokens = 0.0
                self.local.tpm_bucket.tokens = 0.0
        return ok

    def stop(self) -> None:
        self._stop.set()
        self._flusher.join(timeout=1.0)
        self.flush_once()


# ---------------------------------------------------------------------------
# Demos
# ---------------------------------------------------------------------------

def demo_dual_bucket() -> None:
    limiter = TenantLimiter(rpm_limit=3, tpm_limit=100)
    requests = [
        ("short", 20),
        ("short", 25),
        ("short", 30),
        ("long", 80),
        ("long", 50),
    ]
    print("=== Dual Token Bucket (RPM=3, TPM=100/min) ===")
    for i, (label, est) in enumerate(requests, 1):
        ok, reason = limiter.allow(est)
        status = "ALLOW" if ok else f"DENY ({reason})"
        print(f"  req {i} [{label:5s}] est={est:3d} -> {status}")
        if ok:
            # Simulate completion cheaper than estimate for first two.
            actual = est - 5 if i <= 2 else est
            limiter.reconcile(est, actual)
    assert limiter.allow(10)[0] is False
    print("dual-bucket demo ok")


def demo_local_shard_async_flush() -> None:
    store = MockRedisStore()
    # Global: RPM=20, TPM=500; 4 gateways → each ~4.25 RPM, ~106 TPM local
    g1 = LocalShardLimiter("acme", 20, 500, store, num_gateways=4)
    g2 = LocalShardLimiter("acme", 20, 500, store, num_gateways=4)

    print("\n=== Local shard + async flush (2 gateways) ===")
    allowed = 0
    for _ in range(8):
        ok, _ = g1.allow(40)
        if ok:
            allowed += 1
            g1.reconcile(40, 40)
        ok2, _ = g2.allow(40)
        if ok2:
            allowed += 1
            g2.reconcile(40, 40)
    time.sleep(0.2)  # let flush run
    g1.flush_once()
    g2.flush_once()
    print(f"  local allows across 2 GWs: {allowed}")
    global_lim = store.get_or_create("acme", 20, 500)
    print(
        f"  global remaining ~ rpm={global_lim.rpm_bucket.tokens:.1f} "
        f"tpm={global_lim.tpm_bucket.tokens:.1f}"
    )
    g1.stop()
    g2.stop()
    print("local-shard demo ok")


def demo() -> None:
    demo_dual_bucket()
    demo_local_shard_async_flush()
    print("\ntoken_rate_limiter: all demos passed.")


if __name__ == "__main__":
    demo()
