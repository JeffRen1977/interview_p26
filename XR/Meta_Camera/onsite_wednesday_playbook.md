# Meta Camera Onsite — Wednesday Playbook

**When:** Wednesday 2026-08-26 (today is Monday 8/24 → ~36 hours of prep)  
**Team:** Camera Software / Reality Labs — 0-to-1 camera stack at the intersection of AI and AR/VR  
**Loop:** 2× AI-Native Coding · 2× Camera In-Domain Design · 1× Behavioral  
**Sources:** [`meta_document/AI-Native SWE - Full Loop Candidate Prep.pdf`](./meta_document/AI-Native%20SWE%20-%20Full%20Loop%20Candidate%20Prep.pdf) · [`meta_document/Camera Systems IDD.pdf`](./meta_document/Camera%20Systems%20IDD.pdf) · [`meta_document/Specialist SWE FL Prep.pdf`](./meta_document/Specialist%20SWE%20FL%20Prep.pdf)

This is the **interview-week operating doc**. Deep notes stay where they already are; this file is what you rehearse out loud.

---

## 0. What they are actually scoring

Official IDD prompt (memorize this sentence — it is the shape of both domain rounds):

> You will be asked to **solve one problem and design a component of a high-performance real-time engine** based on the requirements. You will be interviewing with a Meta SWE or engineering leader who has **domain expertise** in this area.

This is **not** “design Instagram” and **not** a LeetCode round. It is: pick a real-time camera engine, freeze requirements, draw the data plane vs control plane, compute a budget, deep-dive one component, then degrade it when it fails.

### Rubric (same four bars on both domain rounds)

| Bar | Hire signal | No-hire signal |
| --- | --- | --- |
| **Problem navigation** | You pin product, fps, latency, power, failure mode in 3 minutes and say what you will **not** design | You ask 15 clarifying questions and never start |
| **Solution design** | End-to-end working engine + 1–2 components in depth | A pile of module names with no data path |
| **Technical excellence** | Numbers, trade-offs, failure points, testability | “We’ll optimize later”; no fences, no N+2, no thermal |
| **Technical communication** | You drive; you absorb interviewer pushback into the design | Monologue, or freeze when they change a constraint |

### AI-Native coding rubric (five bars)

Problem Solving · Code Development & Understanding · Verification & Debugging · Technical Communication · **AI Usage**.

Required: use the CoderPad AI assistant. You do **not** get credit for “the model said so.” You get credit for **narrating the bug, pinning the contract, reviewing the diff, running tests yourself**.

### Behavioral rubric (AI-Native loop)

AI-Driven Impact · Continuous AI Learning · Partnering · Embracing Ambiguity · Communicating Effectively.

Specialist/RL overlay (same stories, different labels): Resolving Conflict · Growing Continuously · Driving Results.

---

## 1. 36-hour calendar

Do **not** open new papers. Rehearse out loud. Sleep wins over a seventh design scenario.

### Monday night (today) — 2.5 to 3 hours

| Block | Time | What |
| --- | --- | --- |
| A | 45 min | Timed AI-native run of **one** project you have not looked at recently (`ttl_kv_index` or `ratelimiter_engine`). English narration. Every phase: run tests + commit. Playbook: [`AI_native_coding/playbook.md`](./AI_native_coding/playbook.md) |
| B | 60 min | Domain: draw E2E + ISP + HAL3 from memory on paper. Then **one** full 25-min mock: *glasses capture engine* **or** *passthrough*. Script: §3 + [`camera_system_design/README.md`](./camera_system_design/README.md) |
| C | 30 min | Your 3A (AF) depth: PDAF vs CDAF, N+2, hunting, Dual Pixel. English. §3.4 |
| D | 20 min | STAR cards §5 — speak each in 90 seconds with numbers |

### Tuesday — full day

| Block | Time | What |
| --- | --- | --- |
| AM-1 | 60 min | Second AI-native timed run (`maze_solver` or `max_unique_chars`). Force Phase 3: **say complexity before optimizing** |
| AM-2 | 90 min | Domain mocks × 2 (timer 40 min each): (1) **multi-cam SLAM sync** [`01`](./camera_system_design/01-multi-camera-slam-sync.md) (2) **power/thermal glasses** [`03`](./camera_system_design/03-smart-glasses-power-thermal.md). After each: recompute bandwidth + latency + watts on a changed fps |
| Lunch | 20 min | CoderPad practice link in Career Profile. Confirm Zoom screen share. Disable virtual background |
| PM-1 | 45 min | Android stack + V4L2 + interrupt/mutex flashcards §3.3 + §3.6 + §3.7. Then debug drill: *“preview drops 1 in 100 frames”* from [`additional_questions.md`](./additional_questions.md) §3 |
| PM-2 | 40 min | Video encode + memory/thermals §3.5 + §3.8. One mock: *real-time 1080p30 encode component under 2 W* |
| PM-3 | 45 min | Behavioral: 6 STAR stories + “Why Meta Camera” + 4 questions per interviewer type. Record yourself once |
| Evening | 30 min | **Whiteboard cheat sheet only** (§7). No new content. Phone on DND after 22:00 |

### Wednesday morning

- 20 min: redraw E2E pipeline once, say the opening 60 seconds once
- 10 min: skim STAR titles, not the essays
- Eat. Water. Headset. Zoom screen-share tested
- Do **not** cram MIPI lane rates in the lobby

---

## 2. Round type A — AI-Native Coding (2 × 60 min)

Official format: **one thematic problem, 3–4 checkpoints, 60 minutes, CoderPad with AI + Run Tests**. Languages include Python / C++ / Java / … Confirm with recruiter; default Python unless they say C++.

You already have the muscle memory: [`AI_native_coding/playbook.md`](./AI_native_coding/playbook.md) · drills in [`AI_native_coding/`](./AI_native_coding/).

### Time box (do not renegotiate this in the room)

| Clock | Action | Output |
| --- | --- | --- |
| 0:00–0:03 | `ls`, read tests, **run existing tests** | Know the baseline red |
| 0:03–0:10 | Read models + public API. Speak the contract | One-sentence API |
| 0:10–0:18 | Phase 1: **narrate root cause**, then minimal AI fix | Baseline green |
| 0:18–0:35 | Phase 2: algorithm choice out loud, pinned signature | Feature green |
| 0:35–0:52 | Phase 3: complexity first, then rewrite; keep naive for differential test | Scale tests green |
| 0:52–0:58 | Add 2–3 edge tests **you** write | Empty / duplicate / boundary |
| 0:58–1:00 | Recap trade-offs + questions | — |

### Prompt pattern (always four parts)

`Task + Constraints + Do-NOT + Output-format`

Never: “fix all failing tests” or “implement Phase 2.”  
Always: exact signature, files you will not touch, paste **only** the failing traceback.

### What interviewers write down (speak these)

- “Tests are the spec; I want the contract before I write.”
- “Root cause is X, not Y, because of this one comparison / missing visited / TTL stored as absolute time.”
- “I’ll keep the naive version and add a fast path so I can differential-test.”
- “The model added a deepcopy I don’t need — deleting it.”
- “This is O(2^n); with N=24 that’s why it hangs. Upper-bound prune + bitmask.”

### Hard fail actions

- Changing tests to go green (unless interviewer agrees a test is wrong **and** you showed evidence)
- Letting AI rewrite the whole file
- Silent typing for 10 minutes
- Not running tests after a phase
- Using `time.time()` when tests inject a clock

### If you stall

Say the next checkpoint verbally and ask: “I’ll land a correct O(n²) and then optimize — is that the right split?” Working incomplete Phase 3 with a stated plan beats a broken rewrite.

---

## 3. Round type B — Camera In-Domain Design (2 × ~45 min)

Whiteboard: Excalidraw (no AI) **or** CoderPad Mermaid. Drive like a kickoff with a partner: **you** propose the engine, they stress it.

### 45-minute script (use every time)

| Min | Do | Do not |
| --- | --- | --- |
| 0–5 | Clarify + **assumption table** + “three things I will prove” | Interview the interviewer |
| 5–12 | High-level: **data plane vs control plane** | Draw every ISP block |
| 12–20 | **Compute** bandwidth, latency, power, memory | Quote magnitudes without adding |
| 20–32 | Deep-dive **one** component (they pick, or you recommend) | Five shallow subsystems |
| 32–38 | Failure ladder + hysteresis | “We’ll handle errors” |
| 38–42 | How we **prove it** (rig, replay, fleet SLI) | Skip validation |
| 42–45 | Your questions | Empty close |

**Opening 60 seconds (adapt the nouns, keep the structure):**

> I’ll split this into three pieces: (1) the pixel data path, (2) the closed-loop control path, (3) the constraints — latency, power, thermal. I’ll deep-dive (2) and (3) because that’s where systems fail. Assumptions: [table]. I want to prove three things: end-to-end delay fits in X ms, peak DDR stays under Y GB/s, and every block has a degraded-but-alive state.

### Default assumption table (say it, let them correct it)

| | Smart glasses | VR headset |
| --- | --- | --- |
| Cameras | 1 media RS 12MP + optional ULP CV | 4–8 GS tracking + optional passthrough pair |
| Rate | 1080p30 record / QVGA 10–30 ULP | Tracking 640×480 @ 60–90; passthrough 60–90 locked to display |
| Latency | Preview <30 ms; wake <100–200 ms | Pose <5–10 ms; photon-to-photon <15–20 ms |
| Power / thermal | **1.5–2.5 W peak**; skin ~43 °C | Wider SoC, still junction + fan policy |
| Memory | 2–4 GB shared LPDDR; **no hot-path memcpy** | Still no 4K NV12 CPU copy |

Then ask: **is this a tracking problem, an IQ/media problem, or a power problem?** Only deep-dive that axis.

### Whiteboard arithmetic (compute live; do not recite)

```
MB/s = W × H × bytes/px × fps
```

| Format | bytes/px |
| --- | --- |
| RAW8 | 1.0 |
| RAW10 packed | 1.25 |
| RAW10 unpacked u16 | 2.0 |
| NV12 | 1.5 |
| RGB888 | 3.0 |

**Count every DDR pass.** ISP write + encoder read = 2×. TNR also reads the previous frame = 3×.

Anchors: **1080p30 NV12 = 93 MB/s** per pass · **4K30 NV12 = 373 MB/s** · DDR energy **~50–150 mW per GB/s**.

MIPI: `payload Gbps = W×H×bits×fps×1e-9`, lane rate ≈ payload × 1.15 / lanes.

Latency ledger (add line by line):

```
exposure midpoint → SOF          exposure/2
SOF → EOF                        ~1/fps  (often the largest term)
EOF → DMA + fence                0.1–0.5 ms
ISP through                      0.5–5 ms (FE-only vs full BE)
algo / warp                      2–10 ms
display 1 vsync + scan           16.7 ms @60 / 11.1 ms @90
```

Thermal: `ΔT = P × Rth`. Glasses temple ~15–25 °C/W, τ ~ minutes → **burst is allowed, chatter is not**. Hysteresis in tens of seconds.

Full table: [`camera_system_design/README.md`](./camera_system_design/README.md).

---

### 3.1 E2E camera system — the picture you must draw in <3 min

```
[Sensor Bayer / GS RAW]
        │ MIPI CSI-2 (D-PHY / C-PHY)
        ▼
 CSIPHY (electrical lock) → CSID (SOF/EOF, VC, DT, CRC)
        ▼
 IFE / ISP front-end
        ├─ pixels: BLC → LSC → BPC → (Bayer NR) → demosaic?
        ├─ stats:  BG / BHIST / BF  ──► 3A on DSP/ARM ──► CCI back to sensor (N+2)
        └─ dma-buf + fence  (CPU does not copy)
                │
     ┌──────────┼──────────┐
     ▼          ▼          ▼
 Display/GPU  Encoder    NPU / SLAM
```

**One sentence:** control plane (CCI, 3A, stream on/off) is not the data plane (MIPI → DMA). Pixels never go through the CPU on the hot path.

Wearables dual pipeline (say this early on glasses/Quest):

| Path | Policy | What you cut |
| --- | --- | --- |
| Tracking / VIO | Always-on, low res, **fixed or slow AE**, GS | Beauty, TNR, full 3A hunt |
| Passthrough | Photon-to-photon <15–20 ms | Heavy NR; display-direct + warp |
| User media | Quality > latency | Full BE, HDR, encode |

Likely prompt: *design the capture engine for Ray-Ban-class glasses* or *design the camera subsystem for Quest passthrough + tracking*. Full answers: [`02-e2e-isp-pipeline.md`](./camera_system_design/02-e2e-isp-pipeline.md) · [`01-multi-camera-slam-sync.md`](./camera_system_design/01-multi-camera-slam-sync.md) · [`06-passthrough-reprojection.md`](./camera_system_design/06-passthrough-reprojection.md).

**Your close:** “ISP is a hardware dataflow with a stats tap. Software only schedules requests and buffers. 3A is a delayed feedback loop, not a filter on the current frame.”

---

### 3.2 ISP — stages, why order, what you skip

Draw this order and name **purpose + failure**:

| Stage | Why | If skipped |
| --- | --- | --- |
| BLC | Remove pedestal / dark current | AE/AWB baseline wrong; gray blacks |
| LSC | Optical vignetting + color shading | Dark/colored corners |
| BPC | Static OTP + dynamic outliers | Demosaic turns dots into colored blobs |
| (Bayer NR) | Denoise before interpolation | Noise amplified |
| **Stats tap** | Linear domain after LSC, before heavy tonemap | 3A meters a beautified image |
| AWB gains | Bayer R/B scale | Color cast |
| Demosaic | Bayer → RGB | Zipper / false color |
| CCM | Sensor RGB → sRGB/P3 | Wrong hues, CFA crosstalk left in |
| Gamma / tone | Linear HDR → display | Crushed or noisy shadows |
| TNR / spatial NR / sharpen | Video vs preview | Ghosting vs crunchy noise |

Qualcomm-shaped hardware map (fine to use as a vocabulary, say “or equivalent”): **IFE** (FE + stats) → **BPS** (full-size still) → **IPE** (YUV, TNR, scale). Tracking = FE-only.

Stats live on **IFE**, not on the final pretty YUV.

---

### 3.3 Android camera stack — the contract, not the class names

```
App / CameraX / Camera2     Surface + CaptureRequest
        │ Binder
CameraService               session, arbitration, callbacks
        │ HIDL/AIDL
HAL3                        configureStreams / processCaptureRequest / Result
                            buffer handle + sync fence + metadata = one frame
        │
OEM graph (CamX/Chi-like)   compile request → HW nodes
        │ V4L2 + dma-buf fd
Kernel                      CSI + ISP + SMMU; CCI to sensor
```

**HAL3 invariant:** every Request eventually produces a Result with **the metadata that actually applied** (exposure, gain, timestamp). Results may be partial and slightly reordered; a hole in frame numbers is a bug.

**Never block in `processCaptureRequest`.** Queue and return. Blocking turns a pipeline into a stop-and-wait preview.

Buffer story: Gralloc allocates → fd to HAL → ISP DMA writes → **release fence** back to SurfaceFlinger / encoder. App holding buffers = drop. Depth ~3–8; “ZSL ring stores RAW + **that frame’s** 3A metadata, not the live preview 3A.”

Arbitration (glasses + phone apps + always-on CV): preempt by priority; **the long tail is 3A reconverge**, not the ioctl. Privacy LED must be **hardware-tied to streamon**. Design: [`07-camera-arbitration-privacy.md`](./camera_system_design/07-camera-arbitration-privacy.md).

Detail notes: [`camera/android_framework.md`](../../camera/android_framework.md).

---

### 3.4 One of the 3As — lead with AF (your depth), be fluent in AE/AWB

3A is a **delayed MIMO controller**. Stats from frame N, actuators latch on **N+1 / N+2**. If you treat it as “this frame’s filter,” you will hunt.

```
IFE stats (frame N) → 3A (DSP)
        → exposure / gain / CCM / VCM
        → CCI group-hold write
        → takes effect ~ frame N+2
```

#### AF (go deep here — Pixel / Dual Pixel / PDAF / MLPD)

| Mode | What | Weakness |
| --- | --- | --- |
| CDAF | Maximize contrast | Slow, hunt (“breathing”) |
| PDAF | Phase → defocus **sign and magnitude** | Needs calibration; fails low texture / low light |
| Dual Pixel | Dense phase over the array | Pixel-class; still needs fine CDAF sometimes |
| Laser/ToF | Active range | Near/dark only |
| Hybrid | PDAF coarse + CDAF fine | Phone default |

State machine (Android): `INACTIVE / PASSIVE_SCAN / ACTIVE_SCAN / FOCUSED_LOCKED / NOT_FOCUSED_LOCKED`.

**Talk track you can own:**

- PDAF is a **calibrated estimator**, not a truth sensor. Module tilt, remosaic, new CFA → the disparity-to-DAC map changes. You shipped calibration process + failure RCA with TechEng.
- Temporal filter (P24) kills frame-to-frame DAC jitter in complex lighting — same idea as AE IIR: **rate-limit the actuator**.
- Hardware PDAF (die-size cut ~47% in your story) moves correlation off the CPU — glasses **must** do this or AF is unaffordable.
- Glasses often **fixed-focus + large DoF**; say that out loud so you don’t invent a VCM that isn’t there. Tracking cameras: **fixed exposure**, no hunt.

Hunting debug: loop gain too high, **wrong delay model** (commanding N+1 when hardware is N+2), or ROI bouncing. Fix: IIR + max step + hysteresis + hold on low confidence.

#### AE (competent)

Actuators: exposure time, analog gain, digital gain (sensor and/or ISP).  
`Ymeas` from Bayer grid / histogram → dynamic `Ytarget(lux, highlight, face)` → `Pnext = Pcur × (Ytarget/Ymeas)` through IIR → split `(t, again, dgain)` via an exposure table.

Video: **lock fps first** (exposure ≤ 1/fps), then gain. Flicker: snap t to 1/100 or 1/120. Night stills: lower Ytarget, leave lift to fusion — don’t crank gain to middle-gray.

#### AWB (competent)

Gray world fails on large saturated colors. Real path: **gamut gate → project to Planckian locus → CCT → R/B gains + CCM interpolate**. Smooth in time; skin/face constraint (Real Tone story is allowed as product taste, not as a claim you invented AWB).

Gains = diagonal Bayer scale (illuminant). CCM = 3×3 in RGB (CFA crosstalk + color space).

Full math: [`camera/3A.md`](../../camera/3A.md). Design hook: [`02`](./camera_system_design/02-e2e-isp-pipeline.md) §3.2.

---

### 3.5 Video encode / decode — treat it as a real-time engine component

Likely component: *1080p30 (or 4K) realtime encoder that cannot stall the ISP*.

```
IPE YUV (UBWC NV12 dma-buf)
        │ import fd, wait ISP fence
        ▼
 VPU / HW encoder (H.264 / HEVC)
        │ output bitstream buffers (another pool)
        ▼
 muxer (MP4)  ─ or ─  passthrough skip encode entirely
```

Design points (this is what they want, not CABAC trivia):

| Topic | What to say |
| --- | --- |
| Zero-copy | Encoder **imports** the same fd. CPU never sees 1080p. |
| Backpressure | If encoder queue fills, **drop or skip encode**, do not block IFE thread. Preview can survive; freeze cannot. |
| Rate control | CBR/VBR for upload; CQP-like for local archive. Under thermal, drop fps or resolution **before** cranking QP into mush — and write true PTS. |
| GOP | IPPP for low delay; long GOP for compression. Glasses live: short GOP or periodic IDR so a thermal restart doesn’t black-screen the file. |
| Latency | Encode is **not** on the passthrough path. Media record can be 3–8 frames of pipeline delay. Never put HEVC on the photon-to-photon budget. |
| Audio A/V | Shared SoC clock; video PTS = SOF or exposure midpoint (pick one and be consistent); audio from DSP. Drift: stretch audio, don’t drop video randomly. |
| Decode | Passthrough/MR may decode nothing (live). Playback / streaming is a **different** engine: decode → GPU warp. Don’t mix the two in one diagram. |
| Format | NV12 limited range (16–235) is what H.264 expects. Full-range RGB is a bug. |

Thermal interaction: at T2, **keep IFE+VPU clocks**, cut NPU/CPU. Encoder GOP simplify at T3. See [`03`](./camera_system_design/03-smart-glasses-power-thermal.md).

---

### 3.6 Linux kernel driver — V4L2 as the real-time capture engine

Likely component: *the kernel capture path for one CSI camera*.

```
sensor subdev  →  CSI PHY/CSID subdev  →  ISP subdev  →  /dev/video*  (vb2)
     CCI/I2C           settle, lanes           crop/fmt         QBUF / DQBUF
```

| Concept | Line to say |
| --- | --- |
| Media controller | Pads + links; `media-ctl` builds the graph. Usecase switch = relink, not memcpy. |
| subdev vs video node | Subdev: format/control, no buffers. Video node: buffer endpoint. |
| Memory | `MMAP` / `USERPTR` / **`DMABUF`**. Production camera is DMABUF. |
| Probe | DT: I2C, clocks, reset GPIO, regulators, lane map, endpoints. Power seq → chip ID → register subdev. |
| Group hold | Exposure/gain batched so they apply on the **same** frame. |
| IRQ | Top half: read status, **clear IRQ**, stamp SOF. Bottom: threaded IRQ / workqueue for buffer done. **No mutex sleep, no malloc, no printf in ISR.** |
| Cache | CPU write → device: **clean**. Device write → CPU: **invalidate**. Miss this = torn frames. |
| Debug | `dmesg`, `media-ctl -p`, CSID CRC/overflow counters, `ftrace`, scope on MIPI / FSYNC. |

CSIPHY vs CSID: PHY = electrical lock / deserialize. CSID = packets, VC, DT, CRC. Wrong layer = wasted debug. [`camera/sensor.md`](../../camera/sensor.md) · [`camera/camera_driver.md`](../../camera/camera_driver.md).

---

### 3.7 OS concepts — always in camera language

| Concept | Camera mapping |
| --- | --- |
| Process vs thread | `cameraserver` is a process (binder, permissions). Capture, 3A, encoder are threads (or HW). Crash isolation: encoder process dying must not kill preview. |
| Mutex | Protects **control** state (session graph). **Never** hold a mutex across DQBUF wait on the hot path — priority inversion → drops. |
| condvar | `while (!ready)` — spurious wakeups. Timeout = drop-oldest vs drop-newest policy. |
| Lock-free SPSC | Capture thread → ISP/CV thread. `payload` then `release` store of index; consumer `acquire`. `alignas(64)` head vs tail. |
| Interrupt | SOF timestamp in ISR (timebase). Buffer-done in thread. |
| Exceptions | Real-time path: **error codes**, not C++ exceptions (unbounded unwind). Kernel: `IS_ERR` / `goto err`. |
| Priority | Preview/IFE > encoder > telemetry. Watchdog: if 3A thread wedges, keep last-good exposure. |
| `volatile` vs `atomic` | `volatile` = MMIO. Cross-thread = `std::atomic`. Using volatile for sync is wrong. |

Flashcards: [`additional_questions.md`](./additional_questions.md) §2.

---

### 3.8 Power, memory, thermals — the glasses product constraint

**Architecture is a thermal state machine.** ISP is only the actuator for S2/S3.

```
S0 Sleep      AON IMU/voice          ~1–10 mW
S1 ULP vision low-res detect         tens of mW
S2 Peek       short preview/capture  <1 W, hundreds of ms
S3 Record     1080p30 + encode       near the wall
S4 Throttle   720p15 / NPU off       hold skin temp
```

S3 ↮ S0 while the user is recording. Hysteresis S3↔S4.

Memory:

- Fixed **frame pool**, not malloc. Refcount: one frame, N consumers (preview, encode, CV).
- dma-buf / Gralloc / Ion heaps; SMMU maps into each IP.
- Ring depth 3. Analysis stream 320×240 to NPU, **not** 1080p.
- UBWC/AFBC if both IPE and encoder speak it (~0.7× bytes).
- Forbidden: preview RGB888, CPU memcpy of 1080p.

Throttle order: **fps first** (no `configureStreams` flash, 3A stays), **then** resolution, **then** stop record. IQ team owes a NR table per rung so the image doesn’t suddenly go crunchy.

Energy: 500 mAh × 3.7 V = **1.85 Wh**. 2 W continuous ≈ **55 min**. Answer with a **mix model** (N photos + M minutes video + all-day S0), not a single number.

Full design: [`03-smart-glasses-power-thermal.md`](./camera_system_design/03-smart-glasses-power-thermal.md).

---

### 3.9 Likely IDD prompts (pick a component, don’t boil the ocean)

| Prompt they might say | Component to deep-dive | Notes |
| --- | --- | --- |
| High-perf realtime capture engine for glasses | Buffer/fence + thermal SM | §3.1 + §3.8 |
| Multi-camera SLAM / controller tracking | FSYNC + IMU–camera time | [`01`](./camera_system_design/01-multi-camera-slam-sync.md), [`code/timestamp_sync.md`](./code/timestamp_sync.md) |
| Passthrough / EIS | Reprojection vs timewarp | [`06`](./camera_system_design/06-passthrough-reprojection.md) — **safety: never black** |
| 3A for video or AF for dual-pixel | Delayed control loop | Your home turf |
| Video record 1080p30 under 2 W | Encoder backpressure + QoS | §3.5 |
| Android HAL3 session for 2 clients | Arbitration + privacy LED | [`07`](./camera_system_design/07-camera-arbitration-privacy.md) |
| Kernel CSI bring-up | IRQ, vb2, cache, group hold | §3.6 |
| AI + classical ISP | Preview HW fast path vs NPU still | [`04`](./camera_system_design/04-ai-isp-hybrid.md) |
| Eye / face tracking | Duty cycle vs GPU save | [`05`](./camera_system_design/05-eye-face-tracking.md) |
| Calibration or validation infra | Version-bind IQ ↔ FW; bit-exact replay | [`08`](./camera_system_design/08-calibration-system.md), [`09`](./camera_system_design/09-validation-infrastructure.md) |

**E6 tell:** “Here’s how I’d know this is wrong on 100k devices” — counters per layer, bit-exact replay, fleet SLI (drop rate, SOF timeout, AE hunt, skin temp).

---

### 3.10 Debug one-pager (they will ask this as a design or a pop quiz)

*“Preview drops 1 of every 100 frames. How do you find it?”*

Count frames **per layer** until the first mismatch: sensor SOF → CSID SOF/CRC → IFE done → HAL Result numbers → app Surface → compositor.

| Layer | Typical cause |
| --- | --- |
| Sensor | Exposure > frame period; wrong mode |
| MIPI | CRC, settle, **CSID overflow** |
| DDR | Encoder + display + ISP collide |
| Pool | Consumer holds buffers |
| Scheduler | Lock / log thread inverted priority — **Perfetto** |
| Thermal | DVFS after minutes |

1/100 periodic → a timer (3A, telemetry, GC) beating the frame clock. Observe with ftrace, **not printf** (Heisenbug).

More: [`additional_questions.md`](./additional_questions.md) §3 · [`camera/掉帧.md`](../../camera/掉帧.md).

---

## 4. Round type C — Behavioral (45 min)

STAR. **Last 2–3 years.** Concrete. Numbers. “I” not “we” on **your** actions. Meta wants honesty on failure.

### Story board (map many questions onto 6 stories)

| # | Story | Signals | Numbers to say |
| --- | --- | --- | --- |
| 1 | **PDAF multi-year roadmap** (HWPD, MLPD, DDM, infra) | Ambiguity, driving results, technical vision | P25–P28 pillars; HW offload for power; 40–100× zoom / low light |
| 2 | **HW PDAF area −47% die** without IQ regression | Impact, trade-off, partnering with silicon | 47% area; IQ held; what you cut / verified |
| 3 | **MLPD + LSTM** vs TechEng disagreement | Conflict, data over opinion, AI-driven impact | Experiment metric (temporal stability / defocus error); what would have changed your mind |
| 4 | **Real Tone** — PM / PCIQ / AF / 3A | Partnering, communication, IQ bugs to close | Test reqs you wrote; bugs filed; release bar met |
| 5 | **DXO#2 AF crash → RCA → DXO#3 recover** | Failure, ownership, move-fast vs quality | Score drop, bug, time to fix, what test you added so it can’t recur |
| 6 | **Remosaic IQ** full sweep PD RAW vs JPEG; **AF agent** + sim metadata | Debug, continuous learning, tools > heroics | Sweep, root cause class, simulator fields (PD RAW in metadata) |

**PDAF calibration under a new sensor format** is a backup for “impossible deadline / HW changed.” **AF agent vs human judgment** is the honest failure (didn’t fully replace expert eyes; you narrowed the gap and built infra).

### Question → story cheat

| They ask | You tell |
| --- | --- |
| Unclear / changing requirements | Roadmap + what you **cut** (50MP remosaic not shippable) |
| Conflict with HW / algo / product | MLPD LSTM — experiments, not volume |
| Hardest bug | Remosaic / DXO#2 — systematic RCA |
| Biggest impact | −47% PDAF area **or** roadmap that other teams executed against |
| How you use AI | AF agent, MLPD, knowledge graph; **safety**: don’t ship unvalidated IQ; eval harness |
| Ambiguity / pivot | Rapid 2-week Android release after google3; bug triage agent |
| Mentorship | Design docs, onboarding, 1:1s, offline IQ tools |
| Move fast vs quality | What was **not** negotiable (safety of calibration, data correctness); debt you actually paid |

### Phrasing

- Situation: 2 sentences. Task: **your** goal. Action: 3–5 decisions **you** made. Result: metric + what you installed so it lasts (test, dashboard, process).
- AI-driven impact: “We put human IQ judgments into an agent + simulator so bring-up wasn’t a tribal queue. Eval stayed bit-exact replay, not vibes.”
- Failure: “AF agent didn’t match expert raters. We didn’t pretend. We scoped it to triage, added metadata for debug, and kept humans on the residual.”

### Why this role (30 seconds)

> I’ve spent years owning AF as a **real-time control + calibration + silicon** problem on Pixel, including PDAF hardware, ML defocus, and the messy cross-team path to ship. Meta Camera is 0-to-1 on glasses and VR, where the same loop is tighter: milliwatts, glass-to-glass, privacy. I want to build that engine, not tune a mature phone stack forever.

---

## 5. Logistics and Meta hygiene

From the official guides:

- Quiet room, reliable link, headset, **Zoom screen share enabled before the first loop**
- **No virtual/blurred background**, no unauthorized AI (only CoderPad’s assistant when they give it)
- Casual dress
- A few minutes at the end for **your** questions
- Recruiter: follow up if you haven’t heard in a week

**If you don’t know:** say so, map it to a neighbor you do know, ask a constraint. Domain IDD **intentionally** includes unfamiliar surface area.

---

## 6. Questions for them (pick 2 per round)

**Domain engineer**

- Is this role closer to **tracking data path**, **media/IQ**, or **framework/HAL**? Where is the boundary with sensor HW and algorithm?
- Android HAL3 vs custom Linux/RTOS on the product you ship next?
- Who owns sensor driver vs IQ vs 3A? What does a new-sensor bring-up look like in weeks?
- How is the **thermal/power budget** split across camera / display / NPU? Shared measurement?

**EM / leader**

- Glasses vs headset: how much camera code is actually shared?
- Painful technical debt on the current stack?
- How do you decide FE-only vs full ISP when product wants both passthrough and “pretty” capture?

**Behavioral**

- What does “move fast” mean when a camera OTA cannot roll back like a service?
- How is AI used in **your** team’s daily bring-up / IQ / debug, not just in the product?

---

## 7. Pocket cheat sheet (print or one screen Wednesday AM)

**Open domain:** assumption table → data vs control → prove latency / bandwidth / degrade.

**E2E:** Sensor → MIPI → CSIPHY/CSID → IFE (pixels + stats) → dma-buf/fence → display | encode | NPU.

**3A:** stats N → actuate N+2; AF = your mountain; hunting = delay model or gain.

**HAL3:** Request/Result/fence; never block request; ZSL metadata sticks to its RAW.

**Kernel:** ISR minimal; DMABUF; cache clean/invalidate; group hold.

**Encode:** import fd; never stall ISP; encode off passthrough budget.

**Thermal:** S0–S4; fps then res; hysteresis tens of seconds; 1.5–2.5 W glasses.

**Debug drops:** count SOF/CSID/IFE/HAL/app; 1/100 = periodic task.

**AI coding:** tests first, narrate bug, pin signature, run tests, review diff, complexity before rewrite.

**1080p30 NV12 = 93 MB/s/pass.** Readout ~ 1/fps dominates latency.

---

## 8. Anti-patterns (explicitly avoid)

| Don’t | Do |
| --- | --- |
| Recite Qualcomm register names | Physics + contracts (N+2, fence, DDR passes) |
| Design Instagram CDN in a camera IDD | Real-time engine component |
| Perfect system, no failures | Degrade ladder + how we detect on fleet |
| “Copy the buffer to the algorithm” | dma-buf + fence |
| Argue taste vs TechEng | Show the experiment that would change your mind |
| AI dumps a full rewrite | One function, you understand every line |
| Theoretical behavioral answers | STAR with a date, a metric, a scar |

---

## 9. File map (when you need depth)

| Need | Open |
| --- | --- |
| This loop | **This file** |
| 60-min AI-coding muscle | [`AI_native_coding/playbook.md`](./AI_native_coding/playbook.md) |
| Domain 45-min structure + arithmetic | [`camera_system_design/README.md`](./camera_system_design/README.md) |
| Ten scenarios | [`camera_system_design/`](./camera_system_design/) |
| 3A math | [`camera/3A.md`](../../camera/3A.md) |
| Android / HAL3 | [`camera/android_framework.md`](../../camera/android_framework.md) |
| Sensor / CSI | [`camera/sensor.md`](../../camera/sensor.md) |
| V4L2 / DMA | [`camera/camera_driver.md`](../../camera/camera_driver.md) |
| Frame drops | [`camera/掉帧.md`](../../camera/掉帧.md) · [`additional_questions.md`](./additional_questions.md) §3–4 |
| OS / ISR flashcards | [`additional_questions.md`](./additional_questions.md) §2 |
| STAR raw notes | [`behavior/行为.md`](../../behavior/行为.md) |
| Staff STAR (4 cards, speakable) | [`behavior/meta-staff-star.md`](../../behavior/meta-staff-star.md) |
| General (longer) prep list | [`camera_software_engineer_prep.md`](./camera_software_engineer_prep.md) |

You already have more notes than you can speak in five interviews. Wednesday is a **performance**: drive the problem, compute one budget, deep-dive one component, tell six stories with numbers.
