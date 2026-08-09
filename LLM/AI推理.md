# AI Inference Systems & Infrastructure Complete Interview Guide & Solutions

---

## Role & Technical Scope Overview
An **AI Inference Systems & Infrastructure Engineer** designs, optimizes, and scales production serving engines for Large Language Models (LLMs), Multimodal Models, and deep learning services. The goal is to maximize throughput (tokens/sec/dollar) while meeting strict Service Level Objectives (SLOs) for latency (Time-to-First-Token and Inter-Token Latency).

The core technical pillars evaluated in AI Inference interviews include:
1. **Serving Architectures & Scheduling**: Continuous / Iteration-level Batching (Orca), Chunked Prefill (Sarathi), and Disaggregated Prefill-Decode architectures (Splitwise / Mooncake).
2. **KV Cache Management & Attention Architectures**: PagedAttention (vLLM), RadixAttention / Prefix Caching (SGLang), and Attention Variants (MHA, MQA, GQA, and DeepSeek MLA).
3. **Latency Metrics & Acceleration Techniques**: TTFT, TPOT/ITL, Roofline Model for inference, Speculative Decoding (EAGLE / Medusa), and FlashDecoding (sequence parallel decode).
4. **Quantization & Kernel Optimization**: W4A16, W8A8, FP8 inference, CUDA Graphs (eliminating CPU kernel launch overhead), and TensorRT-LLM / vLLM internals.
5. **System Deployment & Scaling**: Triton Inference Server, Dynamic Batching, Prefix-aware Load Balancing, and Autoscaling policies.

---

# Part 1: Inference Serving Architectures & Request Scheduling

---

### Question 1: Continuous Batching (Iteration-Level Batching) vs. Static Batching
> **How does Continuous Batching (Iteration-Level Batching / Orca) work? Why is traditional static batching highly inefficient for LLM text generation?**

#### Solution:

```
Traditional Static Batching (Padded to Max Length / Waits for Slowest Request):
Req 1 (Short): [P][D][D][D]................ (Idles waiting for Req 2 & 3!)
Req 2 (Medium):[P][D][D][D][D][D][D]........ (Idles waiting for Req 3!)
Req 3 (Long):  [P][D][D][D][D][D][D][D][D][D][D][D] ===> Batch finishes together.
               |<------------- Massive GPU Waste / Bubble ------------->|

Continuous Batching (Iteration-Level Scheduling - vLLM / Orca):
Step 1: [Req 1: D1] [Req 2: D1] [Req 3: D1]
Step 2: [Req 1: D2] [Req 2: D2] [Req 3: D2]
Step 3: [Req 1: D3] [Req 2: D3] [Req 3: D3]  ==> Req 1 FINISHES!
Step 4: [Req 4: P ] [Req 2: D4] [Req 3: D4]  ==> Req 4 INJECTED IMMEDIATELY!
Step 5: [Req 4: D1] [Req 2: D5] [Req 3: D5]
```

#### 1. The Inefficiency of Static Batching in Autoregressive Models
* **Variable Input & Output Lengths**: Prompts range from 10 to 4,000+ tokens; responses range from 1 to 2,000+ tokens.
* **Early Completion Waste**: If Request 1 finishes in 10 tokens but Request 2 needs 500 tokens, static batching holds Request 1's memory and runs dummy computations over padding tokens until the entire batch finishes.
* **High Inter-Token Latency (ITL)**: New incoming requests are forced to wait in a queue until the current batch fully concludes.

#### 2. Continuous / Iteration-Level Batching Mechanics
* **Iteration-Level Granularity**: The scheduler operates at the granularity of a **single token generation step** (iteration) rather than a whole request lifecycle.
* **Dynamic Join & Exit**:
  * As soon as a sequence emits an `<EOS>` (End-of-Sequence) token or hits `max_tokens`, it is immediately evicted from the active batch, and its KV cache is freed.
  * A new waiting request is scheduled into the newly vacated batch slot on the very next token iteration without stopping other running sequences.
* **Impact**: Increases GPU hardware utilization and serving throughput by **$3\times - 8\times$** compared to static batching.

---

### Question 2: Chunked Prefill & Preventing TTFT-Induced Decode Bubbles
> **What is Chunked Prefill (Sarathi-Serve / vLLM v1)? How does mixing long prompt prefill with token decode cause severe Inter-Token Latency (ITL) jitter, and how does chunking fix it?**

#### Solution:

#### 1. The Interference Problem between Prefill and Decode
* **Prefill Phase**: Compute-bound GEMM with high FLOPs (e.g., a 4,000-token prompt takes $150\text{ms}$ on an H100).
* **Decode Phase**: Memory-bandwidth bound GEMV with tiny FLOPs (e.g., 1 token step takes $15\text{ms}$).
* **Problem (ITL Jitter / Stuttering)**: If the engine co-schedules a long 4,000-token prefill in the same iteration as ongoing decode requests, the decode tokens are delayed by $150\text{ms}$. Users perceive this as a massive, visible lag in text streaming.

#### 2. Chunked Prefill Mechanism
```
Unchunked (Huge Latency Spike):
Iteration N: [ Decode 1 ] [ Decode 2 ] [ Prefill 4000 tokens (150ms) ]  ===> Decode delayed by 150ms!

Chunked Prefill (Budget = 512 tokens/step):
Step 1: [ Decode 1 ] [ Decode 2 ] [ Chunk 1: 512 tokens (18ms) ]
Step 2: [ Decode 1 ] [ Decode 2 ] [ Chunk 2: 512 tokens (18ms) ]
Step 3: [ Decode 1 ] [ Decode 2 ] [ Chunk 3: 512 tokens (18ms) ]
... (Smooth 18ms per-token decode streaming maintained throughout!)
```

* **Token Budgeting**: The scheduler defines a fixed computation budget per iteration (e.g., $B = 512$ or $1024$ tokens).
* **Chunk Slicing**: Long prompt prefills are partitioned into equal chunks of size $B$.
* **Co-execution**: In each step, the scheduler pairs ongoing decode tokens with one chunk of prefill:
  $$\text{Iteration Tokens} = N_{\text{decode}} + \text{Chunk}_{\text{prefill}} \le B$$
* **Benefits**:
  1. Keeps Inter-Token Latency (ITL) predictable and smooth ($< 25\text{ms}$).
  2. Converts decode GEMV into a more compute-efficient batch GEMM, increasing overall system MFU without violating latency SLOs.

---

### Question 3: Disaggregated Prefill and Decode Architecture (Splitwise / Mooncake)
> **Explain the Disaggregated Prefill-Decode architecture. Why separate prefill nodes from decode nodes? How is the KV cache transferred across the network?**

#### Solution:

```
                                [ Incoming Client Requests ]
                                             |
                                +----------------------------+
                                |  Global Inference Router   |
                                +----------------------------+
                                             |
                   +-------------------------+-------------------------+
                   |                                                   |
        [ Prefill Cluster (P-Nodes) ]                               [ Decode Cluster (D-Nodes) ]
     - Optimized for Compute (H100 / SXM)                        - Optimized for Memory BW (HBM3e)
     - High Batch GEMM                                           - Low Batch GEMV
     - Computes Prompt KV Cache                                  - Continuous Token Generation
                   |                                                   ^
                   +=======[ High-Speed RDMA / RoCE Network ]==========+
                             (Fast Async KV Cache Transfer)
```

#### 1. Why Disaggregate?
* **Resource Mismatch**:
  * **Prefill**: Highly compute-bound ($>70\%$ Tensor Core utilization). Prefers maximum FLOPS, larger batch sizes.
  * **Decode**: Highly memory-bandwidth bound ($<20\%$ compute utilization). Prefers fast HBM3e bandwidth and low latency.
* **SLO Conflict**: In a unified node, prefill spikes directly ruin the Time-Per-Output-Token (TPOT) SLO of decode requests.
* **Optimal Hardware Sizing**: P-nodes can use compute-dense chips, while D-nodes can use high-memory-capacity configurations.

#### 2. KV Cache Transfer over RDMA
1. **P-Node**: Computes prompt attention and generates the complete Key and Value activation tensors for all layers.
2. **Async RDMA Write (Remote Direct Memory Access)**:
   * The P-Node uses RoCE v2 or InfiniBand to write KV blocks directly into the target D-Node's GPU HBM via GPU-Direct RDMA (GD-RDMA) with zero CPU overhead.
3. **D-Node**: Receives the pre-populated KV cache blocks and initiates the autoregressive decode loop starting from token 1.

---

# Part 2: KV Cache Memory Management & Attention Architectures

---

### Question 4: PagedAttention & vLLM Virtual Memory Architecture
> **Explain the PagedAttention algorithm in vLLM. How does it resolve internal and external memory fragmentation in the KV cache?**

#### Solution:

```
Logical KV Cache (Contiguous in Request View):
Token 0 ... Token 3 | Token 4 ... Token 7 | Token 8 ... Token 11 | Token 12 ... Token 15
    [ Block 0 ]     |     [ Block 1 ]     |     [ Block 2 ]      |     [ Block 3 ]

                                 | Block Table Mapping
                                 v
Physical Memory (Non-Contiguous Pages in GPU HBM):
[ Phys Page 14: Block 0 ]  [ Phys Page 3: Block 2 ]  [ Phys Page 89: Block 1 ]  [ Phys Page 2: Block 3 ]
```

#### 1. The Fragmentation Problem in Traditional Serving
* In naive serving engines, systems pre-allocate a contiguous memory block for each request based on `max_context_length` (e.g., 2048 or 4096 tokens).
* **External Fragmentation**: Memory is partitioned into reserved static buffers that cannot be shared.
* **Internal Fragmentation**: If a user generates 50 tokens in a 2048-token pre-allocated slot, **$97.5\%$ of the allocated KV cache memory is wasted**.
* **Result**: Up to $60\% - 80\%$ of GPU memory is wasted on empty reservations, capping maximum concurrency.

#### 2. PagedAttention Mechanics
* Inspired by **Operating System Virtual Memory with Paging**:
  1. **Fixed-Size Physical Blocks**: KV cache is broken into fixed-size physical blocks (e.g., 16 or 32 tokens per block).
  2. **Logical-to-Physical Block Table**: Each active request maintains a lightweight dynamic routing table:
     $$\text{BlockTable}: \text{Logical Block Index} \longrightarrow \text{Physical Page Address}$$
  3. **On-Demand Allocation**: Physical pages are allocated strictly when new tokens fill the current block.
  4. **Memory Sharing via Copy-on-Write (CoW)**: Multiple requests sharing a common prompt (e.g., parallel sampling, tree search) point to the same physical KV pages without duplication. When a branch modifies a token, only that single page is copied.
* **Impact**: Reduces KV memory waste to **$<4\%$** (only the fractional last block), enabling **$2\times - 4\times$ higher concurrent batch capacity**.

---

### Question 5: RadixAttention & Prefix Caching (SGLang)
> **What is RadixAttention? How does it use a Radix Tree (Trie) to enable automatic KV cache reuse across multi-turn chats, few-shot prompts, and agentic workflows?**

#### Solution:

```
Radix Tree KV Cache Hierarchy:

                     [ Root ]
                        |
            "You are a helpful AI assistant..." (System Prompt: Block 0-3)
                        |
         +--------------+--------------+
         |                             |
  User Prompt A                 User Prompt B
(Chat Session 1)              (Chat Session 2)
  [ Blocks 4-7 ]                [ Blocks 8-11 ]
```

#### 1. The Motivation: Massive Redundant Prefills
In production APIs:
* $50\%+$ of tokens belong to shared system prompts, formatting instructions, few-shot examples, or multi-turn chat history.
* Computing prefill for identical prompt prefixes on every query wastes GPU compute and inflates TTFT.

#### 2. RadixAttention Data Structure & LRU Eviction
* **Radix Tree (Trie over Token Sequences)**:
  * Key: Sequence of token IDs.
  * Value: Cached physical KV memory block pointers in GPU HBM.
* **Lookup during Request Arrival**:
  * New request token stream is matched against the Radix Tree.
  * Finds the Longest Common Prefix ($L_{\text{match}}$).
  * **Instant Prefill Bypass**: The engine directly loads the cached KV blocks for the first $L_{\text{match}}$ tokens with **zero prefill FLOPs**, only computing prefill for the newly appended suffix.
* **Eviction Policy**:
  * When GPU memory is low, an **LRU (Least Recently Used)** cache eviction algorithm prunes leaf nodes in the tree, freeing their underlying physical PagedAttention blocks.

---

### Question 6: Attention Architectural Trade-Offs (MHA vs. MQA vs. GQA vs. DeepSeek MLA)
> **Compare Multi-Head Attention (MHA), Multi-Query Attention (MQA), Grouped-Query Attention (GQA), and Multi-Head Latent Attention (MLA). Derive the KV cache memory formulas per token.**

#### Solution:

```
MHA (Standard):    [ Q1 Q2 Q3 Q4 Q5 Q6 Q7 Q8 ]  ===>  [ K1 K2 K3 K4 K5 K6 K7 K8 ] [ V1 V2 V3 V4 V5 V6 V7 V8 ] (1:1 Ratio)
MQA (Multi-Query): [ Q1 Q2 Q3 Q4 Q5 Q6 Q7 Q8 ]  ===>  [            K1           ] [            V1           ] (N:1 Ratio)
GQA (Grouped):     [ Q1 Q2 Q3 Q4 ] [ Q5 Q6 Q7 Q8 ] ==>  [      K1     ]           [      K2     ]           (Group Ratio)
MLA (DeepSeek):    Compressed Latent Vector c_KV (Low-Rank) ===> Decompressed on-the-fly via Absorbed Matrix!
```

#### 1. Mathematical Breakdown & KV Cache Memory Formulas
Let $L$ = number of layers, $H_Q$ = number of query heads, $H_{KV}$ = number of key-value heads, $d$ = head dimension, $b$ = precision in bytes (e.g., $b=2$ for FP16, $b=1$ for FP8).

$$\text{KV Cache Bytes per Token} = 2 \times L \times H_{KV} \times d \times b$$

| Architecture | $H_{KV}$ Heads | KV Cache Size Formula | Relative KV Footprint (vs. MHA) | Primary Model Adoption |
| :--- | :--- | :--- | :--- | :--- |
| **MHA (Multi-Head)** | $H_{KV} = H_Q$ (e.g., 32) | $2 \cdot L \cdot H_Q \cdot d \cdot b$ | $\mathbf{1.0\times}$ (Baseline: High memory) | GPT-3, LLaMA-1 |
| **MQA (Multi-Query)** | $H_{KV} = 1$ | $2 \cdot L \cdot 1 \cdot d \cdot b$ | $\mathbf{1 / H_Q\times}$ ($\approx 3\%$ of MHA) | Falcon, PaLM |
| **GQA (Grouped-Query)**| $1 < H_{KV} < H_Q$ (e.g., 8) | $2 \cdot L \cdot H_{KV} \cdot d \cdot b$ | $\mathbf{H_{KV} / H_Q\times}$ (e.g., $1/4$ or $1/8$) | LLaMA-2/3, Mistral, Gemma |
| **MLA (Multi-Head Latent)**| Compressed Latent $d_c$ | $L \cdot (d_c + d_R) \cdot b$ | $\mathbf{\approx 1/10\times}$ of MHA | DeepSeek-V2, DeepSeek-V3 |

#### 2. DeepSeek Multi-Head Latent Attention (MLA) Deep Dive
* Instead of storing full $K, V$ matrices ($2 \times H_Q \times d$), MLA projects $K, V$ into a low-rank compressed latent vector $c_t^{KV} \in \mathbb{R}^{d_c}$ (where $d_c \ll H_Q \cdot d$).
* **Matrix Absorption Trick during Inference**:
  During generation, the up-projection matrix $W^{UK}$ is mathematically folded directly into the query projection ($Q \cdot W^{UK}$), allowing the attention score to be computed directly against the compressed $c_t^{KV}$ cache without ever decompressing the full Key vectors into VRAM!

---

# Part 3: Latency Metrics & Generation Acceleration

---

### Question 7: Key Latency Metrics (TTFT, TPOT/ITL) & Latency-Throughput Trade-Off
> **Define Time-to-First-Token (TTFT) and Time-Per-Output-Token (TPOT / ITL). How does increasing batch size affect each metric?**

#### Solution:

#### 1. Metric Definitions
1. **Time-to-First-Token (TTFT)**:
   $$\text{TTFT} = T_{\text{queue}} + T_{\text{prefill}}$$
   * Measures the duration from when the client sends a prompt until the first token appears. Dictated by **queueing delay** and **prefill GEMM execution time**.
2. **Time-Per-Output-Token (TPOT) / Inter-Token Latency (ITL)**:
   $$\text{TPOT} = \frac{T_{\text{total}} - \text{TTFT}}{N_{\text{output\_tokens}} - 1}$$
   * Measures the average delta time between consecutive streaming tokens. Dictated by **decode GEMV iteration latency**.
3. **Total Request Latency**: $T_{\text{total}} = \text{TTFT} + (N - 1) \times \text{TPOT}$.

#### 2. The Latency vs. Throughput Frontier
```
Latency (TPOT)
  ^
  |                                        / (High Batch: Congested Memory BW)
  |                                       /
  |                                      /
  |                            ---------/
  |                  --------- (Linear Region)
  |  ---------------+ (Compute Idle Region)
  +----------------------------------------------------> Batch Size / Throughput
```

* **Low Batch Size ($B = 1 - 4$)**:
  * TPOT is minimized (fastest user experience), but GPU memory bandwidth is under-saturated (poor hardware efficiency; high serving cost).
* **Large Batch Size ($B = 64 - 256$)**:
  * Throughput (tokens/sec) scales dramatically, but TPOT degrades because each token step requires loading more KV cache data from HBM.
* *Infra Goal*: Tune dynamic batching caps to maximize throughput at the exact knee of the curve right before violating the client's TPOT SLO (e.g., TPOT $\le 30\text{ms}$).

---

### Question 8: Speculative Decoding (EAGLE, Medusa, Speculative Verification)
> **How does Speculative Decoding break the memory-bandwidth bottleneck of autoregressive generation? How do verification kernels validate $K$ speculative tokens in parallel?**

#### Solution:

```
Autoregressive (Sequential: 4 Steps = 4 Memory Passes):
Step 1: [Token 1] ---> Step 2: [Token 2] ---> Step 3: [Token 3] ---> Step 4: [Token 4]

Speculative Decoding (1 Draft Phase + 1 Parallel Target Verification):
Draft Model (Fast): Drafts [ "capital", "of", "France", "is" ] in 4 ultra-fast steps.
Target Model (70B): Runs a SINGLE forward pass evaluating all 4 tokens in parallel (Compute-Bound GEMM)!
Target Verifies:    [ "capital" (OK), "of" (OK), "France" (OK), "is" (OK), "Paris" (Bonus Token) ]
Result: Generated 5 tokens in the time of 1 Target Model decode step!
```

#### 1. Core Principle
* Recall that single-token decode is **memory-bandwidth bound** (Arithmetic Intensity $\approx 1\text{ FLOP/byte}$).
* The target model's compute ALUs are sitting $80\%+$ idle during standard decode.
* Speculative decoding uses this idle compute to verify multiple candidate tokens simultaneously in a **single forward GEMM pass**.

#### 2. Verification Algorithm (Greedy Sampling)
1. **Draft Generation**: A small draft model (e.g., LLaMA-3-1B) generates $K$ draft tokens ($x_1, x_2, \dots, x_K$) sequentially.
2. **Parallel Target Evaluation**: The large target model (e.g., LLaMA-3-70B) processes all $K$ tokens in a single forward pass with a causal tree mask, producing conditional probability distributions $P(x_{i+1} \mid x_{\le i})$.
3. **Acceptance Verification**:
   * For each token $i \in [1, K]$:
     $$\text{Accept } x_i \iff \arg\max P_{\text{target}}(x_i \mid x_{<i}) == x_i$$
   * Stop at the first rejected token $j$.
   * Emit accepted tokens $x_1, \dots, x_{j-1}$ plus one corrected sample token from $P_{\text{target}}(\cdot \mid x_{<j})$.
4. **Mathematical Guarantee**: Output text matches the exact mathematical probability distribution of the target model with **$2\times - 3.5\times$ speedup**.

---

### Question 9: FlashDecoding (Sequence Parallel Decode Acceleration)
> **Why does standard FlashAttention fail to parallelize efficiently during the decode phase? How does FlashDecoding parallelize attention across the sequence dimension ($S$)?**

#### Solution:

```
Standard FlashAttention in Decode (Low Parallelism):
Batch B=1, Heads H=32. Total Thread Blocks = 1 * 32 = 32.
On an H100 with 132 SMs, 100 SMs sit completely IDLE!

FlashDecoding (Split-KV across Sequence Dimension S):
Split S = 64k tokens into 64 chunks (1024 tokens each).
Total Thread Blocks = 1 (Batch) * 32 (Heads) * 64 (Chunks) = 2,048 Thread Blocks!
All 132 SMs are fully saturated!
A final reduction kernel merges partial Softmax accumulators.
```

#### 1. The Parallelism Bottleneck in Decode Attention
* During Prefill: Query length $Q = S$ (e.g., 4096). FlashAttention parallelizes across `Batch x Heads x Q_tiles`, yielding thousands of thread blocks that saturate all GPU Streaming Multiprocessors (SMs).
* During Decode: Query length is **strictly $Q = 1$**.
  * FlashAttention can only parallelize across $\text{Batch} \times \text{Heads}$.
  * For batch size $B=1$ on a 32-head model, only 32 thread blocks are launched. On an NVIDIA H100 (132 SMs), **$>75\%$ of the GPU SMs sit completely idle**.

#### 2. FlashDecoding Mechanics
1. **Split KV along Sequence Axis ($S$)**: Divides the long KV cache sequence $S$ into $C$ independent chunks (e.g., chunks of 512 tokens).
2. **Concurrent SM Computation**: Each chunk is assigned to a different SM. Each SM computes partial attention and maintains local Softmax running stats ($m_c, l_c$).
3. **Reduction Pass**: A lightweight tree reduction kernel combines the $C$ partial outputs using Online Softmax equations to produce the final output token vector.
4. **Impact**: Speeds up long-context decode attention ($S > 16\text{k}$) by up to **$8\times$**.

---

# Part 4: Quantization, Kernels & Engine Optimization

---

### Question 10: CUDA Graphs for LLM Inference
> **Why is CPU launch overhead devastating for small-batch LLM inference? How do CUDA Graphs eliminate CPU launch overhead, and how do you handle dynamic batch sizes?**

#### Solution:

#### 1. CPU Kernel Launch Overhead
* A single Transformer layer requires 15–25 individual CUDA kernel launches (GEMMs, LayerNorms, Add, RoPE, Softmax). A 32-layer model executes **$\sim 600$ kernel launches per token**.
* Each CUDA launch incurs a CPU driver overhead of $\sim 3-5\mu\text{s}$.
  $$\text{CPU Overhead per Token} = 600 \times 4\mu\text{s} = \mathbf{2.4\text{ \textbf{ms}}}$$
* If the actual GPU computation takes only $5\text{ms}$, CPU launch overhead accounts for **$32\%$ of total inference latency**!

#### 2. CUDA Graph Solution
```
Standard Execution: CPU enqueues Op 1 -> GPU runs -> CPU enqueues Op 2 -> GPU runs (Hundreds of driver roundtrips)

CUDA Graph Execution:
Step 1: Capture Graph once during warmup (All 600 kernel nodes baked into a single static GPU graph).
Step 2: Runtime Launch ===> CPU fires 1 single ioctl call: cudaGraphLaunch(graph_handle) (< 5 microseconds)!
```

#### 3. Handling Dynamic Batch Sizes
* CUDA Graphs require **fixed static memory pointers and fixed tensor shapes**.
* **Bucketed CUDA Graphs Strategy**:
  * Pre-capture and instantiate dedicated CUDA Graphs for discrete batch sizes:
    $$\text{Graph Pool} = \{ B = 1, B = 2, B = 4, B = 8, B = 16, B = 32, B = 64 \}$$
  * At runtime, the scheduler pads the active batch to the nearest bucket size and launches the matching pre-captured graph.

---

### Question 11: Weight-Only Quantization (W4A16 / W8A16) vs. Weight-Activation (W8A8 / FP8)
> **Compare W4A16, W8A16, and FP8/W8A8 quantization for LLM inference. When is each optimal?**

#### Solution:

| Quantization Mode | Weights | Activations | Primary Hardware Engine | Benefit & Bottleneck | Best Used For |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **W4A16 (e.g. AWQ / GPTQ)** | INT4 | FP16/BF16 | FP16 Tensor Cores (Dequant on-the-fly) | Halves memory footprint by $4\times$. Maximizes decode bandwidth speed. Compute bound remains FP16. | Small to medium batch decode on memory-constrained GPUs. |
| **W8A16** | INT8 | FP16/BF16 | FP16 Tensor Cores | $2\times$ memory reduction with virtually zero loss in perplexity. | General serving where INT4 degradation is unacceptable. |
| **FP8 / W8A8 (e.g. SmoothQuant)** | FP8 / INT8 | FP8 / INT8 | **FP8 / INT8 Tensor Cores** | **Doubles peak compute FLOPS** ($2\times$ faster GEMM). Speeds up both Prefill and high-batch Decode. | High-concurrency enterprise serving on Ada/Hopper/Blackwell. |

* **Rule of Thumb**:
  * If **Decode Latency (TPOT)** at small batch size is the bottleneck $\rightarrow$ Use **W4A16 (AWQ)** to minimize weight transfer time.
  * If **Prefill Latency (TTFT)** or high-throughput batching is the bottleneck $\rightarrow$ Use **FP8 / W8A8** to leverage $2\times$ Tensor Core compute throughput.

---

# Part 5: Production Deployment, Load Balancing & Autoscaling

---

### Question 12: Prefix-Aware Load Balancing & Smart Routing
> **In a multi-replica LLM cluster, why is standard Round-Robin load balancing sub-optimal? How does Prefix-Aware Load Balancing improve cluster efficiency?**

#### Solution:

```
Round-Robin Routing (Cache Oblivious):
Req 1 (Doc A):  ====> Replica 1 (Computes & caches Doc A)
Req 2 (Doc A):  ====> Replica 2 (Misses cache! Recomputes Doc A from scratch)
Req 3 (Doc A):  ====> Replica 3 (Misses cache! Recomputes Doc A from scratch)
Result: Zero KV cache reuse across replicas!

Prefix-Aware Routing (Cache Affinity):
Req 1 (Doc A):  ====> Replica 1 (Computes & caches Doc A)
Req 2 (Doc A):  ====> Replica 1 (Cache HIT! 100% Prefill bypassed -> Instant TTFT)
Req 3 (Doc A):  ====> Replica 1 (Cache HIT! 100% Prefill bypassed -> Instant TTFT)
Result: Maximizes cluster-wide Radix cache hit rate!
```

#### 1. Prefix-Aware Router Architecture
* Maintains a lightweight hash map / Trie of active prefix hashes across all replica nodes.
* **Routing Policy**:
  $$\text{Score}(Node_i) = \alpha \cdot \text{PrefixMatchLength}(Node_i) - \beta \cdot \text{ActiveQueueDepth}(Node_i)$$
* **Trade-off Balancing**:
  * Routes queries with matching document prefixes to the replica holding the warm KV cache.
  * If the target replica's queue depth exceeds a safety threshold, spills over to the least-loaded replica to preserve TTFT SLOs.

---

## Final Quick Reference Sheet for AI Inference Interviews
* **Continuous Batching**: Evicts finished requests and admits new ones at every token iteration.
* **Chunked Prefill**: Breaks long prompts into chunks ($B \le 512$) to eliminate ITL jitter for concurrent decode.
* **Disaggregated P/D**: Separates compute-bound P-nodes from memory-bound D-nodes via RDMA KV transfer.
* **PagedAttention**: OS virtual memory for KV cache. Fixed pages eliminate fragmentation ($<4\%$ waste).
* **MLA Attention**: Compresses KV cache into a low-rank latent vector ($d_c$) with matrix absorption.
* **FlashDecoding**: Parallelizes decode attention across the sequence dimension ($S$) using split-KV reduction.
* **CUDA Graphs**: Captures full Transformer graph to eliminate $\sim 2.4\text{ms}$ CPU driver launch overhead per token.
