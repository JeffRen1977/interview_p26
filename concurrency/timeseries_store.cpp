// Problem: Unified real-time + historical event store.
//
// *** This is the problem Verkada's prompt is pointing at. ***
// "strategies for managing both real-time and historical data in
//  multi-threaded environments"
//
// Scenario:
//   Cameras emit events (motion, person detected, door opened) continuously.
//   Two very different access patterns hit the same data:
//
//     WRITE  (real-time): high frequency, latency-sensitive, append-only,
//                         must never block on disk I/O.
//     READ   (historical): "show me all events on camera 7 between T1 and T2",
//                          bursty, tolerant of tens of milliseconds,
//                          vastly more frequent than writes in a UI-driven
//                          product.
//
// The core design idea -- SAY THIS OUT LOUD IN THE INTERVIEW:
//   Split the data by temperature, and give each tier the synchronization
//   primitive that matches its access pattern.
//
//     HOT  tier: a fixed-capacity ring buffer of the most recent events.
//                Bounded memory, O(1) append, overwrite-oldest. This absorbs
//                the real-time write burst.
//     COLD tier: an indexed archive (per-camera, time-sorted). Written by a
//                background flusher, read by queries.
//
//   A query for [t1, t2] fans out to both tiers and merges. The ingest path
//   never touches the cold tier or the disk -- it only appends to the hot ring.
//
// Synchronization strategy per tier:
//   - Hot ring: one std::mutex. Critical section is a few instructions
//     (memcpy-ish append), so contention is negligible and a shared_mutex
//     would only add overhead.
//   - Cold archive: std::shared_mutex. Reads dominate by orders of magnitude
//     and the critical section is long (binary search + copying a range), so
//     letting readers run concurrently is a real win.
//   - The flusher moves data hot->cold. It is the only writer to the cold tier.
//
// Why not one big mutex over everything?
//   A long historical query would block the real-time ingest path -- exactly
//   the coupling that makes camera pipelines drop frames. Tiering decouples
//   the tail latency of queries from the ingest path.
//
// Whiteboard talking points / follow-ups to volunteer:
// - Timestamps must be monotonic for the merge to be correct; use
//   steady_clock for durations, system_clock only for wall-clock display.
// - Deduplication at the seam: an event can briefly live in BOTH tiers (it is
//   copied to cold before being overwritten in hot). Dedupe by (id) or by
//   using a half-open watermark. This file uses a flush watermark: cold owns
//   everything strictly before `flushed_upto_`, hot serves the rest.
// - Backpressure: if the flusher falls behind, the ring overwrites unflushed
//   events -> permanent data loss. Track and expose `dropped_`; a real system
//   alarms on it or applies backpressure upstream.
// - Lock ordering: any code touching both tiers takes hot BEFORE cold, always.
//   Documented and never violated -> no deadlock possible.

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "bench_scale.h"

// ---------------------------------------------------------------------------
// Event
// ---------------------------------------------------------------------------
struct Event {
    std::uint64_t id = 0;
    std::uint32_t camera_id = 0;
    std::uint64_t timestamp_ms = 0;  // monotonic, milliseconds
    std::uint8_t kind = 0;           // motion / person / door / ...

    bool operator<(const Event& other) const {
        // Sort by time, break ties by id so the ordering is a total order and
        // the merge is deterministic.
        if (timestamp_ms != other.timestamp_ms) {
            return timestamp_ms < other.timestamp_ms;
        }
        return id < other.id;
    }
};

// ---------------------------------------------------------------------------
// HotRing: bounded, overwrite-oldest buffer of the most recent events.
// ---------------------------------------------------------------------------
// Guarded by a plain mutex. append() is the real-time path: it does no
// allocation and no I/O, so the critical section is tiny.
//
// SUBTLETY #1 -- do not use the producer's event id as the flush watermark.
//   Producers take ids from a shared atomic counter, then append. Thread A can
//   take id 100, get descheduled, and append AFTER thread B appends id 101.
//   So ids do NOT arrive in increasing order, and "everything with id <= N is
//   flushed" is simply false. Event 100 would be skipped by the flusher
//   forever and silently lost.
//
//   Fix: the ring assigns its OWN sequence number inside append(), under the
//   same mutex. That makes seq order == insertion order by construction, so a
//   scalar "flushed up to seq S" watermark is actually sound. The lesson is
//   general: a component that needs an ordering should define that ordering
//   itself rather than trusting one handed in by concurrent callers.
class HotRing {
 public:
    explicit HotRing(std::size_t capacity)
        : capacity_(capacity), slots_(capacity) {
        assert(capacity_ > 0);
    }

    // Real-time ingest. Never blocks on anything but the (tiny) mutex.
    void append(const Event& e) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (count_ == capacity_) {
            // Overwriting the oldest slot. If the flusher never even copied
            // this event out, it is gone for good -- count it.
            //
            // Compare against draining_upto_seq_, NOT flushed_upto_seq_. The
            // flusher copies events out, writes them to cold, and only then
            // marks them flushed. An overwrite inside that window is harmless
            // -- the flusher already holds the data -- and counting it would
            // make the drop metric fire spuriously under load. A metric that
            // cries wolf is worse than no metric.
            if (slots_[head_].seq > draining_upto_seq_) {
                ++dropped_;
            }
            head_ = (head_ + 1) % capacity_;
            --count_;
        }
        Slot& slot = slots_[(head_ + count_) % capacity_];
        slot.event = e;
        slot.seq = ++next_seq_;  // assigned under the lock => monotonic
        ++count_;
    }

    // Copy out everything in [t1, t2] that hot still owns, i.e. events the
    // cold tier has not yet taken responsibility for.
    // `cold_watermark_seq`: cold owns every event with seq <= this.
    std::vector<Event> query(std::uint32_t camera_id, std::uint64_t t1,
                             std::uint64_t t2,
                             std::uint64_t cold_watermark_seq) const {
        std::vector<Event> out;
        std::lock_guard<std::mutex> lock(mutex_);
        for (std::size_t i = 0; i < count_; ++i) {
            const Slot& slot = slots_[(head_ + i) % capacity_];
            if (slot.seq <= cold_watermark_seq) {
                continue;  // cold tier already serves this one
            }
            const Event& e = slot.event;
            if (e.camera_id == camera_id && e.timestamp_ms >= t1 &&
                e.timestamp_ms <= t2) {
                out.push_back(e);
            }
        }
        return out;
    }

    // Take a snapshot of everything not yet flushed, for the background
    // flusher. Returns the highest seq in the batch.
    std::uint64_t drain_unflushed(std::vector<Event>& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::uint64_t max_seq = draining_upto_seq_;
        for (std::size_t i = 0; i < count_; ++i) {
            const Slot& slot = slots_[(head_ + i) % capacity_];
            if (slot.seq > draining_upto_seq_) {
                out.push_back(slot.event);
                max_seq = std::max(max_seq, slot.seq);
            }
        }
        // Publish "captured" immediately, while still under the lock, so a
        // concurrent append() does not count these as dropped.
        draining_upto_seq_ = max_seq;
        return max_seq;
    }

    // Called by the flusher AFTER the batch is durably in the cold tier.
    void mark_flushed(std::uint64_t upto_seq) {
        std::lock_guard<std::mutex> lock(mutex_);
        flushed_upto_seq_ = std::max(flushed_upto_seq_, upto_seq);
    }

    std::uint64_t dropped() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dropped_;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

 private:
    struct Slot {
        Event event;
        std::uint64_t seq = 0;
    };

    const std::size_t capacity_;
    std::vector<Slot> slots_;
    std::size_t head_ = 0;
    std::size_t count_ = 0;
    std::uint64_t next_seq_ = 0;
    std::uint64_t draining_upto_seq_ = 0;  // copied out by the flusher
    std::uint64_t flushed_upto_seq_ = 0;   // durably in the cold tier
    std::uint64_t dropped_ = 0;
    mutable std::mutex mutex_;
};

// ---------------------------------------------------------------------------
// ColdArchive: per-camera, time-sorted history. Read-mostly.
// ---------------------------------------------------------------------------
// Guarded by shared_mutex: many concurrent readers, one writer (the flusher).
// The critical section is long (binary search + range copy), which is exactly
// when a reader-writer lock pays for itself.
class ColdArchive {
 public:
    // Writer side -- exclusive. Only the flusher calls this.
    // `upto_seq` is the hot-ring sequence number this batch covers.
    void insert_batch(const std::vector<Event>& batch, std::uint64_t upto_seq) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        for (const Event& e : batch) {
            // multimap keyed by timestamp gives us sorted order + range query
            // for free. A real system would use immutable time-partitioned
            // segments so writes never touch what readers are scanning.
            by_camera_[e.camera_id].emplace(e.timestamp_ms, e);
        }
        // Published only after the data is in the map, and while still holding
        // the exclusive lock: a reader that sees this watermark is guaranteed
        // to see the rows too.
        watermark_seq_.store(
            std::max(watermark_seq_.load(std::memory_order_relaxed), upto_seq),
            std::memory_order_release);
    }

    // Reader side -- shared. Many of these run at once.
    std::vector<Event> query(std::uint32_t camera_id, std::uint64_t t1,
                             std::uint64_t t2) const {
        std::vector<Event> out;
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_camera_.find(camera_id);
        if (it == by_camera_.end()) {
            return out;
        }
        const auto& series = it->second;
        // O(log n) to find the start, then linear over the matching range.
        for (auto e = series.lower_bound(t1); e != series.upper_bound(t2); ++e) {
            out.push_back(e->second);
        }
        return out;
    }

    // Cold owns every event with hot-ring seq <= watermark.
    std::uint64_t watermark() const {
        return watermark_seq_.load(std::memory_order_acquire);
    }

    std::size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::size_t n = 0;
        for (const auto& kv : by_camera_) {
            n += kv.second.size();
        }
        return n;
    }

 private:
    std::unordered_map<std::uint32_t, std::multimap<std::uint64_t, Event>>
        by_camera_;
    mutable std::shared_mutex mutex_;
    std::atomic<std::uint64_t> watermark_seq_{0};
};

// ---------------------------------------------------------------------------
// EventStore: ties the two tiers together.
// ---------------------------------------------------------------------------
class EventStore {
 public:
    explicit EventStore(std::size_t hot_capacity,
                        std::chrono::milliseconds flush_interval =
                            std::chrono::milliseconds(5))
        : hot_(hot_capacity), flush_interval_(flush_interval) {
        flusher_ = std::thread([this] { flush_loop(); });
    }

    ~EventStore() { stop(); }

    EventStore(const EventStore&) = delete;
    EventStore& operator=(const EventStore&) = delete;

    // Real-time write path. Hot ring only. No I/O, no cold-tier lock.
    void ingest(const Event& e) { hot_.append(e); }

    // Historical read path: fan out to both tiers, merge, sort, dedupe.
    //
    // SUBTLETY #2 -- the watermark alone cannot make this both lossless and
    // duplicate-free, because the flusher runs concurrently with the query.
    // There are exactly two orderings and they fail differently:
    //
    //   (a) read watermark W, then query cold.
    //       The flusher may advance past W before cold is read, so cold
    //       returns events with seq > W. Hot also returns them (it only drops
    //       seq <= W). -> DUPLICATES.
    //
    //   (b) query cold, then read watermark W.
    //       The flusher may move an event into cold after we read cold but
    //       before we read W. Cold's result predates it, and hot drops it
    //       because seq <= W. -> LOST EVENT.
    //
    // Losing an event is unacceptable; a duplicate is trivially removable.
    // So choose (a) deliberately and finish the job with an explicit dedupe.
    // "Pick the failure mode you can repair" is the actual design lesson --
    // and it is worth saying out loud, because the tempting answer is to
    // insist the watermark handles it.
    //
    // The alternative, if duplicates were expensive: take a seqlock-style
    // consistent snapshot of both tiers, or have the flusher publish the
    // batch and the watermark under one lock the reader also takes. Both cost
    // the ingest-path decoupling this whole design exists to buy.
    std::vector<Event> query(std::uint32_t camera_id, std::uint64_t t1,
                             std::uint64_t t2) const {
        const std::uint64_t wm = cold_.watermark();  // read BEFORE cold query

        std::vector<Event> result = cold_.query(camera_id, t1, t2);
        std::vector<Event> recent = hot_.query(camera_id, t1, t2, wm);
        result.insert(result.end(), recent.begin(), recent.end());

        // Sort is by (timestamp, id), so copies of one event land adjacent.
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end(),
                                 [](const Event& a, const Event& b) {
                                     return a.id == b.id;
                                 }),
                     result.end());
        return result;
    }

    // Force a flush and wait for it -- used by tests and by graceful shutdown.
    //
    // The cold tier's design assumes a SINGLE writer. But flush_now() is
    // public: a caller can invoke it while the background flusher is mid-flush,
    // and both would drain and insert the same events, duplicating rows in
    // cold storage. This mutex enforces the single-writer invariant the rest
    // of the design depends on. Whenever you write "only one thread does X",
    // make the code guarantee it rather than the comment.
    void flush_now() {
        std::lock_guard<std::mutex> flush_lock(flush_mutex_);
        std::vector<Event> batch;
        const std::uint64_t upto = hot_.drain_unflushed(batch);  // seq, not id
        if (batch.empty()) {
            return;
        }
        // Order matters: make it visible in cold BEFORE marking it flushed in
        // hot. Otherwise a query in between would find it in neither tier.
        cold_.insert_batch(batch, upto);
        hot_.mark_flushed(upto);
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(stop_mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
        }
        stop_cv_.notify_all();
        if (flusher_.joinable()) {
            flusher_.join();
        }
        flush_now();  // final drain so nothing is lost on shutdown
    }

    std::uint64_t dropped() const { return hot_.dropped(); }
    std::size_t hot_size() const { return hot_.size(); }
    std::size_t cold_size() const { return cold_.size(); }

 private:
    void flush_loop() {
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(stop_mutex_);
                // wait_for with a predicate: wakes early on shutdown instead of
                // sleeping out the full interval. Never use a bare sleep in a
                // loop you need to shut down promptly.
                if (stop_cv_.wait_for(lock, flush_interval_,
                                      [this] { return stopping_; })) {
                    return;
                }
            }
            flush_now();
        }
    }

    HotRing hot_;
    ColdArchive cold_;
    std::chrono::milliseconds flush_interval_;
    std::mutex flush_mutex_;  // one flusher at a time

    // Lock ordering, documented once and never violated:
    //   flush_mutex_ -> hot_.mutex_ -> cold_.mutex_
    // No path ever takes them in another order, so no cycle can form.

    std::thread flusher_;
    mutable std::mutex stop_mutex_;
    std::condition_variable stop_cv_;
    bool stopping_ = false;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
void test_query_spans_both_tiers() {
    EventStore store(1024);

    // Events 1..100 at t = 100..199 on camera 1.
    for (std::uint64_t i = 1; i <= 100; ++i) {
        store.ingest(Event{i, 1, 99 + i, 0});
    }
    store.flush_now();  // 1..100 now live in cold

    // Events 101..150 stay hot.
    for (std::uint64_t i = 101; i <= 150; ++i) {
        store.ingest(Event{i, 1, 99 + i, 0});
    }

    // A window straddling the hot/cold seam.
    auto r = store.query(1, 190, 220);
    assert(r.size() == 31);  // t = 190..220 inclusive
    for (std::size_t i = 0; i + 1 < r.size(); ++i) {
        assert(r[i].timestamp_ms < r[i + 1].timestamp_ms);  // sorted
    }
    // No duplicates across the seam.
    for (std::size_t i = 0; i + 1 < r.size(); ++i) {
        assert(r[i].id != r[i + 1].id);
    }
}

void test_camera_isolation() {
    EventStore store(1024);
    for (std::uint64_t i = 1; i <= 50; ++i) {
        store.ingest(Event{i, static_cast<std::uint32_t>(i % 5), 1000 + i, 0});
    }
    store.flush_now();
    auto r = store.query(3, 0, 100000);
    assert(r.size() == 10);
    for (const Event& e : r) {
        assert(e.camera_id == 3);
    }
}

// The real test: writers and readers hammering concurrently while the
// background flusher moves data between tiers. Run this under TSan.
void test_concurrent_ingest_and_query() {
    EventStore store(4096, std::chrono::milliseconds(1));

    constexpr int kWriters = 4;
    constexpr int kReaders = 4;
    const int kPerWriter = scaled(2000, 200);
    std::atomic<std::uint64_t> next_id{1};
    std::atomic<bool> done{false};
    std::atomic<std::uint64_t> total_read{0};

    std::vector<std::thread> threads;
    for (int w = 0; w < kWriters; ++w) {
        threads.emplace_back([&, w] {
            for (int i = 0; i < kPerWriter; ++i) {
                const std::uint64_t id =
                    next_id.fetch_add(1, std::memory_order_relaxed);
                store.ingest(Event{id, static_cast<std::uint32_t>(w),
                                   1000 + static_cast<std::uint64_t>(i), 0});
            }
        });
    }
    for (int r = 0; r < kReaders; ++r) {
        threads.emplace_back([&, r] {
            while (!done.load(std::memory_order_acquire)) {
                auto res = store.query(static_cast<std::uint32_t>(r % kWriters),
                                       1000, 1000 + kPerWriter);
                total_read.fetch_add(res.size(), std::memory_order_relaxed);
                // Sorted and duplicate-free, always.
                for (std::size_t i = 0; i + 1 < res.size(); ++i) {
                    assert(!(res[i + 1] < res[i]));
                    assert(res[i].id != res[i + 1].id);
                }
            }
        });
    }

    for (int i = 0; i < kWriters; ++i) {
        threads[static_cast<std::size_t>(i)].join();
    }
    done.store(true, std::memory_order_release);
    for (std::size_t i = kWriters; i < threads.size(); ++i) {
        threads[i].join();
    }

    store.stop();
    // Ring holds 4096; we wrote 8000. Nothing should be lost because the
    // flusher keeps up -- but assert the invariant we actually care about:
    // every event is either in cold, or still in hot, or explicitly counted
    // as dropped. Nothing vanishes silently.
    const std::uint64_t written =
        static_cast<std::uint64_t>(kWriters) * kPerWriter;
    assert(store.cold_size() + store.dropped() >= written - store.hot_size());
    std::cout << "  concurrent: wrote=" << written
              << " cold=" << store.cold_size()
              << " dropped=" << store.dropped()
              << " reads=" << total_read.load() << "\n";
}

int main() {
    test_query_spans_both_tiers();
    test_camera_isolation();
    test_concurrent_ingest_and_query();
    std::cout << "timeseries_store: ok\n";
    return 0;
}
