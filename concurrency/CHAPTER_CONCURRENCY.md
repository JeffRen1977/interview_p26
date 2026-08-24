# Chapter 9 — Concurrency

*Thread safety, data races, and the systems questions behind them*

---

## 9.0 Why this chapter is different

Every other chapter in this book has a comforting property: when you get the
answer wrong, something tells you. A bad binary search returns the wrong index.
A leaky allocator eventually runs out of memory. You write a test, the test
fails, you fix it.

Concurrency does not work like this.

A program with a data race in it will usually pass its tests. It will pass them
a thousand times in a row. It will pass them on your laptop, in CI, and in the
staging environment. Then it will corrupt a frame buffer at 3 a.m. on a customer
device, once, and never do it again.

Here is that claim as a measurement rather than a warning. The following
function has an unambiguous, textbook data race — four threads incrementing a
plain `int`:

```cpp
int counter = 0;
for (int i = 0; i < 4; ++i)
    threads.emplace_back([&] {
        for (int j = 0; j < 100000; ++j) ++counter;   // data race
    });
```

Compiled with optimizations on a modern machine, this prints:

```
counter=400000  expected=400000
```

The correct answer. Every time. The compiler kept the counter in a register for
the whole loop, so the threads barely interleaved at all. The code is still
undefined behaviour, ThreadSanitizer still reports it, and a different compiler,
a different core count, or a slightly different loop body will produce silent
corruption instead.

This is why concurrency interviews are structured the way they are. The
interviewer is not really checking whether you can produce a working
producer-consumer queue — plenty of people can type one from memory. They are
checking whether you know *which parts are load-bearing*, because in this domain
a program that works is not the same as a program that is correct, and only one
of those two things survives contact with production.

So this chapter is organized around judgment, not recall. Every section
introduces a pattern, then spends most of its time on the decisions inside it.
By the end you should be able to write four data structures from a blank file —
a bounded queue, a thread pool, a read-mostly cache, and a two-tier event store
— and, more importantly, defend every choice in them.

> **Running the code.** All examples come from a companion repository of
> compilable, tested programs. Each one builds with CMake and runs its own
> assertions:
>
> ```bash
> cmake -S . -B build && cmake --build build -j
> ctest --test-dir build --output-on-failure
> ```
>
> File names in the margin (like `thread_pool.cpp`) point at the full version.
> The listings in this chapter are trimmed to their essentials.

---

## 9.1 Getting the words right

Interviewers form an impression of your depth in the first two minutes, and the
cheapest way to make a good one is vocabulary. Not jargon — *precision*. Most
candidates say "it's not thread-safe, so I'll add a mutex." Very few can say
what "not thread-safe" means. The distinction below takes fifteen seconds to
state and immediately separates the two groups.

### Data race

A **data race** occurs when:

1. two threads access the same memory location,
2. at least one access is a write,
3. the accesses are not ordered by a happens-before relationship, and
4. neither access is atomic.

The crucial part is what follows: **a data race is undefined behaviour in C++.**
Not "you might read a stale value" — *undefined*. The compiler is entitled to
assume your program has no data races and to optimize on that assumption. This
is not pedantry; it is the reason the counter example above printed the right
answer, and it is the reason this innocent-looking loop can hang forever:

```cpp
bool stop = false;              // not atomic

std::thread worker([&] {
    while (!stop) { ++work; }   // may be hoisted out of the loop
});
stop = true;                     // may never be observed
```

Because a race-free program could never modify `stop` from another thread, the
compiler may load it once, before the loop, and turn the condition into a
constant. Your shutdown flag becomes an infinite loop. In the companion code
(`data_race_demos.cpp`) this version reports `work=0` after running for twenty
milliseconds — the increments were optimized away entirely, because nothing in a
race-free program could have observed them.

### Race condition

A **race condition** is broader and more mundane: the result depends on timing.
Every data race is a race condition, but the reverse is emphatically false, and
the gap between them is where the interesting bugs live.

```cpp
if (!cache.contains(key))      // takes the lock, releases it
    cache.insert(key, value);  // takes the lock again
```

Both calls are perfectly synchronized. There is no data race here — a sanitizer
will find nothing to complain about. The bug is that the lock is released
between the check and the act, so the answer goes stale before it is used. Run
this with eight threads and you get eight inserts where you wanted one.

> **Say it in the room.** "A data race is two unsynchronized accesses to the
> same location with at least one write — that's undefined behaviour. A race
> condition is the broader design bug where the outcome depends on timing. You
> can have a race condition with no data race at all, like check-then-act under
> a lock that's released in between." Fifteen seconds, and you have established
> more than most candidates do in the whole interview.

### Happens-before

The relation that makes concurrent access legal. You get it from:

- unlocking a mutex, then locking that same mutex;
- a release-store to an atomic, then an acquire-load of that same atomic;
- creating a thread (everything before `std::thread`'s constructor is visible in
  the new thread);
- `join()`;
- fulfilling a `promise`, then `future::get()`.

That list is short and worth memorizing, because "is there a happens-before edge
here?" is the question you will actually be asking yourself when you audit code.

### The sentence that reframes everything

Here is the single most useful idea in this chapter:

> **Locks do not protect variables. Locks protect invariants.**

An invariant is a property that spans several pieces of state and must appear
true to every observer. Once you internalize this, a whole family of bugs
becomes visible at a glance — starting with the most common misconception in
concurrent C++:

```cpp
class BrokenStats {
 public:
    void record(int latency_ms) {
        count_.fetch_add(1, std::memory_order_relaxed);
        total_.fetch_add(latency_ms, std::memory_order_relaxed);
    }
    double average() const {
        const int c = count_.load(std::memory_order_relaxed);
        const long long t = total_.load(std::memory_order_relaxed);
        return c == 0 ? 0.0 : static_cast<double>(t) / c;
    }
 private:
    std::atomic<int> count_{0};
    std::atomic<long long> total_{0};
};
```

Every member is atomic. Every individual operation is race-free. A sanitizer
reports nothing. **And the class is broken**, because a reader can arrive
between the two `fetch_add`s and observe a `count_` that has been incremented
against a `total_` that has not. The average comes out wrong. There is no data
race anywhere — the invariant "`total_` is the sum of `count_` samples" simply
isn't atomic, and no amount of making the *fields* atomic will make the
*relationship between them* atomic.

The fix is not more atomics. It is one lock around the whole invariant:

```cpp
void record(int latency_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++count_;
    total_ += latency_ms;
}
```

When an interviewer shows you a class full of `std::atomic` members and asks
whether it is thread-safe, this is the answer they are hoping for.

---

## 9.2 The toolbox

### Choosing a primitive

| Primitive | Reach for it when |
|---|---|
| `std::mutex` | Default. Always your first answer. |
| `std::lock_guard` | Simple scoped locking. Cheapest, least flexible. |
| `std::unique_lock` | You need to unlock early, defer, try — or use a condition variable. |
| `std::scoped_lock` (C++17) | Locking two or more mutexes at once. Deadlock-free by construction. |
| `std::shared_mutex` (C++17) | Reads dominate **and** the read critical section is long. See §9.5 — this is not free. |
| `std::recursive_mutex` | Almost never. Usually a design smell; see §9.8. |
| `std::atomic` | Single-variable state: counters, flags, pointers. |
| `std::condition_variable` | Waiting for a *state change* (not for a lock). |
| `std::call_once` / function-local static | One-time initialization. |
| Spinlock | The critical section is a few instructions and you will not be preempted holding it. Rare. |

### Six rules that are always true

**1. Always wait with a predicate.**

```cpp
cv.wait(lock, [&] { return !queue_.empty() || stop_; });   // correct
if (queue_.empty()) cv.wait(lock);                          // broken
```

Spurious wakeups are explicitly permitted by the standard, and a notification
can arrive before you begin waiting. The predicate re-checks the real condition;
the condition variable is only a wakeup mechanism, never the source of truth.

**2. Never hold a lock across a blocking call.** No I/O, no `sleep`, no callback
into code you do not control, no waiting on another thread that might want your
lock. Compute under the lock, act outside it. In §9.4 you will see a rate
limiter that computes a sleep duration inside the critical section and performs
the sleep outside it — that structure is the whole point.

**3. Never return a reference or iterator to protected data.** The lock dies
when the function returns; the reference outlives it. Return by value, or return
a `shared_ptr` to an immutable snapshot.

**4. One lock ordering, documented, obeyed globally.** More in §9.8.

**5. Prefer `notify_one`,** unless waiters are blocked on *different* predicates
— in which case you must use `notify_all` or you can wake the wrong thread and
lose the notification entirely. This is why a bounded queue uses two condition
variables (`not_empty_`, `not_full_`) rather than one.

**6. Notify after unlocking where you can.** Otherwise the thread you just woke
immediately blocks on the mutex you are still holding.

### The memory-order ladder

Start at `seq_cst` — it is the default and it is always correct. Weaken only
when you can name the pairing out loud.

- **`relaxed`** — atomicity, no ordering. Correct for a statistics counter you
  read after `join()`. Wrong for anything that publishes data.
- **`acquire` / `release`** — the workhorse pair, and the one to reach for. The
  mental model is one sentence: *write the data, then release-store the flag;
  acquire-load the flag, then read the data.*

```cpp
// producer                        // consumer
payload.width  = 1920;             while (!ready.load(acquire)) {}
payload.height = 1080;             use(payload);   // guaranteed visible
ready.store(true, release);
```

  Note that `payload` is an ordinary non-atomic struct and there is still no
  data race, because the release/acquire pair created a happens-before edge.

- **`acq_rel`** — read-modify-write operations that both consume and publish.
- **`seq_cst`** — adds a single total order over all sequentially-consistent
  operations across all threads.

### `volatile` is not a concurrency tool

In C++, `volatile` means only "do not optimize this access away." It exists for
memory-mapped hardware registers. It provides no atomicity and no ordering. If
you have written Java or C#, unlearn what `volatile` means there — it is a
genuinely different keyword with genuinely different semantics.

---

## 9.3 Pattern 1 — The bounded queue

Almost every concurrency interview passes through a producer-consumer queue,
either as the question or as a component of the question. It is worth being able
to write one without thinking, so that your attention is free for the parts that
actually differentiate candidates.

Here is the core, distilled (the full `bounded_blocking_queue.cpp` adds
timeouts and `try_`-variants on top of this skeleton):

```cpp
template <typename T>
class BoundedQueue {
 public:
    explicit BoundedQueue(std::size_t cap) : cap_(cap) {}

    bool push(T v) {
        std::unique_lock<std::mutex> lk(m_);
        not_full_.wait(lk, [&] { return q_.size() < cap_ || stop_; });
        if (stop_) return false;
        q_.push(std::move(v));
        lk.unlock();
        not_empty_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lk(m_);
        not_empty_.wait(lk, [&] { return !q_.empty() || stop_; });
        if (q_.empty()) return std::nullopt;      // stopped and drained
        T v = std::move(q_.front());
        q_.pop();
        lk.unlock();
        not_full_.notify_one();
        return v;
    }

    void shutdown() {
        { std::lock_guard<std::mutex> lk(m_); stop_ = true; }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

 private:
    std::queue<T> q_;
    const std::size_t cap_;
    mutable std::mutex m_;
    std::condition_variable not_empty_, not_full_;
    bool stop_ = false;
};
```

Twenty-five lines. The interesting content is in the four decisions behind them.

### Decision 1: bounded, always

An unbounded queue converts a slow consumer into an out-of-memory crash. If
producers can outrun consumers even briefly, memory grows without limit, and the
failure arrives far from the cause. Bounding the queue converts an unbounded
memory problem into a bounded, local, *visible* one: what should we do when it
is full? Which brings us to:

### Decision 2: the overflow policy is a product decision

When the queue is full, you have three choices, and the right one depends
entirely on what the data means:

- **Block the producer** (backpressure). Correct when you must not lose data —
  writing recorded video to disk, for instance. Losing recorded footage is a
  product failure, not a performance one.
- **Drop the oldest.** Correct when freshness beats completeness. For live
  video analytics you want the *most recent* frame, not a backlog of stale ones;
  a queue that has fallen behind is actively harmful.
- **Drop the newest.** Rare, but right when the existing backlog is a coherent
  unit you would rather finish than corrupt.

State your choice and your reason out loud. Candidates who silently pick one
look like they did not know there was a decision.

> **A trap worth knowing.** Real pipelines often need *both* policies on the
> same source: a recording path that must not lose frames and a preview path
> that must not lag. The answer is not one queue with a compromise policy; it is
> fan-out — one producer, several queues, each with its own policy, sharing
> frame data through `shared_ptr` so the payload is not copied per consumer.
> `local_sd_card_writer.cpp` builds exactly this.

### Decision 3: count what you drop

Whichever policy you choose, expose a counter. Silent data loss is the worst
failure mode a system can have, because it produces no symptom until someone
goes looking for footage that was never stored. This sounds obvious and is
routinely skipped.

### Decision 4: shutdown is part of the design

Notice that `stop_` appears in *both* wait predicates and that `shutdown()` uses
`notify_all`. A blocked thread does not notice a flag; it has to be woken to
look. Forgetting this is the most common way an otherwise-correct queue hangs on
teardown — and "how does this shut down?" is one of the most common follow-up
questions in the interview.

Note also that `pop()` returns `std::optional<T>`. Once shutdown is possible,
"pop returns a T" is no longer a total function, and the type should say so.

---

## 9.4 Pattern 2 — The thread pool

If you prepare exactly one data structure, prepare this one. It is the most
frequently asked concurrency implementation question, and it exercises task
queues, condition variables, futures, exception handling, and shutdown all at
once.

### The worker loop

```cpp
void worker_loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;      // drain policy
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();          // NOT under the lock
    }
}
```

Two details carry almost all the weight.

**The task runs outside the lock.** Invoking user code under the pool mutex
would serialize the entire pool — you would have N threads taking turns. Worse,
if a task ever calls `submit()` itself, it deadlocks instantly on a
non-recursive mutex. Interviewers ask "what if a task submits another task?"
precisely to see whether your lock scope is right.

**`if (stop_ && tasks_.empty())` encodes a policy.** This is *drain*: already-
queued work still runs before the pool exits. The alternative, *discard*, is
`if (stop_) return;`. Neither is wrong — but you should know which one you wrote
and be able to say why. Drain is the safer default; discard is correct when
shutdown means "abandon everything, we are going down now."

### Submission and futures

```cpp
template <typename F, typename... Args>
auto submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {
    using Ret = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<Ret()>>(
        [func = std::forward<F>(f),
         tup  = std::make_tuple(std::forward<Args>(args)...)]() mutable -> Ret {
            return std::apply(std::move(func), std::move(tup));
        });

    std::future<Ret> result = task->get_future();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_) throw std::runtime_error("submit on stopped ThreadPool");
        tasks_.emplace([task] { (*task)(); });
    }
    cv_.notify_one();
    return result;
}
```

Three things to be ready to explain:

**Why `packaged_task`?** It is the adapter between "a callable" and "a promise."
It invokes the function and fulfils the associated future with either the return
value *or the thrown exception*. That second half matters: an exception inside a
task must not terminate the worker thread. It is captured, stored, and re-thrown
at the call site when the caller invokes `future::get()`. The pool stays healthy;
the error reaches the person who asked for the work.

**Why the `shared_ptr`?** `std::packaged_task` is move-only, but `std::function`
requires its target to be copy-constructible. Wrapping the task in a
`shared_ptr` makes the enclosing lambda copyable. This is a genuine wart, and
noticing it is a good sign. The C++23 answer is `std::move_only_function`; the
do-it-yourself answer is a small type-erased wrapper holding a
`unique_ptr<Concept>` (there is one at the bottom of `thread_pool.cpp`).

**Why throw on submit-after-shutdown?** Because the alternative is worse. If you
silently drop the task, the caller holds a future that will never become ready,
and they will block in `get()` forever. A hang is harder to debug than an
exception.

### Shutdown

```cpp
void shutdown() {
    { std::lock_guard<std::mutex> lock(mutex_); if (stop_) return; stop_ = true; }
    cv_.notify_all();
    for (std::thread& w : workers_) if (w.joinable()) w.join();
}

~ThreadPool() { shutdown(); }
```

The destructor must join. `std::thread`'s destructor calls `std::terminate` if
the thread is still joinable — a detail that has killed many demo programs at
exactly the wrong moment.

> **Follow-ups to expect.** *"How would you add work stealing?"* Give each
> worker its own deque, push locally, steal from the back of another worker's
> deque when yours is empty; it cuts contention on the single shared queue.
> *"How would you prioritize tasks?"* Swap the queue for a priority queue and
> talk about starvation. *"How do you size the pool?"* CPU-bound: roughly
> `hardware_concurrency()`. I/O-bound: more, because threads are blocked rather
> than running — and then explain that the real answer is to measure.

---

## 9.5 Pattern 3 — Read-mostly data, and a result that surprises people

Some state is read constantly and written almost never: camera configuration,
feature flags, routing tables, "is analytics enabled for this device?" checked
on every single frame. The instinct is immediate — *reads dominate, so use a
reader-writer lock.* Let us test that instinct.

Three implementations of the same read-mostly map (`readers_writer_patterns.cpp`):

**A plain mutex**, as the baseline:

```cpp
CameraConfig get(std::uint32_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(id);
    return it == map_.end() ? CameraConfig{} : it->second;   // by value
}
```

**A shared mutex**, letting readers run concurrently:

```cpp
CameraConfig get(std::uint32_t id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);        // shared
    ...
}
void set(std::uint32_t id, const CameraConfig& cfg) {
    std::unique_lock<std::shared_mutex> lock(mutex_);        // exclusive
    map_[id] = cfg;
}
```

**Copy-on-write**, making reads lock-free entirely:

```cpp
CameraConfig get(std::uint32_t id) const {
    std::shared_ptr<const ConfigMap> snap = std::atomic_load(&map_);
    auto it = snap->find(id);
    return it == snap->end() ? CameraConfig{} : it->second;
}

void set(std::uint32_t id, const CameraConfig& cfg) {
    std::lock_guard<std::mutex> lock(write_mutex_);     // writers only
    auto new_map = std::make_shared<ConfigMap>(*std::atomic_load(&map_));
    (*new_map)[id] = cfg;
    std::atomic_store(&map_, std::shared_ptr<const ConfigMap>(new_map));
}
```

Eight reader threads, 1.6 million reads, one writer updating every 200µs:

| Strategy | Time |
|---|---|
| `std::mutex` | 54 ms |
| `std::shared_mutex` | **80 ms** |
| Copy-on-write | 46 ms |

**The reader-writer lock lost to a plain mutex.**

This is not a quirk of one machine. A `shared_mutex` has to track a reader count
and coordinate reader-writer handoff, and that bookkeeping is itself
synchronized. When the critical section is a single hash lookup — a few dozen
nanoseconds — the bookkeeping costs more than the concurrency it buys. The
shared lock only pays for itself when readers hold it long enough to amortize
that overhead: scanning a range, copying a large result, doing real work.

There is a version of `get_many()` in the companion file that reads many keys
under one lock, and *that* is where `shared_mutex` wins comfortably.

> **Say it in the room.** "A read-write lock helps when reads dominate *and* the
> read critical section is long. For a single hash lookup I've measured
> `shared_mutex` come out slower than a plain mutex, because the reader
> bookkeeping costs more than the contention it removes. I'd measure before
> choosing." This is a much stronger answer than "reads dominate, so I'd use
> `shared_mutex`," and it is the kind of thing that gets remembered in the
> debrief.

### How copy-on-write wins, and what it costs

COW readers take no lock at all: one atomic load and a refcount increment. The
returned `shared_ptr` keeps that version of the map alive even if a writer swaps
in a new one a nanosecond later — which is precisely what makes it safe.

The costs are real. Every write copies the entire structure, so COW suits small,
rarely-written data and nothing else. A reader holding an old snapshot keeps it
alive, so memory is bounded by the slowest reader. And note the portability
wrinkle in the code above: `std::atomic<std::shared_ptr<T>>` is C++20; the
free functions `std::atomic_load`/`atomic_store` are the C++17 spelling and are
deprecated in C++20. Know both.

COW also hands you something the locking versions cannot: a *consistent
multi-key view* for free. One snapshot, no lock, no possibility of the map
changing between two lookups.

### Scaling patterns, in the order you should propose them

1. **Don't share.** Thread-local accumulation, merged at the end. Always the
   best answer when it applies.
2. **Shard.** N independent locks keyed by `hash(key) % N`. The standard answer
   to map contention, and the standard price is losing any global ordering
   property across shards. (Watch the hash: `std::hash` for integers is often
   the identity, so sequential keys land in the same shard after the modulo.
   Mix the bits first.)
3. **Reader-writer lock.** Only with a long read critical section, per above.
4. **Copy-on-write / RCU.** Lock-free reads, O(n) writes, small data.
5. **Lock-free with raw atomics.** Last resort, justified by a measurement.

> **Perspective.** An uncontended `std::mutex` costs roughly 20 nanoseconds.
> Most instincts of the form "we need something lock-free here" are wrong, and
> saying so demonstrates the judgment that senior interviews are actually
> testing. Note also that `std::atomic<T>::is_lock_free()` can return `false`
> for non-trivial `T` — the library silently took a lock and you gained nothing.
> Always check.

### An aside on cache lines

While measuring, one more result worth carrying into the interview. Two threads,
each incrementing its own counter, no logical sharing at all:

| Layout | Time |
|---|---|
| Both counters in one cache line | 35 ms |
| `alignas(64)` on each | **7 ms** |

Five times faster from placement alone. This is **false sharing**: the two
counters live on one 64-byte cache line, so the cores fight over that line
through the coherence protocol despite never touching the same variable. The fix
is padding — `alignas(64)`, or `std::hardware_destructive_interference_size`.
This is also why the lock-free SPSC ring in `spsc_ring_buffer.cpp` puts its head
and tail indices on separate cache lines.

---

## 9.6 Pattern 4 — When real-time meets historical

This is the systems design question, and it is the one worth the most
preparation, because it is where a concurrency interview stops being about
syntax and starts being about architecture.

The setup is always some version of this: *data arrives continuously and must be
ingested with low latency; queries arrive sporadically and ask about arbitrary
time ranges, including ranges that reach back weeks.* Security cameras emitting
motion events. Trading systems with live ticks and historical bars. Telemetry
pipelines. Log aggregators. Same shape every time.

Two access patterns share one dataset:

|  | Real-time writes | Historical reads |
|---|---|---|
| Frequency | High, continuous | Bursty |
| Latency tolerance | Very low | Tens of milliseconds is fine |
| Shape | Append-only | Range scan by key and time |
| Volume | Huge over time | Huge per query |

The naive design puts one mutex around one big container. It is correct, and it
fails the moment a user runs a large query: that query holds the lock for
milliseconds, and the ingest path — which must never stall — stalls. On a camera
that means dropped frames caused by someone in another building clicking a date
picker.

### The core idea

> **Split the data by temperature, and give each tier the synchronization
> primitive that matches its access pattern.**

This one sentence is the answer to the question. Everything else is detail.

| Tier | Contents | Access pattern | Primitive |
|---|---|---|---|
| **Hot** | Last N seconds, fixed capacity | High-frequency append, latency-critical | Plain `mutex` — the critical section is a few instructions |
| **Cold** | Full history, indexed | Read-dominated range scans | `shared_mutex` — long critical section, many readers |
| **Bridge** | Background flusher, hot → cold | Single writer | Serialized; it is the only cold writer |

The ingest path touches only the hot ring. It performs no allocation, no I/O,
and never takes the cold tier's lock — so a slow query cannot block it. The
query path fans out to both tiers and merges. A background thread moves data
from hot to cold on an interval.

```
   ingest ──▶ ┌──────────────┐   flusher    ┌──────────────────┐
              │   HOT RING   │ ───────────▶ │  COLD ARCHIVE    │
              │  fixed cap   │  (batches)   │  per-camera,     │
              │  std::mutex  │              │  time-sorted     │
              └──────────────┘              │  shared_mutex    │
                     │                      └──────────────────┘
                     │                               │
                     └──────────┬────────────────────┘
                                ▼
                         query: merge + sort + dedupe
```

The hot tier's append is about as small as a critical section gets:

```cpp
void append(const Event& e) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ == capacity_) {                      // overwrite oldest
        if (slots_[head_].seq > draining_upto_seq_) ++dropped_;
        head_ = (head_ + 1) % capacity_;
        --count_;
    }
    Slot& slot = slots_[(head_ + count_) % capacity_];
    slot.event = e;
    slot.seq = ++next_seq_;
    ++count_;
}
```

The cold tier is the mirror image — a long critical section under a shared lock,
which is exactly the case where `shared_mutex` earns its keep:

```cpp
std::vector<Event> query(std::uint32_t camera_id,
                         std::uint64_t t1, std::uint64_t t2) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);      // shared
    ...
    for (auto e = series.lower_bound(t1); e != series.upper_bound(t2); ++e)
        out.push_back(e->second);
    return out;
}
```

Note how §9.5's lesson applies in both directions within one design: a plain
mutex for the tiny append, a shared mutex for the long scan. The primitive
follows the critical section, not the read/write ratio alone.

### Three subtleties — and this is the good part

The design above is easy to describe and surprisingly easy to get wrong. The
three bugs below were all present in the first working version of the companion
code, and all three were caught by its own tests. They are the most valuable
material in this chapter, because they are exactly the follow-up questions an
interviewer will push you toward.

#### Subtlety 1: do not trust an ordering handed to you by concurrent callers

The flusher needs to know what it has already moved to cold storage. The obvious
mechanism is a watermark: *everything with id ≤ N has been flushed.* The first
version used the producer's event id for this.

It silently lost data.

Producers take an id from a shared atomic counter and *then* append:

```cpp
const std::uint64_t id = next_id.fetch_add(1);   // thread A gets 100
store.ingest(Event{id, ...});                    // ... then is descheduled
```

Thread A can take id 100, get descheduled, and append *after* thread B appends
id 101. Ids do not arrive in increasing order. So "everything ≤ 101 is flushed"
becomes true while event 100 is still sitting unflushed in the ring, the flusher
skips it forever, and it is eventually overwritten. Gone — with no error, no
exception, and no failing test until one asserted on total conservation.

The fix is to stop borrowing someone else's ordering:

```cpp
slot.seq = ++next_seq_;   // assigned inside append(), under the ring's mutex
```

Because the sequence number is assigned under the same lock that performs the
insertion, sequence order *is* insertion order by construction, and a scalar
watermark over it is finally sound.

> **The general lesson**, and it is worth stating this way in an interview: *a
> component that depends on an ordering should define that ordering itself
> rather than trusting one produced by concurrent callers.* Timestamps have the
> same problem, incidentally — two events generated microseconds apart on
> different threads can be appended in the opposite order.

#### Subtlety 2: when you cannot remove a failure mode, choose the one you can repair

A query reads the watermark, then queries cold, then queries hot. The flusher
runs concurrently. There are exactly two possible orderings, and both are wrong:

- **Read the watermark, then query cold.** The flusher may advance past the
  watermark before cold is read, so cold returns events the hot filter does not
  exclude. Result: **duplicates**.
- **Query cold, then read the watermark.** The flusher may move an event into
  cold after the cold read but before the watermark read. Cold's snapshot
  predates it; hot filters it out as already-flushed. Result: **a lost event**.

There is no third ordering. You cannot have both properties without a lock
spanning both tiers — which would reintroduce exactly the coupling the whole
design exists to avoid.

So choose deliberately. Losing an event is unacceptable; a duplicate is one
`std::unique` away from being fixed:

```cpp
const std::uint64_t wm = cold_.watermark();          // read BEFORE cold query

std::vector<Event> result = cold_.query(camera_id, t1, t2);
std::vector<Event> recent = hot_.query(camera_id, t1, t2, wm);
result.insert(result.end(), recent.begin(), recent.end());

std::sort(result.begin(), result.end());             // by (timestamp, id)
result.erase(std::unique(result.begin(), result.end(),
                         [](const Event& a, const Event& b) {
                             return a.id == b.id;
                         }),
             result.end());
```

The tempting instinct is to keep tuning the watermark until it handles both
cases. It cannot. Recognizing an impossibility and engineering around it is a
stronger answer than an elaborate scheme that quietly still loses data.

#### Subtlety 3: a metric that cries wolf is worse than no metric

The hot ring counts an event as dropped when it is overwritten before being
flushed. The first version reported this:

```
wrote=8000  cold=8000  dropped=802
```

Every event reached cold storage, and 802 were reported lost. The counter was
comparing against `flushed_upto_seq_` — updated only *after* the batch was
durably in cold storage. But the flusher copies events out first, writes them,
and marks them flushed last. An overwrite inside that window is harmless: the
flusher is already holding the data. The counter was firing on a safe race.

The fix is to distinguish two states that had been conflated — **captured** by
the flusher, and **durable** in cold storage:

```cpp
std::uint64_t draining_upto_seq_ = 0;  // copied out by the flusher
std::uint64_t flushed_upto_seq_  = 0;  // durably in the cold tier
```

Afterwards the accounting is exact — `cold + dropped == written` on every run —
and the drops that remain are genuine ring overwrites from a flusher that fell
behind, which is precisely the backpressure signal the metric exists to provide.

> **A fourth one, briefly.** `flush_now()` was public, so a caller could drain
> concurrently with the background flusher and double-insert into cold storage.
> The design said "the flusher is the only writer to the cold tier" — but that
> was a *comment*, not a guarantee. If your design depends on an invariant, make
> the code enforce it. A `std::mutex` around the flush path costs nothing and
> turns a comment into a fact.

### Points to volunteer

When you present this design, the following will each land:

- **Use `steady_clock`, never `system_clock`,** for anything measuring elapsed
  time. `system_clock` is wall-clock and can jump backwards under NTP
  correction. A camera that has just synced time after boot will hand your rate
  limiter a negative interval or your cache a timeout hours in the future. This
  is a real bug, not a theoretical one.
- **Bounded memory or you die.** The hot tier must be fixed-capacity.
- **Expose the drop counter,** per §9.3.
- **Partition further under load.** Shard by camera, and make the cold tier
  immutable time-partitioned segments so writers never touch what readers are
  scanning — at which point the reader lock largely disappears.
- **Name the failure mode.** "If the flusher falls behind, the ring overwrites
  unflushed events and we lose data. That's why the drop counter exists, and
  it's what I'd alarm on."

---

## 9.7 The bugs that hide

Five archetypes, in rough order of how often they appear in real code. Every one
of them is in `data_race_demos.cpp` in both broken and fixed form.

**1. Non-atomic read-modify-write.** `++counter` is load, add, store — three
steps, interleavable, updates lost. Fix: `fetch_add`, or a lock.

**2. The non-atomic flag.** Covered in §9.1: the compiler may hoist the load out
of the loop and your shutdown flag becomes an infinite loop. Fix:
`std::atomic<bool>` (or `std::stop_token` in C++20).

**3. Atomic members, non-atomic invariant.** The `BrokenStats` class from §9.1.
Fix: lock the invariant, not the fields.

**4. Check-then-act.** The lock is released between the check and the act, so
the answer goes stale. Fix: one operation that does both —
`insert_if_absent(k, v)` rather than `contains(k)` followed by `insert(k, v)`.

**5. Escaping references.** Returning `const std::string&` from a method that
released its lock on return. The caller reads it after a writer has rehashed the
map. Fix: return by value, or return a `shared_ptr` snapshot.

### The one your tools cannot find

Build all five broken versions under ThreadSanitizer and run them. You get
**two** race reports for **three** broken demos.

Archetype 4 — check-then-act — is completely silent. TSan has nothing to report,
because there *is* no data race: every access is properly locked. The bug is
that the synchronization boundary is in the wrong place, and no sanitizer can
detect that, because nothing about it violates the memory model. The program is
perfectly well-defined. It simply computes the wrong answer.

> **Say it in the room.** "ThreadSanitizer is necessary but not sufficient. It
> finds unsynchronized access; it cannot find a wrongly-placed synchronization
> boundary. For that I have to reason about which invariant the lock protects
> and whether it's ever observed mid-update. Tools check the memory model; only
> I can check the design."

---

## 9.8 Deadlock

Know the four conditions cold, because they are asked directly:

1. **Mutual exclusion** — the resource cannot be shared.
2. **Hold and wait** — a thread holds one lock while waiting for another.
3. **No preemption** — locks cannot be forcibly taken away.
4. **Circular wait** — a cycle in the waits-for graph.

Break any one and deadlock becomes impossible. In practice you break the last
two. The canonical setup is transferring between two accounts, or in a camera
system, moving a recording between two storage volumes:

```cpp
void transfer_BROKEN(Volume& from, Volume& to, std::int64_t n) {
    std::lock_guard<std::mutex> l1(from.mutex());
    std::lock_guard<std::mutex> l2(to.mutex());     // may already be held
    ...
}
```

`transfer(A, B)` locks A then B. `transfer(B, A)` locks B then A. Two threads,
opposite directions, deadlock.

**Fix 1 — `std::scoped_lock`.** Give this answer first.

```cpp
std::scoped_lock lock(from.mutex(), to.mutex());    // atomic acquisition
```

It acquires all the mutexes with a deadlock-avoidance algorithm, is
exception-safe, and unlocks in reverse order. The C++11 spelling is
`std::lock(m1, m2)` followed by two `lock_guard`s with `std::adopt_lock` — know
both, and note that `scoped_lock` is the modern one-liner.

**Fix 2 — a global lock ordering.** Break circular wait by hand. Use this when
the locks are acquired in different functions and no single `scoped_lock` can
see them all:

```cpp
Volume* first  = &from;
Volume* second = &to;
if (first->id() > second->id()) std::swap(first, second);   // canonical order
std::lock_guard<std::mutex> l1(first->mutex());
std::lock_guard<std::mutex> l2(second->mutex());
```

Any total order works as long as it is documented and universally obeyed. In the
two-tier store of §9.6 the ordering is written once, in a comment beside the
members, and never violated: `flush_mutex_ → hot_.mutex_ → cold_.mutex_`.

**Fix 3 — try-lock and back off.** Break hold-and-wait instead:

```cpp
for (;;) {
    std::unique_lock<std::mutex> l1(from.mutex());
    std::unique_lock<std::mutex> l2(to.mutex(), std::try_to_lock);
    if (l2.owns_lock()) { /* do the work */ return; }
    l1.unlock();                                   // release before retrying
    std::this_thread::sleep_for(jitter());
}
```

This can never deadlock, but it can **livelock** if every thread retries in
lockstep — hence the randomized backoff. Mentioning livelock unprompted is a
strong signal.

### The other deadlock: self-recursion

A public method takes the lock, then calls another public method that takes the
same lock. On a `std::mutex` that is undefined behaviour and in practice a hang.

The reflexive fix is `std::recursive_mutex`. The better fix — and the one that
impresses — is to split the class:

```cpp
void add(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    add_locked(id);
}
void add_many(const std::vector<int>& ids) {
    std::lock_guard<std::mutex> lock(mutex_);      // taken ONCE
    for (int id : ids) add_locked(id);
}
 private:
    void add_locked(int id) { items_.push_back(id); }  // precondition: locked
```

A `recursive_mutex` usually hides a design problem: once re-entry is allowed you
can no longer say which invariants hold at any given point, because you might be
halfway through a modification one frame up the stack. The `_locked` suffix
convention encodes the precondition in the name and costs nothing.

---

## 9.9 Verifying concurrent code

Testing cannot prove the absence of a race. We opened this chapter with a racy
program that printed the correct answer every time. So what does work?

### ThreadSanitizer

TSan is a **happens-before race detector**. It does not look for wrong answers;
it instruments memory accesses and reports any pair that lacks a happens-before
edge — even on a run that produced perfectly correct output. That is exactly the
property you need.

```bash
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build build-tsan -j
ctest --test-dir build-tsan --output-on-failure
```

Two practical notes. TSan is roughly 5–15x slower, and far worse for tight
atomic loops — a benchmark that runs in 200 ms natively can sit for minutes. The
answer is to shrink iteration counts under instrumentation, not to skip the run:
**TSan needs one conflicting pair of accesses, not a million.** The companion
code detects the sanitizer at compile time and scales its loops down by 100x.

And TSan cannot be combined with AddressSanitizer; use ASan+UBSan in a separate
build for the memory bugs that concurrency bugs tend to turn into.

### Make sure your tests can fail

A cautionary tale. The companion suite is built on `assert`. Configured as
`RelWithDebInfo`, CMake defines `NDEBUG` — which compiles every `assert` to
nothing. Fifteen test programs ran, printed `ok`, and verified *nothing at all*.
The only visible symptom was a scattering of "variable set but not used"
warnings, which is a remarkably quiet way for an entire test suite to be
vacuous.

```cmake
foreach(cfg RELEASE RELWITHDEBINFO MINSIZEREL)
    string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_${cfg} "${CMAKE_CXX_FLAGS_${cfg}}")
endforeach()
```

Every one of the bugs in §9.6 was found in the ten minutes after this change.
Before it, the suite was green and the code was broken. Whatever your assertion
mechanism, confirm it can actually fail.

### Test the invariant, not the output

Concurrent tests should assert on properties that must hold under *every*
interleaving, rather than on a specific expected result:

- **Conservation.** Total bytes across all volumes is unchanged after ten
  thousand concurrent transfers. Catches lost updates, which a deadlock test
  would not.
- **Capacity.** The cache never exceeds its bound, no matter the interleaving.
- **Ordering and uniqueness.** Query results are always sorted and never contain
  duplicates.
- **Budget.** The rate limiter never grants more than `burst + rate × elapsed`.
- **Nothing vanishes.** Every event is in cold storage, still in hot, or
  explicitly counted as dropped.

That last one is what caught the id-ordering bug of §9.6. An output-equality
test would have passed.

---

## 9.10 In the room

### Before you write anything, ask

- How many producers, how many consumers?
- Bounded or unbounded? What happens when it is full?
- What are the latency requirements? Is any path real-time?
- What is the read/write ratio, and how long is a typical read?
- How does this shut down?

These are not stalling tactics. Each one changes the design, and asking them
demonstrates that you know it.

### While you write

**Build the simple correct version first.** Say so explicitly: *"Let me get this
correct with a plain mutex, and then we can talk about optimizing it."* No
interviewer has ever been annoyed by this. Many candidates die in the weeds of a
lock-free design they cannot finish.

**Narrate the invariant.** Not "this locks the queue," but "this lock protects
the invariant that `size_` matches the number of elements in the buffer." It is
the vocabulary of §9.1 applied live, and it is audibly different from what most
candidates say.

**Handle shutdown before you are asked.** It is the most common follow-up and
the most commonly forgotten piece.

### After you write, volunteer

- *"Here's how I'd test this — and here's why testing can't prove the absence of
  a race, so I'd run it under TSan."*
- *"Here's what I'd measure before optimizing."*
- *"Here's what happens under overload, and what I'd alarm on."*

### If you get stuck

Say what you are considering, out loud. Silence reads as being lost; thinking
aloud reads as collaboration, and an interviewer who can hear your reasoning can
nudge you back on track. In a concurrency interview especially, the reasoning is
the thing being evaluated — the code is just the artifact it leaves behind.

---

## Exercises

1. **One condition variable or two?** The queue in §9.3 uses two condition
   variables. Rewrite it with a single `cv_` on which both producers and
   consumers wait. Now construct a concrete interleaving in which `notify_one`
   wakes the wrong thread and the queue deadlocks with work still in it. What
   is the minimum fix, and why is two CVs better than that fix?

2. **Drain versus discard.** Change the thread pool of §9.4 from drain to
   discard. Which single line changes? Now make it a constructor parameter and
   write a test that distinguishes the two behaviours deterministically.

3. **Measure it yourself.** Extend the benchmark of §9.5 with a `get_many()`
   that reads 100 keys under one lock. At what number of keys does
   `shared_mutex` overtake `std::mutex` on your machine? Explain the crossover.

4. **The LRU trap.** A thread-safe LRU cache seems like it should allow
   concurrent reads. Explain why `get()` cannot take a shared lock in a strict
   LRU. Then implement a CLOCK cache whose `get()` can, and describe exactly
   what eviction accuracy you traded away.

5. **Break the store.** In the two-tier store of §9.6, revert the fix from
   Subtlety 1 so the watermark uses producer-assigned ids again. Write a test
   that reliably detects the resulting data loss. Why must the test assert on
   conservation rather than on query results?

6. **Find the silent one.** Build `data_race_demos.cpp` under TSan and run it
   with `--broken`. Three demos are broken; two are reported. Explain precisely
   why the third is invisible, and describe a code review practice that would
   catch it.

7. **Design.** A camera produces 30 fps. Analytics takes 50 ms per frame.
   Describe what happens, then design the fix. Now add a second consumer that
   writes every frame to disk and must never drop one. Why can these two
   consumers not share a queue?
