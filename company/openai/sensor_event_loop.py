#!/usr/bin/env python3
"""Embedded multi-sensor event loop demo (SPSC rings + poll/sleep + time sync).

Mirrors the OpenAI Embedded Experiences design question:
  - One SPSC queue per sensor (no multi-producer CAS)
  - Single event-loop thread drains queues (Reactor)
  - Busy-poll while hot; block on Event when idle (power saving)
  - On camera frame, align nearby IMU samples by timestamp window
"""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass
from enum import Enum, auto
from typing import Generic, List, Optional, TypeVar

T = TypeVar("T")


class SpscRing(Generic[T]):
    """Power-of-two SPSC ring. One producer thread, one consumer thread."""

    def __init__(self, capacity: int) -> None:
        if capacity < 2 or (capacity & (capacity - 1)) != 0:
            raise ValueError("capacity must be power of 2 and >= 2")
        self._cap = capacity
        self._mask = capacity - 1
        self._buf: List[Optional[T]] = [None] * capacity
        self._head = 0  # consumer
        self._tail = 0  # producer
        # In C++ these are atomics with acquire/release + alignas(64).
        # CPython GIL makes the demo race-free enough for teaching.

    def push(self, item: T) -> bool:
        if ((self._tail + 1) & self._mask) == (self._head & self._mask):
            return False  # full — drop newest (caller's policy)
        self._buf[self._tail & self._mask] = item
        self._tail += 1
        return True

    def pop(self) -> Optional[T]:
        if self._head == self._tail:
            return None
        item = self._buf[self._head & self._mask]
        self._buf[self._head & self._mask] = None
        self._head += 1
        return item


class SensorType(Enum):
    IMU = auto()
    CAMERA = auto()
    AUDIO = auto()


@dataclass
class SensorEvent:
    type: SensorType
    timestamp_ms: float
    payload: object  # IMU vec / frame id / audio chunk id — zero-copy stand-in


class EventLoop:
    def __init__(self) -> None:
        self.imu_q: SpscRing[SensorEvent] = SpscRing(256)
        self.cam_q: SpscRing[SensorEvent] = SpscRing(32)
        self.audio_q: SpscRing[SensorEvent] = SpscRing(64)
        self._wake = threading.Event()
        self._running = True
        self.imu_history: List[SensorEvent] = []
        self.fused: List[dict] = []
        self.drops = {"imu": 0, "cam": 0, "audio": 0}

    def dispatch(self, event: SensorEvent) -> None:
        ok = True
        if event.type is SensorType.IMU:
            ok = self.imu_q.push(event)
            if not ok:
                self.drops["imu"] += 1
        elif event.type is SensorType.CAMERA:
            ok = self.cam_q.push(event)
            if not ok:
                self.drops["cam"] += 1
        else:
            ok = self.audio_q.push(event)
            if not ok:
                self.drops["audio"] += 1
        self._wake.set()  # wake loop if sleeping

    def stop(self) -> None:
        self._running = False
        self._wake.set()

    def run(self) -> None:
        idle_spins = 0
        while self._running:
            processed = False

            # High-priority: drain IMU first (200Hz path).
            while (ev := self.imu_q.pop()) is not None:
                self.imu_history.append(ev)
                if len(self.imu_history) > 512:
                    self.imu_history = self.imu_history[-512:]
                processed = True

            while (ev := self.cam_q.pop()) is not None:
                self._fuse_camera(ev)
                processed = True

            while (ev := self.audio_q.pop()) is not None:
                processed = True  # demo: count only

            if processed:
                idle_spins = 0
                continue

            idle_spins += 1
            if idle_spins < 3:
                # Short active poll window (low wakeup latency).
                time.sleep(0.0005)
            else:
                # Sleep until producer wakes us — power saving.
                self._wake.wait(timeout=0.05)
                self._wake.clear()
                idle_spins = 0

    def _fuse_camera(self, cam: SensorEvent) -> None:
        """Align IMU samples within ±2.5ms of the camera timestamp."""
        window_ms = 2.5
        t = cam.timestamp_ms
        matched = [
            e
            for e in self.imu_history
            if abs(e.timestamp_ms - t) <= window_ms
        ]
        self.fused.append(
            {
                "cam_t": t,
                "cam_payload": cam.payload,
                "imu_count": len(matched),
                "imu_ts": [e.timestamp_ms for e in matched],
            }
        )


def _producer(
    loop: EventLoop,
    kind: SensorType,
    hz: float,
    duration_s: float,
    payload_fn,
) -> None:
    period = 1.0 / hz
    t0 = time.monotonic()
    n = 0
    while time.monotonic() - t0 < duration_s:
        now_ms = (time.monotonic() - t0) * 1000.0
        loop.dispatch(
            SensorEvent(type=kind, timestamp_ms=now_ms, payload=payload_fn(n))
        )
        n += 1
        time.sleep(period)


def main() -> None:
    loop = EventLoop()
    thread = threading.Thread(target=loop.run, name="event-loop", daemon=True)
    thread.start()

    duration = 0.35
    producers = [
        threading.Thread(
            target=_producer,
            args=(loop, SensorType.IMU, 200.0, duration, lambda i: ("gyro", i)),
            daemon=True,
        ),
        threading.Thread(
            target=_producer,
            args=(loop, SensorType.CAMERA, 30.0, duration, lambda i: ("frame", i)),
            daemon=True,
        ),
        threading.Thread(
            target=_producer,
            args=(loop, SensorType.AUDIO, 50.0, duration, lambda i: ("pcm", i)),
            daemon=True,
        ),
    ]
    for p in producers:
        p.start()
    for p in producers:
        p.join()

    time.sleep(0.05)  # drain tail
    loop.stop()
    thread.join(timeout=1.0)

    assert loop.fused, "expected at least one fused camera+imu bundle"
    # Most camera frames should find a nearby IMU sample at 200Hz.
    with_imu = sum(1 for f in loop.fused if f["imu_count"] > 0)
    assert with_imu >= len(loop.fused) // 2

    print(f"fused_frames={len(loop.fused)} with_imu={with_imu}")
    print(f"drops={loop.drops}")
    print("sample:", loop.fused[len(loop.fused) // 2])
    print("sensor_event_loop: ok")


if __name__ == "__main__":
    main()
