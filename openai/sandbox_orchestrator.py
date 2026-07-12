"""
Isolated code-execution sandbox orchestrator (educational simulation).

Interview talking points:
- Prefer MicroVM (Firecracker/Kata) over Docker for untrusted multi-tenant code.
- <100ms ready: warm pool first, snapshot restore fallback.
- cgroups-like caps + wall-clock timeout; default-deny egress allowlist.

This does NOT spawn real VMs/containers — it models the control plane.

Run
----
    cd openai
    python3 sandbox_orchestrator.py
"""

from __future__ import annotations

import time
import uuid
from dataclasses import dataclass, field
from enum import Enum
from typing import Dict, List, Optional, Set


class IsolationBackend(Enum):
    DOCKER = "docker"  # shared host kernel — NOT for untrusted multi-tenant
    MICROVM = "microvm"  # Firecracker / Kata — hardware isolation


class VmState(Enum):
    COLD = "cold"
    SNAPSHOT = "snapshot"
    READY = "ready"  # in warm pool
    BUSY = "busy"
    DEAD = "dead"


@dataclass
class ResourceLimits:
    cpu_cores: float = 1.0
    memory_mb: int = 512
    pids_max: int = 64
    timeout_s: float = 0.05  # short for demo
    disk_mb: int = 256


@dataclass
class NetworkPolicy:
    default_deny: bool = True
    allowlist: Set[str] = field(default_factory=set)

    def check_egress(self, host: str) -> bool:
        if not self.default_deny:
            return True
        return host in self.allowlist


@dataclass
class MicroVM:
    vm_id: str
    backend: IsolationBackend
    state: VmState
    limits: ResourceLimits
    network: NetworkPolicy
    image: str = "python-ada:snapshot-v1"
    restored_from_snapshot: bool = False


@dataclass
class JobResult:
    job_id: str
    ok: bool
    exit_reason: str
    ready_latency_ms: float
    egress_blocked: List[str] = field(default_factory=list)
    stdout: str = ""


class SandboxOrchestrator:
    """
    Control plane:
      acquire (warm | snapshot) → inject → enforce limits → destroy → replenish
    """

    def __init__(
        self,
        warm_pool_target: int = 3,
        snapshot_restore_ms: float = 8.0,
        cold_boot_ms: float = 400.0,
    ):
        self.warm_pool_target = warm_pool_target
        self.snapshot_restore_ms = snapshot_restore_ms
        self.cold_boot_ms = cold_boot_ms
        self.pool: List[MicroVM] = []
        self._tenant_inflight: Dict[str, int] = {}
        self.max_per_tenant = 2
        self._replenish_warm_pool()

    def _new_ready_vm(self, from_snapshot: bool = False) -> MicroVM:
        return MicroVM(
            vm_id=str(uuid.uuid4())[:8],
            backend=IsolationBackend.MICROVM,
            state=VmState.READY,
            limits=ResourceLimits(),
            network=NetworkPolicy(
                default_deny=True,
                allowlist={"pypi.internal.example"},
            ),
            restored_from_snapshot=from_snapshot,
        )

    def _replenish_warm_pool(self) -> None:
        while len(self.pool) < self.warm_pool_target:
            self.pool.append(self._new_ready_vm(from_snapshot=False))

    def _acquire_vm(self) -> tuple[MicroVM, float, str]:
        """Returns (vm, ready_latency_ms, path)."""
        if self.pool:
            vm = self.pool.pop(0)
            vm.state = VmState.BUSY
            # Warm hit: already booted — sub-10ms bookkeeping.
            return vm, 2.0, "warm_pool"

        # Fallback: simulate Firecracker snapshot restore toward <100ms.
        t0 = time.perf_counter()
        time.sleep(self.snapshot_restore_ms / 1000.0)
        vm = self._new_ready_vm(from_snapshot=True)
        vm.state = VmState.BUSY
        latency = (time.perf_counter() - t0) * 1000.0
        return vm, latency, "snapshot_restore"

    def _destroy(self, vm: MicroVM) -> None:
        vm.state = VmState.DEAD
        # One-job-one-destroy: never return dirty VM to pool.
        self._replenish_warm_pool()

    def admit(self, tenant_id: str) -> bool:
        n = self._tenant_inflight.get(tenant_id, 0)
        if n >= self.max_per_tenant:
            return False
        self._tenant_inflight[tenant_id] = n + 1
        return True

    def _release_tenant(self, tenant_id: str) -> None:
        self._tenant_inflight[tenant_id] = max(
            0, self._tenant_inflight.get(tenant_id, 0) - 1
        )

    def run_job(
        self,
        tenant_id: str,
        code: str,
        egress_hosts: Optional[List[str]] = None,
        burn_cpu_s: float = 0.0,
    ) -> JobResult:
        job_id = str(uuid.uuid4())[:8]
        if not self.admit(tenant_id):
            return JobResult(job_id, False, "tenant_concurrency_exceeded", 0.0)

        try:
            if IsolationBackend.DOCKER.value == "docker":
                # Explicit teaching note in control flow comments:
                # Docker shares host kernel — rejected for this threat model.
                pass

            vm, ready_ms, path = self._acquire_vm()
            assert vm.backend == IsolationBackend.MICROVM

            blocked: List[str] = []
            for host in egress_hosts or []:
                if not vm.network.check_egress(host):
                    blocked.append(host)

            # Wall-clock watchdog (cgroups alone do not cap infinite logical work).
            started = time.perf_counter()
            time.sleep(min(burn_cpu_s, vm.limits.timeout_s + 0.02))
            elapsed = time.perf_counter() - started

            if burn_cpu_s > vm.limits.timeout_s:
                self._destroy(vm)
                return JobResult(
                    job_id,
                    False,
                    "timeout_kill",
                    ready_ms,
                    egress_blocked=blocked,
                    stdout=f"path={path}",
                )

            if blocked and any(h not in vm.network.allowlist for h in (egress_hosts or [])):
                # Default-deny: job may still run offline; report blocks.
                pass

            # Simulate success under memory/cpu caps (enforced by host cgroup in prod).
            if "ALLOCATE_HUGE" in code and vm.limits.memory_mb <= 512:
                self._destroy(vm)
                return JobResult(
                    job_id, False, "oom_kill", ready_ms, blocked, stdout=f"path={path}"
                )

            self._destroy(vm)
            return JobResult(
                job_id,
                True,
                "ok",
                ready_ms,
                egress_blocked=blocked,
                stdout=f"path={path} elapsed={elapsed:.3f}s",
            )
        finally:
            self._release_tenant(tenant_id)


def why_not_docker() -> str:
    return (
        "Docker/runc shares the host kernel; a kernel exploit escapes to host/neighbors. "
        "MicroVMs (Firecracker/Kata) give each tenant a guest kernel behind KVM."
    )


def demo() -> None:
    print("=== Isolation choice ===")
    print(f"  {why_not_docker()}")

    orch = SandboxOrchestrator(warm_pool_target=2, snapshot_restore_ms=8.0)

    print("\n=== 1. Warm pool hit (<100ms ready) ===")
    r1 = orch.run_job("tenant-a", code="print(1)")
    print(f"  ok={r1.ok} reason={r1.exit_reason} ready_ms={r1.ready_latency_ms:.1f} {r1.stdout}")
    assert r1.ok and r1.ready_latency_ms < 100

    print("\n=== 2. Drain pool → snapshot restore fallback ===")
    # Consume remaining warm VMs then force snapshot path.
    orch.pool.clear()
    r2 = orch.run_job("tenant-a", code="print(2)")
    print(f"  ok={r2.ok} ready_ms={r2.ready_latency_ms:.1f} {r2.stdout}")
    assert r2.ok and r2.ready_latency_ms < 100
    assert "snapshot_restore" in r2.stdout

    print("\n=== 3. Default-deny egress ===")
    r3 = orch.run_job(
        "tenant-b",
        code="fetch",
        egress_hosts=["evil.botnet.example", "pypi.internal.example"],
    )
    print(f"  blocked={r3.egress_blocked}")
    assert "evil.botnet.example" in r3.egress_blocked
    assert "pypi.internal.example" not in r3.egress_blocked

    print("\n=== 4. Wall-clock timeout (cgroup companion) ===")
    r4 = orch.run_job("tenant-b", code="while True: pass", burn_cpu_s=1.0)
    print(f"  ok={r4.ok} reason={r4.exit_reason}")
    assert r4.exit_reason == "timeout_kill"

    print("\n=== 5. Memory policy trip ===")
    r5 = orch.run_job("tenant-c", code="ALLOCATE_HUGE")
    print(f"  ok={r5.ok} reason={r5.exit_reason}")
    assert r5.exit_reason == "oom_kill"

    print("\n=== 6. Per-tenant concurrency cap ===")
    orch.max_per_tenant = 1
    # Hold inflight by using admit without completing — use parallel cap check:
    assert orch.admit("tenant-x")
    r6 = orch.run_job("tenant-x", code="print(3)")
    print(f"  second job while inflight: ok={r6.ok} reason={r6.exit_reason}")
    assert r6.exit_reason == "tenant_concurrency_exceeded"
    orch._release_tenant("tenant-x")

    print("\nsandbox_orchestrator: all demos passed.")


if __name__ == "__main__":
    demo()
