"""Injectable clock. Tests freeze time with FakeClock; production uses WallClock."""

from __future__ import annotations

import time


class WallClock:
    def now(self) -> float:
        return time.time()


class FakeClock:
    """Deterministic clock. `now` is a unix-ish timestamp, not 'seconds since start'."""

    def __init__(self, now: float = 0.0) -> None:
        self._now = float(now)

    def now(self) -> float:
        return self._now

    def advance(self, seconds: float) -> None:
        self._now += seconds
