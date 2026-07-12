"""
Streaming ChatGPT backend demo — SSE-style token push + barge-in cancel.

Interview talking points:
- Flush each token immediately (TTFT); do not buffer the full reply.
- Distinguish RPS (one stream request) vs TPS (tokens emitted).
- Cancel stops generation and releases "GPU" work (here: a worker thread).

Run
----
    cd openai
    python3 streaming_chat_demo.py
"""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from enum import Enum
from typing import Callable, Dict, Iterator, List, Optional


class StreamState(Enum):
    RUNNING = "running"
    CANCELLED = "cancelled"
    DONE = "done"


@dataclass
class Generation:
    generation_id: str
    tokens: List[str]
    state: StreamState = StreamState.RUNNING
    commit_cursor: int = -1
    emitted: List[str] = field(default_factory=list)
    _cancel: threading.Event = field(default_factory=threading.Event)

    def cancel(self, commit_cursor: Optional[int] = None) -> None:
        if commit_cursor is not None:
            self.commit_cursor = commit_cursor
        self._cancel.set()
        self.state = StreamState.CANCELLED

    @property
    def cancelled(self) -> bool:
        return self._cancel.is_set()


class InferenceWorker:
    """Simulates GPU decode: emits one token every `token_interval_s`."""

    def __init__(self, token_interval_s: float = 0.05):
        self.token_interval_s = token_interval_s

    def stream(self, gen: Generation) -> Iterator[str]:
        for i, tok in enumerate(gen.tokens):
            if gen.cancelled:
                return
            time.sleep(self.token_interval_s)
            if gen.cancelled:
                return
            gen.emitted.append(tok)
            gen.commit_cursor = i
            yield tok
        gen.state = StreamState.DONE


class StreamGateway:
    """
    Connection plane: fans tokens out as SSE-like events.
    On client disconnect / Stop, cancel the generation (save "GPU").
    """

    def __init__(self, worker: InferenceWorker):
        self.worker = worker
        self._gens: Dict[str, Generation] = {}
        self._lock = threading.Lock()

    def start_chat(
        self,
        generation_id: str,
        prompt_tokens: List[str],
        on_event: Callable[[str], None],
    ) -> Generation:
        gen = Generation(generation_id=generation_id, tokens=prompt_tokens)
        with self._lock:
            self._gens[generation_id] = gen

        def _run() -> None:
            # First byte / headers would already be flushed to client here.
            for tok in self.worker.stream(gen):
                # Immediate flush equivalent — no full-reply buffering.
                on_event(f"data: {{\"type\":\"token\",\"text\":{tok!r}}}\n\n")
            if gen.state == StreamState.CANCELLED:
                on_event('data: {"type":"cancelled"}\n\n')
            else:
                on_event(
                    f'data: {{"type":"done","completion_tokens":{len(gen.emitted)}}}\n\n'
                )
            with self._lock:
                self._gens.pop(generation_id, None)

        threading.Thread(target=_run, daemon=True).start()
        return gen

    def barge_in(self, generation_id: str, commit_cursor: Optional[int] = None) -> bool:
        with self._lock:
            gen = self._gens.get(generation_id)
        if gen is None:
            return False
        gen.cancel(commit_cursor=commit_cursor)
        return True


def demo() -> None:
    worker = InferenceWorker(token_interval_s=0.03)
    gateway = StreamGateway(worker)

    reply = ["The", " quick", " brown", " fox", " jumps", " over", " the", " lazy", " dog", "."]
    events: List[str] = []

    print("=== Scene 1: full stream (no cancel) ===")
    gen = gateway.start_chat("gen-1", reply, events.append)
    while gen.state == StreamState.RUNNING:
        time.sleep(0.02)
    time.sleep(0.05)
    print(f"  tokens={len(gen.emitted)} state={gen.state.value}")
    assert gen.state == StreamState.DONE
    assert len(gen.emitted) == len(reply)

    print("\n=== Scene 2: barge-in after ~3 tokens ===")
    events.clear()
    gen2 = gateway.start_chat("gen-2", reply, events.append)
    time.sleep(0.12)  # ~3–4 tokens at 30ms
    ok = gateway.barge_in("gen-2", commit_cursor=gen2.commit_cursor)
    assert ok
    while gen2.generation_id in gateway._gens:  # noqa: SLF001 — demo wait
        time.sleep(0.02)
    time.sleep(0.05)
    print(f"  emitted={gen2.emitted!r}")
    print(f"  state={gen2.state.value} commit_cursor={gen2.commit_cursor}")
    assert gen2.state == StreamState.CANCELLED
    assert len(gen2.emitted) < len(reply)

    print("\n=== Metrics reminder ===")
    print("  RPS: 2 chat requests in this demo")
    print(f"  TPS (scene1): {len(reply)} tokens over one stream")
    print("streaming_chat_demo passed.")


if __name__ == "__main__":
    demo()
