"""
Semantic cache for RAG — cosine hit, TTL, kb_version, neighborhood invalidate.

Interview talking points:
- Compare query↔query in a Cache Index (not query↔doc).
- Hit when cosine >= threshold (e.g. 0.95 in production; demo uses toy embeds).
- TTL lazy expiry; kb_version epoch clear; ANN-style neighborhood batch delete.

Run
----
    cd openai
    python3 semantic_cache.py
"""

from __future__ import annotations

import math
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Set


def cosine_similarity(a: Sequence[float], b: Sequence[float]) -> float:
    dot = sum(x * y for x, y in zip(a, b))
    na = math.sqrt(sum(x * x for x in a))
    nb = math.sqrt(sum(x * x for x in b))
    if na == 0.0 or nb == 0.0:
        return 0.0
    return dot / (na * nb)


def l2_normalize(vec: List[float]) -> List[float]:
    n = math.sqrt(sum(v * v for v in vec)) or 1.0
    return [v / n for v in vec]


@dataclass
class CacheEntry:
    entry_id: str
    query_vec: List[float]
    answer: str
    source_doc_ids: List[str]
    kb_version: int
    expires_at: float
    tenant_id: str = "default"


@dataclass
class Hit:
    entry_id: str
    answer: str
    score: float


class SemanticCache:
    """
    In-memory stand-in for: Embed → Cache Vector Index → Answer KV.
    ANN is emulated by brute-force top-k cosine (fine for interview demo).
    """

    def __init__(
        self,
        threshold: float = 0.95,
        ttl_sec: float = 86400.0,
        tenant_id: str = "default",
    ):
        self.threshold = threshold
        self.ttl_sec = ttl_sec
        self.tenant_id = tenant_id
        self.kb_version = 1
        self._entries: Dict[str, CacheEntry] = {}
        self._next_id = 1

    def lookup(self, query_vec: List[float], top_k: int = 5) -> Optional[Hit]:
        now = time.monotonic()
        scored: List[Hit] = []
        expired: List[str] = []

        for entry in self._entries.values():
            if entry.tenant_id != self.tenant_id:
                continue
            if entry.kb_version != self.kb_version:
                continue
            if entry.expires_at < now:
                expired.append(entry.entry_id)
                continue
            score = cosine_similarity(query_vec, entry.query_vec)
            scored.append(Hit(entry.entry_id, entry.answer, score))

        for eid in expired:
            self._entries.pop(eid, None)

        if not scored:
            return None
        scored.sort(key=lambda h: h.score, reverse=True)
        best = scored[0]
        if best.score >= self.threshold:
            return best
        return None

    def store(
        self,
        query_vec: List[float],
        answer: str,
        source_doc_ids: Optional[List[str]] = None,
        ttl_sec: Optional[float] = None,
    ) -> str:
        eid = f"e{self._next_id}"
        self._next_id += 1
        ttl = self.ttl_sec if ttl_sec is None else ttl_sec
        self._entries[eid] = CacheEntry(
            entry_id=eid,
            query_vec=list(query_vec),
            answer=answer,
            source_doc_ids=list(source_doc_ids or []),
            kb_version=self.kb_version,
            expires_at=time.monotonic() + ttl,
            tenant_id=self.tenant_id,
        )
        return eid

    def bump_kb_version(self) -> None:
        """Epoch invalidation: all prior entries become invisible."""
        self.kb_version += 1
        self._entries.clear()

    def invalidate_by_docs(self, doc_ids: Sequence[str]) -> int:
        """Precise invalidation via source_doc_ids recorded at write time."""
        victim: Set[str] = set(doc_ids)
        to_del = [
            eid
            for eid, e in self._entries.items()
            if victim.intersection(e.source_doc_ids)
        ]
        for eid in to_del:
            del self._entries[eid]
        return len(to_del)

    def invalidate_neighborhood(
        self,
        seed_vec: List[float],
        min_score: float = 0.90,
        top_k: int = 50,
    ) -> int:
        """
        Batch invalidate semantically nearby cache queries (Strategy C).
        Production: Vector DB ANN(seed_vec) then batch delete.
        """
        scored = []
        for entry in self._entries.values():
            if entry.tenant_id != self.tenant_id:
                continue
            if entry.kb_version != self.kb_version:
                continue
            score = cosine_similarity(seed_vec, entry.query_vec)
            if score >= min_score:
                scored.append((score, entry.entry_id))
        scored.sort(reverse=True)
        to_del = [eid for _, eid in scored[:top_k]]
        for eid in to_del:
            self._entries.pop(eid, None)
        return len(to_del)

    def size(self) -> int:
        return len(self._entries)


def _stable_hash(word: str, mod: int = 64) -> int:
    h = 2166136261
    for ch in word:
        h ^= ord(ch)
        h = (h * 16777619) & 0xFFFFFFFF
    return h % mod


def embed(text: str) -> List[float]:
    """Toy bag-of-words embedding (demo only — not a real model)."""
    vec = [0.0] * 64
    for word in text.lower().split():
        vec[_stable_hash(word)] += 1.0
    return l2_normalize(vec)


def demo() -> None:
    # Toy embeds rarely reach 0.95; production uses real models + θ=0.95.
    # Demo uses θ=0.85 for paraphrase pairs, and shows the 0.95 gate explicitly.
    cache = SemanticCache(threshold=0.85, ttl_sec=3600.0)

    q_reset_a = "how do i reset my api key"
    q_reset_b = "how can i reset my api key"
    q_france = "what is the capital of france"
    q_billing = "how do i update billing address"

    v_a, v_b = embed(q_reset_a), embed(q_reset_b)
    v_fr, v_bill = embed(q_france), embed(q_billing)

    print("=== 1. Semantic hit (query↔query cosine) ===")
    assert cache.lookup(v_a) is None
    cache.store(
        v_a,
        "Go to platform.openai.com/api-keys and rotate.",
        source_doc_ids=["doc:api-keys"],
    )
    hit = cache.lookup(v_b)
    print(f"  paraphrase score={cosine_similarity(v_a, v_b):.3f} hit={hit.answer if hit else None}")
    assert hit is not None
    assert cache.lookup(v_fr) is None

    print("\n=== 2. Production gate θ=0.95 (stricter) ===")
    strict = SemanticCache(threshold=0.95, ttl_sec=3600.0)
    strict.store(v_a, "cached", source_doc_ids=["doc:api-keys"])
    strict_hit = strict.lookup(v_b)
    print(
        f"  score={cosine_similarity(v_a, v_b):.3f} "
        f"θ=0.95 → {'HIT' if strict_hit else 'MISS (expected with toy embed)'}"
    )

    print("\n=== 3. TTL expiry ===")
    short = SemanticCache(threshold=0.85, ttl_sec=0.05)
    short.store(v_a, "will expire", source_doc_ids=["doc:api-keys"], ttl_sec=0.05)
    assert short.lookup(v_a) is not None
    time.sleep(0.06)
    assert short.lookup(v_a) is None
    print("  entry expired after TTL — ok")

    print("\n=== 4. Invalidate by source_doc_ids ===")
    cache.store(v_bill, "Update billing in console.", source_doc_ids=["doc:billing"])
    n = cache.invalidate_by_docs(["doc:api-keys"])
    print(f"  deleted {n} entries tied to doc:api-keys")
    assert cache.lookup(v_b) is None
    assert cache.lookup(v_bill) is not None  # unrelated doc kept

    print("\n=== 5. Neighborhood batch invalidate ===")
    cache.store(v_a, "reset answer again", source_doc_ids=["doc:api-keys"])
    # Seed with the reset query cluster; billing should survive if dissimilar.
    deleted = cache.invalidate_neighborhood(v_a, min_score=0.80, top_k=10)
    print(f"  neighborhood deleted={deleted}, remaining={cache.size()}")
    assert cache.lookup(v_b) is None

    print("\n=== 6. kb_version epoch clear ===")
    cache.store(v_fr, "Paris", source_doc_ids=["doc:geo"])
    cache.bump_kb_version()
    assert cache.lookup(v_fr) is None
    print("  kb_version bump cleared cache — ok")

    print("\nsemantic_cache: all demos passed.")


if __name__ == "__main__":
    demo()
