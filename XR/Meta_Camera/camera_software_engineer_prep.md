# Meta Camera Software Engineer — Interview Prep List

**Role:** Camera Software Engineer (Camera Systems / Wearables & AR/VR)

Preparing for Meta’s loop means mastering the intersection of:

- **Low-level systems engineering** — C/C++, concurrency, zero-copy buffer flow
- **Hardware–software co-design** — SoC / ISP / NPU, MIPI CSI-2, V4L2 / HAL
- **Strict edge constraints** — thermal envelopes, glass-to-glass latency, battery budgets

> **Camera 领域系统设计（E6 深度）：** [`camera_system_design/`](./camera_system_design/) — 九个场景，每篇都有 §6–§10 的 E6 层（数字预算 / 决策与否定方案 / 失效降级 / **怎么证明它是对的** / 演进与组织）。答题框架和白板速算表在该目录 README。

> **缺口分析与补充出题预测：** [`additional_questions.md`](./additional_questions.md) — 本清单没覆盖但很可能被问的题（相机–IMU 时间戳对齐、IR LED 亚像素质心、流式包解析、嵌入式 C/C++ 快问快答、调试轮、V4L2 驱动栈、三个新设计场景）。

> **Repo drills already in this workspace:** Embedded coding guide → [`coding_interview_guide.md`](./coding_interview_guide.md) · Camera system design → [`camera_system_design/`](./camera_system_design/) · RAW10 / ROTL / aligned malloc → [`code/`](./code/); lock-free SPSC → [`XR/Pico_vision/24-无锁SPSC队列与Cacheline对齐.md`](../Pico_vision/24-无锁SPSC队列与Cacheline对齐.md) · [`concurrency/spsc_ring_buffer.py`](../../concurrency/spsc_ring_buffer.py) · [`interview_handwrite/mpmc_ring_buffer.cpp`](../../interview_handwrite/mpmc_ring_buffer.cpp); ISP / 3A → [`XR/08`](../Pico_vision/08-影像ISP题详解.md) · [`camera/`](../../camera/); glasses thermal / zero-copy → [`company/openai/smart-glasses-ai-runtime.md`](../../company/openai/smart-glasses-ai-runtime.md).

---

## Loop map

| Round | What they evaluate | Prep focus |
| --- | --- | --- |
| Coding screen + 2 onsite | Algorithmic correctness, speed, edge cases, C/C++ systems fluency | Pointers, strided buffers, streaming windows, graphs, bits, concurrency |
| General system design | Scalable / real-time backend or device infrastructure | OTA, media sync, telemetry |
| Camera domain design | Sensor → MIPI → ISP → memory → NPU/GPU → display/encoder | Latency, 3A, power/thermal, calibration |
| Behavioral | Impact under ambiguity, HW/FW/algo/product collaboration, trade-offs | STAR stories |

---

## 1. Coding rounds (screening + 2 onsite)

**Embedded 电面节奏与四专题地图：** [`coding_interview_guide.md`](./coding_interview_guide.md)（60 min = 15–20 min STAR + 40–45 min C/C++，约 35 min 写完两题；禁止 AI）。可运行题在 [`code/`](./code/)。

Meta’s coding rounds evaluate algorithmic correctness, speed, deterministic edge-case handling, and systems-level C/C++ proficiency (memory layout, pointers, bit manipulation, concurrency).

### High-yield problem categories

| Category | Core focus areas | Typical Meta problems / patterns | Repo practice |
| --- | --- | --- | --- |
| **Pointers, buffers & memory** | Alignment, 2D strided buffers, ring buffers, zero-copy queues | Lock-free SPSC ring buffer; 2D image crop/rotate with arbitrary pitch/stride in-place; `aligned_malloc` for DMA / SIMD | [`code/aligned_malloc.md`](./code/aligned_malloc.md), [`XR/24`](../Pico_vision/24-无锁SPSC队列与Cacheline对齐.md), [`concurrency/video_ring_buffer.cpp`](../../concurrency/video_ring_buffer.cpp), [`camera/camera_driver.md`](../../camera/camera_driver.md) |
| **Sliding window & streaming** | Real-time sensor sample streams, rolling metrics, timestamps | Moving average of frame rates / exposure times; longest substring with constraints; median in a rolling data stream | [`leetcode/longest_substring_without_repeating/`](../../leetcode/longest_substring_without_repeating/) |
| **Trees & graphs / topo sort** | Pipeline graph compilation, DAG node dependency execution | Build order / topological sort for ISP node execution DAG; LCA; clone graph | [`leetcode/lowest_common_ancestor/`](../../leetcode/lowest_common_ancestor/), [`leetcode/number_of_islands/`](../../leetcode/number_of_islands/) |
| **Bit manipulation & SIMD logic** | Bayer demosaicing math, pixel packing/unpacking (RAW10 / RAW12), circular shifts | Unpack MIPI RAW10 to 16-bit integers; `ROTL`/`ROTR` without shift UB; fast integer square root / fixed-point | [`code/mipi_raw10_unpack.md`](./code/mipi_raw10_unpack.md), [`code/bit_rotation.md`](./code/bit_rotation.md) |
| **Concurrency & synchronization** | Thread safety, producer–consumer, reader–writer locks, condition variables | Multi-threaded frame dispatcher (1 sensor thread → N consumer workers without starving); thread-safe LRU cache for frame buffers | [`concurrency/`](../../concurrency/), [`leetcode/lru_cache/`](../../leetcode/lru_cache/), [`interview_handwrite/lru_cache_raw_list.cpp`](../../interview_handwrite/lru_cache_raw_list.cpp) |

### Key system coding implementation patterns

#### 1. Lock-free SPSC frame queue (C++11/17 atomics)

- Use `std::atomic<size_t>` with `memory_order_acquire` / `memory_order_release` semantics.
- Pad read/write pointers with `alignas(64)` (cache-line alignment) to prevent false sharing between capture and processing threads.
- Capture thread = producer (write + release); ISP / CV thread = consumer (acquire + read).
- Know why SPSC is the right topology for a camera capture path, and when you would step up to MPMC ([`XR/25`](../Pico_vision/25-无锁MPMC队列与CAS.md)).

**Whiteboard checklist**

- [ ] Draw ring: `head` / `tail`, power-of-two size vs `N+1` empty-slot convention
- [ ] `push`: store payload, then `tail.store(next, memory_order_release)`
- [ ] `pop`: `head` load acquire, then read payload
- [ ] Explain why `relaxed` is wrong for the publishing store
- [ ] `alignas(64)` on head vs tail — false sharing with capture @ 30–90 fps

#### 2. Strided memory & pixel conversions

- Format conversions: YUV420sp (NV12 / NV21) → RGB; planar vs interleaved layouts.
- Always account for padding: `row_stride != width * bytes_per_pixel`.
- In-place crop / rotate must never assume contiguous `width × height` packing (ISP output is almost never tightly packed).

**Whiteboard checklist**

- [ ] NV12: Y plane `height × y_stride`, UV interleaved `height/2 × uv_stride`
- [ ] Crop rectangle `(x, y, w, h)` with arbitrary pitch — pointer math only, no copies if possible
- [ ] RAW10 unpack: 4 pixels in 5 bytes (`P0[9:2] | P1[9:2] | P2[9:2] | P3[9:2] | {P3[1:0],P2[1:0],P1[1:0],P0[1:0]}`)
- [ ] Why 16-bit dest (not 10-bit packed) for ISP / NPU consumers

### Coding drill list (do these cold)

- [ ] SPSC ring buffer in C++17 with acquire/release + cache-line padding
- [ ] Thread-safe bounded blocking queue (`unique_lock` + `condition_variable`) as the “locked” baseline
- [ ] Frame dispatcher: 1 producer, N workers, no starvation, drop-oldest vs drop-newest policy
- [ ] Thread-safe LRU of frame buffers (capacity in bytes, not just entry count)
- [ ] 2D strided memcpy / crop / 90° rotate
- [ ] NV12 → RGB (correct chroma siting; do not ignore stride)
- [ ] MIPI RAW10 → `uint16_t` unpack ([`code/mipi_raw10_unpack.md`](./code/mipi_raw10_unpack.md))
- [ ] Circular shift `rotl32` / `rotr32` without `>> 32` UB ([`code/bit_rotation.md`](./code/bit_rotation.md))
- [ ] `aligned_malloc` / `aligned_free` with hidden raw pointer ([`code/aligned_malloc.md`](./code/aligned_malloc.md))
- [ ] Endianness detect + `bswap32` ([`code/endianness.md`](./code/endianness.md))
- [ ] `memmove` with overlap ([`code/memmove.md`](./code/memmove.md))
- [ ] 3×3 box / Gaussian filter with stride and replicate pad ([`code/box_filter.md`](./code/box_filter.md))
- [ ] Histogram + CDF equalize ([`code/histogram.md`](./code/histogram.md))
- [ ] Topological sort of an ISP node DAG (detect cycles)
- [ ] Moving average / median of a timestamped exposure or FPS stream
- [ ] Longest substring / sliding window under a constraint (warm-up for streaming)

---

## 2. General system design

Focus on scalable, distributed, or real-time modular backend / device infrastructures. These are still “systems” questions — Meta expects you to name failure modes, rollout, and device constraints, not only service boxes.

### Common questions for client / systems engineers

#### Design an OTA firmware & calibration delivery system

Cover:

- Chunking, delta-updates, rollback
- Signature verification (chain of trust: image → partition → calibration blob)
- Device fleet tiering and canary rollouts
- Calibration is **not** generic firmware: per-unit / per-SKU / thermal-bin variants, version skew vs ISP binary

**Talk track**

1. Separate **firmware image** vs **calibration / golden data** vs **ML model blobs**.
2. Device reports SKU, OS, ISP FW rev, last-good calibration hash, thermal bin.
3. Canary by hardware rev + region + battery/thermal class; halt on crash / IQ-regressed KPIs.
4. Delta (bsdiff / courgette-style) for bandwidth; full image fallback; A/B slots for rollback.
5. Signed manifest; refuse mix-and-match of ISP binary + calibration that were never co-validated.

#### Design a cloud-assisted media backup & sync engine for smart glasses

Cover:

- Low-bandwidth edge triage
- Wi-Fi vs BLE opportunistic upload
- Chunked uploads with resume
- Server-side deduplication

**Talk track**

1. On-device: thumbnail / proxy encode, scene hash, privacy redaction **before** uplink.
2. Radio policy: BLE for metadata / presence; Wi-Fi for bulk; never stall capture path on network.
3. Chunk + content-addressed blobs; resume via byte-range / merkle; dedup across user devices.
4. Thermal: defer encode/upload when skin temp / SoC budget is hot; opportunistic when charging / in case.

#### Design a distributed telemetry & image-quality metric aggregator

Cover:

- Client-side logging throttling
- On-device outlier filtering (drop frames, 3A convergence failures)
- Batch streaming to ingestion pipeline

**Talk track**

1. Two planes: **always-on counters** (drops, SOF timeout, thermal) vs **sampled IQ** (blur, AE hunt, color).
2. Filter on device: do not upload every frame’s PSNR-like proxy; keep outliers + stratified sample.
3. Privacy: no raw pixels in default telemetry; opt-in debug with TTL and user consent.
4. Backend: device → edge buffer → Kafka-like ingest → per-SKU dashboards; alert on fleet regressions after OTA.

### System-design checklist

- [ ] Draw clients, radios, signing, storage, and rollback for OTA
- [ ] State bandwidth, battery, and privacy constraints before APIs
- [ ] Define SLIs: TTFF, drop rate, upload success, calibration apply success
- [ ] Canary + kill switch + last-known-good
- [ ] What happens offline, mid-update, and on CRC / signature failure

---

## 3. Camera domain system design (deep dive)

**四场景完整答题稿（推荐先读）：** [`camera_system_design/`](./camera_system_design/) — 5 步框架 + 多相机 SLAM 同步 + E2E ISP + 眼镜功耗/热 + AI-ISP 混合。

This is the decisive round. You will be evaluated on the modern camera stack:

**Sensor → MIPI → CSI-2 receiver → ISP → memory → NPU/GPU → display / encoder.**

```
[ Image Sensor (Bayer RAW) ]
              │ (MIPI CSI-2 / D-PHY / C-PHY)
              ▼
[ ISP Front-End (IFE / VFE) ] ── (Stats: AWB/AE/AF) ──► [ 3A Control Loop (ARM Core / DSP) ]
              │ (Linear RAW / Demosaic)
              ▼
[ ISP Back-End (BPS / IPE) ]  ──► [ Denoise (TNR / MFNR) + Color Correction / Tone Map ]
              │ (YUV420 / NV12)
      ┌───────┴───────────────────────┐
      ▼                               ▼
[ On-Device NPU / DSP ]       [ Video Encoder / VPU ] ──► [ Display / Cloud Sync ]
(CV / Hand Tracking / VIO)     (H.264 / HEVC / AV1)
```

Related deep notes: [`camera/qualcomm_senior_staff_camera_domain_guide.md`](../../camera/qualcomm_senior_staff_camera_domain_guide.md), [`camera/sensor.md`](../../camera/sensor.md), [`camera/3A.md`](../../camera/3A.md), [`XR/08-影像ISP题详解.md`](../Pico_vision/08-影像ISP题详解.md).

### Essential architecture topics & Meta AR / wearable tradeoffs

#### 1. End-to-end glass-to-glass pipeline & latency

- **Passthrough / VIO (visual-inertial odometry):** sub-15 ms motion-to-photon latency requirement.
- **Zero-copy memory management:** Linux `dma-buf`, Android Gralloc, Ion allocators — no CPU copies on ISP → NPU → Display.
- **Hardware synchronization:** sensor external trigger pins, Genlock for stereo pairs, hardware timestamping against the IMU clock domain (drift compensation).

**Talk track**

| Path | Typical budget | What you cut |
| --- | --- | --- |
| VIO / tracking CV | Always-on, low res, fixed exposure, skip 3A beauty | Stats + lightweight FE only |
| Passthrough | Sub-15 ms photon-to-photon | Bypass heavy TNR/MFNR; display-direct / GPU warp |
| User media (photo/video) | Quality > latency | Full BE, HDR, encode |

Fence-based handoff: ISP writes dma-buf → signals fence → NPU / compositor waits — never `memcpy` a 4K NV12.

#### 2. ISP & 3A control loop

**Pipeline stages (draw this from memory):**

Black Level Correction (BLC) → Lens Shading Correction (LSC) → Demosaicing (Bayer → RGB) → Color Correction Matrix (CCM) → Gamma / Tone Curve → Temporal / Spatial Noise Reduction (TNR / SNR) → Edge Sharpening.

**Dual-pipeline strategy (wearables):**

- Dedicated **low-power, fixed-exposure CV stream** for tracking / VIO
- Parallel **HDR, high-resolution stream** for user media capture

3A lives on ARM / DSP from FE stats (AWB / AE / AF), **not** on the full BE YUV. Closed loop must be stable under duty-cycling and thermal throttling.

#### 3. Power, thermal & form factor (smart glasses / AR)

- **Thermal envelope:** strict dissipation (< 1.5–2.5 W total system power) so frames do not heat against the user’s face.
- **Duty cycling & wake-on-motion:** power-gate sensors and processing blocks when stationary; ultra-low-power CV for wake / gesture.

**Talk track:** cascade wake (IMU / ULP CV → FE-only tracking → full ISP + encode). Never leave IPE + encoder + NPU at max clocks for passthrough. Same constraint language as [`smart-glasses-ai-runtime.md`](../../company/openai/smart-glasses-ai-runtime.md).

#### 4. Sensor calibration & characterization

- **Factory vs run-time:** intrinsics / extrinsics (Zhang’s method), stereo baseline alignment, dynamic thermal calibration for sensor drift.
- **Artifacts:** rolling-shutter skew compensation, flicker reduction (50 Hz / 60 Hz detection), bad pixel correction (BPC).

Stereo + IMU: factory extrinsics are the prior; runtime thermal expansion needs a cheap delta model, not a full recalibration every boot.

### Domain-design checklist (whiteboard in < 3 minutes)

- [ ] Photon → photodiode → analog gain → ADC → MIPI CSI-2 → CSIPHY/CSID → IFE → DDR → BPS/IPE → YUV
- [ ] Stats tap vs pixel tap; 3A on DSP; dual pipeline for CV vs media
- [ ] Zero-copy: dma-buf / Gralloc / Ion + fences
- [ ] Stereo Genlock + IMU timebase / drift
- [ ] Latency budget vs thermal budget vs IQ — say the numbers
- [ ] Failure modes: MIPI CRC / packet drop, SOF timeout, 3A hunt, cache coherency on DMA, rolling shutter + head motion

---

## 4. Behavioral rounds (Meta core values & leadership)

Meta assesses seniority, driving impact through ambiguity, cross-functional collaboration (HW, FW, Algorithms, Product), and navigating architectural trade-offs.

Structure every response with **STAR** (Situation, Task, Action, Result). Prefer **metrics**: ms, mW, drop rate, PSNR/SSIM, crash-free sessions.

### Top prompts & strategic focus

1. **Driving impact in ambiguous technical spaces**
   - *Question:* “Describe a time when requirements were unclear or changing rapidly in a hardware/software project.”
   - *Focus:* technical north star, automated test harnesses / simulation benchmarks, decoupled dependencies, incremental milestones.

2. **Cross-functional conflict & hardware/software trade-offs**
   - *Question:* “Tell me about a disagreement with a hardware, algorithm, or product team regarding specs (e.g. latency vs image quality vs battery life).”
   - *Focus:* empirical data (profiling traces, PSNR/SSIM, power-meter measurements) and co-optimization, not opinion.

3. **Post-mortem & deep-rooted debugging**
   - *Question:* “Tell me about the hardest bug you tracked down in an embedded or camera stack.”
   - *Focus:* systematic RCA (buffer-queue races, MIPI packet drops, DMA cache coherency, 3A hunting); long-term architectural fix over a one-line patch.

### Anchor-story slots (prepare 4–5)

| # | Theme | Evidence to have ready |
| --- | --- | --- |
| 1 | Leadership / north star under ambiguity | Spec freeze points, milestone chart, what you cut |
| 2 | Severe production / field bug | RCA tree, repro, dashboards, permanent fix |
| 3 | HW–SW compromise | Latency vs IQ vs battery numbers both sides accepted |
| 4 | Cross-functional alignment | HW / FW / algo / product — who owned what |
| 5 | (Optional) Mentorship or raising the engineering bar | Test harness, IQ lab, bring-up playbook |

Related: [`behavior/`](../../behavior/).

---

## 5. Interview week execution checklist

- [ ] **Review C++17/20 concurrency primitives:** `std::unique_lock`, `std::condition_variable`, `std::atomic`, memory fences, pointer arithmetic
- [ ] **Diagram the ISP pipeline from memory:** photon capture → display/encode on a whiteboard in under 3 minutes
- [ ] **Prepare 4–5 STAR anchor stories:** leadership, severe production bug, hardware–software compromise, cross-functional alignment
- [ ] **Hand-write SPSC + strided crop + RAW10 unpack** without looking at notes
- [ ] **Rehearse dual-pipeline + sub-15 ms passthrough + 1.5–2.5 W glasses** as one connected design
- [ ] **OTA / glasses sync / telemetry:** one page of boxes each, including failure and rollback
- [ ] **Sleep and voice:** first 60 seconds of domain design = pipeline drawing + latency/power numbers, then dive

---

## Quick “opening sentences” (optional)

**Coding (SPSC):**  
> Capture is a single producer; the ISP/CV consumer is a single consumer. SPSC lets us publish frames with acquire/release and `alignas(64)` so the 90 fps write pointer does not false-share with the reader.

**Domain design:**  
> I split the stack into a low-power, fixed-exposure CV/VIO pipeline and a quality media pipeline, sharing the sensor over CSI but not sharing the heavy BE. Glass-to-glass for passthrough is a fence-based dma-buf path with a sub-15 ms budget; the thermal cap is about 1.5–2.5 W, so IPE/encoder/NPU are duty-cycled, not always-on.

**Behavioral (trade-off):**  
> We treated latency, IQ, and thermals as a measured Pareto, not a debate. Power meter + traces + SSIM on the same clips; we shipped the knee that kept motion-to-photon and skin temperature in spec.
