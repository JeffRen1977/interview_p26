在深度学习底层的 C++ 实现中，**Softmax**、**GELU** 以及大模型 (LLM) 必备的 **RoPE (Rotary Position Embedding)** 是常考的三大基础算子。

下面为您提供符合现代 C++ 规范、且针对**数值稳定性**和**内存连续性**进行了优化的完整手写实现。

---

## 1. 数值稳定的 Softmax (Numerically Stable Softmax)

### 数值稳定性原理

直接计算 $\exp(\mathrm{xi})$ 时，如果 $\mathrm{xi}$ 较大（如 $\mathrm{xi} = 100$），$\exp(100)$ 会直接引发**上溢（Overflow / `inf`）**；如果分子分母同时减去最大值 $\mathrm{xmax}$，公式等价为：

$$
\mathrm{Softmax}(\mathrm{xi}) = \frac{\exp(\mathrm{xi}-\mathrm{xmax})}{\sum \exp(\mathrm{xj}-\mathrm{xmax})}
$$

```text
Softmax(xi) = exp(xi - xmax) / sum_j exp(xj - xmax)
```

（分母对所有下标 `j` 求和。）这样指数幂最大为 $\exp(0)=1$，避免上溢。

### C++ 代码实现 (支持 2D Batch 处理)

```cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <iomanip>

// src/dst: 2D 矩阵指针 [rows, cols]
// 对每一行 (cols 维) 独执行 Softmax
void softmax(const float* src, float* dst, int rows, int cols) {
    for (int r = 0; r < rows; ++r) {
        const float* row_src = src + r * cols;
        float* row_dst = dst + r * cols;

        // Step 1: 寻找当前行的 Max 值 (避免上溢)
        float max_val = -std::numeric_limits<float>::infinity();
        for (int c = 0; c < cols; ++c) {
            max_val = std::max(max_val, row_src[c]);
        }

        // Step 2: 计算 e^(x - max) 并累加 Sum
        float sum_exp = 0.0f;
        for (int c = 0; c < cols; ++c) {
            float exp_val = std::exp(row_src[c] - max_val);
            row_dst[c] = exp_val; // 临时保存分母项
            sum_exp += exp_val;
        }

        // Step 3: 归一化，除以 Sum (乘以倒数变除法为乘法，提高性能)
        float inv_sum = 1.0f / sum_exp;
        for (int c = 0; c < cols; ++c) {
            row_dst[c] *= inv_sum;
        }
    }
}

```

---

## 2. GELU 激活函数 (Gaussian Error Linear Unit)

### 数学公式

GELU 被广泛用于 BERT、GPT 等 Transformer 模型中。标准定义为：

$$
\text{GELU}(x) = x \cdot \Phi(x) = x \cdot \frac{1}{2}\left[1 + \text{erf}\left(\frac{x}{\sqrt{2}}\right)\right]
$$

在 PyTorch 等框架中，常用高精度的**快速 Tanh 近似公式**（运算速度更快）：

$$
\text{GELU}(x) \approx 0.5 \cdot x \cdot \left(1 + \tanh\left(\sqrt{\frac{2}{\pi}} \left(x + 0.044715 \cdot x^3\right)\right)\right)
$$

### C++ 代码实现

```cpp
#include <iostream>
#include <vector>
#include <cmath>

// GELU 准确版本 (基于 std::erf)
void gelu_exact(const float* src, float* dst, size_t size) {
    constexpr float inv_sqrt_2 = 0.7071067811865475f; // 1 / sqrt(2)
    for (size_t i = 0; i < size; ++i) {
        dst[i] = 0.5f * src[i] * (1.0f + std::erf(src[i] * inv_sqrt_2));
    }
}

// GELU 快速近似版本 (Fast Tanh Approximation)
void gelu_fast_tanh(const float* src, float* dst, size_t size) {
    constexpr float sqrt_2_over_pi = 0.7978845608028654f; // sqrt(2 / pi)
    constexpr float coeff = 0.044715f;

    for (size_t i = 0; i < size; ++i) {
        float x = src[i];
        float inner = sqrt_2_over_pi * (x + coeff * x * x * x);
        dst[i] = 0.5f * x * (1.0f + std::tanh(inner));
    }
}

```

---

## 3. RoPE 旋转位置编码 (Rotary Position Embedding)

### 旋转原理

RoPE 通过把 2D 向量视角下的特征对 $(\mathrm{xEven}, \mathrm{xOdd})$（即通道 `2i` 与 `2i+1`）在复平面内旋转角度 $m\cdot\theta$，从而将**绝对位置 $m$** 隐式编码为**相对位置**。

二维旋转公式矩阵形式：

$$
\begin{pmatrix} \mathrm{xPrimeEven} \\ \mathrm{xPrimeOdd} \end{pmatrix}
=
\begin{pmatrix} \cos(m\theta) & -\sin(m\theta) \\ \sin(m\theta) & \cos(m\theta) \end{pmatrix}
\begin{pmatrix} \mathrm{xEven} \\ \mathrm{xOdd} \end{pmatrix}
$$

```text
[x'_even]   [ cos(mθ)  -sin(mθ) ] [xEven]
[x'_odd ] = [ sin(mθ)   cos(mθ) ] [xOdd ]
θ = 10000^(-2i/d)   # d = head_dim
```

其中频率 $\theta = 10000^{-2i/d}$（$d$ 为 `head_dim`）。

### C++ 代码实现 (支持 In-place 旋转)

```cpp
#include <iostream>
#include <vector>
#include <cmath>

/**
 * @brief 对 Query 或 Key 应用 RoPE
 * @param sq_data 维度为 [seq_len, num_heads, head_dim] 的 3D 张量数据指针
 * @param seq_len 序列长度 (Sequence Length)
 * @param num_heads 注意力头数 (Number of Heads)
 * @param head_dim 每个 Head 的维度 (须为偶数，通常为 64, 128 等)
 * @param base 频率基数 (LLM 默认通常为 10000.0f 或 500000.0f)
 */
void apply_rope(float* sq_data, int seq_len, int num_heads, int head_dim, float base = 10000.0f) {
    // 确保 head_dim 是偶数，能够两两成对旋转
    if (head_dim % 2 != 0) return;

    int half_dim = head_dim / 2;

    // 1. 遍历序列中的每个位置 m (pos)
    for (int m = 0; m < seq_len; ++m) {
        // 2. 遍历每一个注意力头 head
        for (int h = 0; h < num_heads; ++h) {
            float* vec = sq_data + (m * num_heads * head_dim) + (h * head_dim);

            // 3. 对该向量的每对元素 (x_2i, x_2i+1) 应用旋转
            for (int i = 0; i < half_dim; ++i) {
                // 计算当前通道对的旋转频率 theta_i = base^(-2i / head_dim)
                float freq = std::pow(base, -2.0f * i / head_dim);
                float angle = m * freq; // m * theta_i

                float cos_a = std::cos(angle);
                float sin_a = std::sin(angle);

                // 旋转相邻的两个元素 (x_2i, x_2i+1)
                float x0 = vec[2 * i];
                float x1 = vec[2 * i + 1];

                vec[2 * i]     = x0 * cos_a - x1 * sin_a;
                vec[2 * i + 1] = x0 * sin_a + x1 * cos_a;
            }
        }
    }
}

```

---

## 4. 完整 Demo 主函数测试

可以用下面的主函数快速测试验证以上三个算子的正确性：

```cpp
int main() {
    std::cout << std::fixed << std::setprecision(4);

    // ==========================================
    // 1. 测试 Stable Softmax
    // ==========================================
    std::cout << "=== 1. Softmax Test ===" << std::endl;
    // 输入带有较大数值 (1000.0f)，验证是否溢出
    std::vector<float> softmax_input = {1000.0f, 1002.0f, 1001.0f};
    std::vector<float> softmax_output(3);

    softmax(softmax_input.data(), softmax_output.data(), 1, 3);
    
    std::cout << "Input:  1000.0, 1002.0, 1001.0" << std::endl;
    std::cout << "Output: ";
    for (float v : softmax_output) std::cout << v << " ";
    std::cout << "\n\n";

    // ==========================================
    // 2. 测试 GELU
    // ==========================================
    std::cout << "=== 2. GELU Test ===" << std::endl;
    std::vector<float> gelu_input = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
    std::vector<float> gelu_out_exact(5);
    std::vector<float> gelu_out_fast(5);

    gelu_exact(gelu_input.data(), gelu_out_exact.data(), 5);
    gelu_fast_tanh(gelu_input.data(), gelu_out_fast.data(), 5);

    std::cout << "Input:     -2.0000 -1.0000  0.0000  1.0000  2.0000" << std::endl;
    std::cout << "GELU Exact: ";
    for (float v : gelu_out_exact) std::cout << v << " ";
    std::cout << "\nGELU Tanh:  ";
    for (float v : gelu_out_fast) std::cout << v << " ";
    std::cout << "\n\n";

    // ==========================================
    // 3. 测试 RoPE
    // ==========================================
    std::cout << "=== 3. RoPE Test ===" << std::endl;
    // 模拟 seq_len=1, num_heads=1, head_dim=4 的张量
    std::vector<float> rope_data = {1.0f, 0.0f, 1.0f, 0.0f};
    std::cout << "RoPE Input (Pos 0): ";
    for (float v : rope_data) std::cout << v << " ";
    std::cout << std::endl;

    apply_rope(rope_data.data(), 1, 1, 4);

    std::cout << "RoPE Output (Pos 0):";
    for (float v : rope_data) std::cout << v << " ";
    std::cout << " (At pos 0, angle is 0, so vector remains unchanged)" << std::endl;

    return 0;
}

```

---

## 5. 面试加分与底层优化视角 (Senior/Staff 视角)

1. **Softmax SIMD 优化**：
* 在真实推理引擎（如 Llama.cpp / TensorRT）中，寻找 Max 值和累加 Exp 都会使用 **AVX2 / NEON 向量化** 进行指令级并行（如 `_mm256_max_ps`, `_mm256_add_ps`）。
* 此外，多次遍历内存（找 Max $\rightarrow$ 求 Exp $\rightarrow$ 归一化）会导致内存带宽瓶颈，常采用 **Flash-Softmax (Online Softmax)** 算法：通过在一趟遍历（One-pass）中动态更新 Max 值和 Sum 值，从而极大节省内存读写开销。


2. **RoPE 旋转预计算 (Sine/Cosine Cache)**：
* 在大模型推理中，不会在运行时重复计算 `std::sin(angle)` 和 `std::cos(angle)`。
* 通常会在模型初始化阶段提前计算好预存表格 **`cos_cached` 和 `sin_cached` [max_seq_len, head_dim]**，运行时直接通过查表 (Lookup Table) + SIMD 指令进行点乘和相加。
