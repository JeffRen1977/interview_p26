"""
Real-time voice assistant pipeline (educational simulation).

Interview talking points:
- <300ms mouth-to-ear budget with streaming ASR∥LLM∥TTS overlap
- WebRTC/UDP + FEC instead of TCP for lossy mobile networks
- Barge-in: HW AEC → VAD → local stop first → cloud context rollback
- dma-buf style zero-copy between capture, NPU, and CPU

Run
----
    cd openai
    python3 voice_assistant_pipeline.py
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional


# ---------------------------------------------------------------------------
# Latency budget
# ---------------------------------------------------------------------------

@dataclass
class LatencyBudget:
    device_aec_vad_encode_ms: float = 25
    uplink_ms: float = 40
    streaming_asr_ms: float = 60
    llm_first_token_ms: float = 80
    streaming_tts_first_ms: float = 50
    downlink_jitter_ms: float = 35

    def total_ms(self) -> float:
        return (
            self.device_aec_vad_encode_ms
            + self.uplink_ms
            + self.streaming_asr_ms
            + self.llm_first_token_ms
            + self.streaming_tts_first_ms
            + self.downlink_jitter_ms
        )

    def ok(self, limit_ms: float = 300.0) -> bool:
        return self.total_ms() <= limit_ms


# ---------------------------------------------------------------------------
# FEC over UDP-like packets (toy XOR parity)
# ---------------------------------------------------------------------------

@dataclass
class AudioPacket:
    seq: int
    payload: bytes


def fec_encode(packets: List[AudioPacket]) -> AudioPacket:
    """One parity packet over the group (simplified FEC)."""
    parity = bytearray(packets[0].payload)
    for p in packets[1:]:
        for i, b in enumerate(p.payload):
            parity[i] ^= b
    return AudioPacket(seq=-1, payload=bytes(parity))


def fec_recover(
    received: Dict[int, AudioPacket],
    missing_seq: int,
    group_seqs: List[int],
    parity: AudioPacket,
) -> Optional[AudioPacket]:
    """Recover one missing packet if the rest + parity are present."""
    if missing_seq not in group_seqs:
        return None
    others = [received[s] for s in group_seqs if s != missing_seq and s in received]
    if len(others) != len(group_seqs) - 1:
        return None
    acc = bytearray(parity.payload)
    for p in others:
        for i, b in enumerate(p.payload):
            acc[i] ^= b
    return AudioPacket(seq=missing_seq, payload=bytes(acc))


# ---------------------------------------------------------------------------
# AEC + VAD + Barge-in + context rollback
# ---------------------------------------------------------------------------

@dataclass
class ConversationState:
    turns: List[str] = field(default_factory=list)
    generation_id: str = "g0"
    commit_cursor: int = -1  # last TTS chunk index played/acked
    assistant_draft_chunks: List[str] = field(default_factory=list)


class DeviceAudioFrontEnd:
    """HW AEC then VAD — echo must not trigger barge-in."""

    def __init__(self) -> None:
        self.playing = False
        self.aec_converged = True

    def on_playback_start(self) -> None:
        self.playing = True

    def on_playback_stop(self) -> None:
        self.playing = False

    def vad_speech_onset(self, mic_energy: float, is_echo_dominated: bool) -> bool:
        if is_echo_dominated and self.playing:
            # Without AEC this would false-trigger; with AEC, echo removed.
            if not self.aec_converged:
                return False
            return False  # AEC cancelled echo → not user speech
        return mic_energy > 0.5


class VoiceSession:
    def __init__(self) -> None:
        self.device = DeviceAudioFrontEnd()
        self.state = ConversationState()
        self.jitter_buffer: List[str] = []
        self.cloud_generating = False

    def start_assistant_reply(self, chunks: List[str], generation_id: str) -> None:
        self.state.generation_id = generation_id
        self.state.assistant_draft_chunks = list(chunks)
        self.state.commit_cursor = -1
        self.cloud_generating = True
        self.device.on_playback_start()
        self.jitter_buffer = []

    def play_chunk(self, idx: int) -> None:
        """Simulate audible playout → advances commit_cursor."""
        if idx < 0 or idx >= len(self.state.assistant_draft_chunks):
            return
        self.jitter_buffer.append(self.state.assistant_draft_chunks[idx])
        self.state.commit_cursor = idx

    def barge_in(self, mic_energy: float, is_echo_dominated: bool) -> bool:
        if not self.device.vad_speech_onset(mic_energy, is_echo_dominated):
            return False
        # 1) Local stop FIRST (must not wait for cloud RTT)
        self.device.on_playback_stop()
        self.jitter_buffer.clear()
        # 2) Cloud cancel + context rollback to commit_cursor
        self._cloud_cancel_and_rollback()
        return True

    def _cloud_cancel_and_rollback(self) -> None:
        self.cloud_generating = False
        committed = self.state.assistant_draft_chunks[: self.state.commit_cursor + 1]
        if committed:
            self.state.turns.append("assistant: " + "".join(committed))
        # Drop uncommitted draft tokens/audio from context window
        self.state.assistant_draft_chunks = committed


# ---------------------------------------------------------------------------
# dma-buf style zero-copy accounting
# ---------------------------------------------------------------------------

@dataclass
class DmaBuf:
    buf_id: int
    viewers: int = 0
    copies: int = 0

    def import_to_npu(self) -> None:
        self.viewers += 1  # share fd — no copy

    def mmap_cpu(self) -> None:
        self.viewers += 1

    def naive_copy_to_npu(self) -> None:
        self.copies += 1


# ---------------------------------------------------------------------------
# Demo
# ---------------------------------------------------------------------------

def demo() -> None:
    print("=== 1. Mouth-to-ear budget (<300ms) ===")
    budget = LatencyBudget()
    print(f"  total={budget.total_ms():.0f}ms ok={budget.ok()}")
    assert budget.ok()
    fat = LatencyBudget(downlink_jitter_ms=120)
    print(f"  oversized jitter total={fat.total_ms():.0f}ms ok={fat.ok()}")
    assert not fat.ok()

    print("\n=== 2. FEC recovers one lost UDP packet ===")
    group = [
        AudioPacket(0, b"AAAA"),
        AudioPacket(1, b"BBBB"),
        AudioPacket(2, b"CCCC"),
    ]
    parity = fec_encode(group)
    received = {0: group[0], 2: group[2]}  # seq=1 lost
    recovered = fec_recover(received, 1, [0, 1, 2], parity)
    assert recovered is not None and recovered.payload == b"BBBB"
    print("  recovered missing seq=1 without retransmission — ok")

    print("\n=== 3. Barge-in with AEC (no false trigger on echo) ===")
    session = VoiceSession()
    session.start_assistant_reply(
        ["Hello", " world", " today", " is", " sunny"], generation_id="g1"
    )
    session.play_chunk(0)
    session.play_chunk(1)  # commit_cursor=1
    # Echo while TTS playing — AEC suppresses → no barge-in
    assert session.barge_in(mic_energy=0.9, is_echo_dominated=True) is False
    # Real user speech
    assert session.barge_in(mic_energy=0.9, is_echo_dominated=False) is True
    assert session.cloud_generating is False
    assert session.state.commit_cursor == 1
    assert session.state.turns[-1] == "assistant: Hello world"
    assert session.jitter_buffer == []
    print(f"  rolled back to committed: {session.state.turns[-1]!r}")

    print("\n=== 4. dma-buf zero-copy vs naive copies ===")
    buf = DmaBuf(buf_id=1)
    buf.import_to_npu()
    buf.mmap_cpu()
    assert buf.copies == 0 and buf.viewers == 2
    naive = DmaBuf(buf_id=2)
    naive.naive_copy_to_npu()
    naive.naive_copy_to_npu()
    print(f"  dma-buf copies={buf.copies} viewers={buf.viewers}; naive copies={naive.copies}")
    assert naive.copies == 2

    print("\nvoice_assistant_pipeline: all demos passed.")


if __name__ == "__main__":
    demo()
