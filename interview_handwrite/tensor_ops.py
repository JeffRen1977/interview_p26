"""
Basic tensor operations without autograd frameworks.
Interview focus: broadcasting, shape reasoning, im2col, softmax stability.
"""

from typing import Optional

import numpy as np


# ---------------------------------------------------------------------------
# Element-wise and linear algebra
# ---------------------------------------------------------------------------

def relu(x: np.ndarray) -> np.ndarray:
    return np.maximum(x, 0)


def sigmoid(x: np.ndarray) -> np.ndarray:
    x = np.clip(x, -500, 500)
    return 1.0 / (1.0 + np.exp(-x))


def softmax(x: np.ndarray, axis: int = -1) -> np.ndarray:
    """Numerically stable softmax."""
    x_max = np.max(x, axis=axis, keepdims=True)
    exp_x = np.exp(x - x_max)
    return exp_x / np.sum(exp_x, axis=axis, keepdims=True)


def batch_norm(x: np.ndarray, gamma: np.ndarray, beta: np.ndarray,
               running_mean: np.ndarray, running_var: np.ndarray,
               eps: float = 1e-5) -> np.ndarray:
    """
    Inference-mode batch norm.
    x: (N, C, H, W)
    gamma/beta/running_mean/running_var: (C,)
    """
    x_norm = (x - running_mean.reshape(1, -1, 1, 1)) / np.sqrt(
        running_var.reshape(1, -1, 1, 1) + eps
    )
    return gamma.reshape(1, -1, 1, 1) * x_norm + beta.reshape(1, -1, 1, 1)


# ---------------------------------------------------------------------------
# Shape ops
# ---------------------------------------------------------------------------

def flatten(x: np.ndarray, start_dim: int = 1) -> np.ndarray:
    """Flatten from start_dim to end."""
    lead = int(np.prod(x.shape[:start_dim]))
    tail = int(np.prod(x.shape[start_dim:]))
    return x.reshape(lead, tail)


def reshape(x: np.ndarray, shape: tuple[int, ...]) -> np.ndarray:
    return x.reshape(shape)


def transpose2d(x: np.ndarray) -> np.ndarray:
    """Swap last two dims, e.g. (N,H,W,C) -> (N,W,H,C)."""
    return np.swapaxes(x, -2, -1)


# ---------------------------------------------------------------------------
# Pooling
# ---------------------------------------------------------------------------

def max_pool2d(x: np.ndarray, kernel_size: int = 2, stride: Optional[int] = None) -> np.ndarray:
    """
    x: (H, W) single channel, valid pooling.
    """
    if stride is None:
        stride = kernel_size

    h, w = x.shape
    out_h = (h - kernel_size) // stride + 1
    out_w = (w - kernel_size) // stride + 1
    out = np.zeros((out_h, out_w), dtype=x.dtype)

    for i in range(out_h):
        for j in range(out_w):
            patch = x[i * stride:i * stride + kernel_size,
                      j * stride:j * stride + kernel_size]
            out[i, j] = np.max(patch)
    return out


def avg_pool2d(x: np.ndarray, kernel_size: int = 2, stride: Optional[int] = None) -> np.ndarray:
    if stride is None:
        stride = kernel_size

    h, w = x.shape
    out_h = (h - kernel_size) // stride + 1
    out_w = (w - kernel_size) // stride + 1
    out = np.zeros((out_h, out_w), dtype=np.float64)

    for i in range(out_h):
        for j in range(out_w):
            patch = x[i * stride:i * stride + kernel_size,
                      j * stride:j * stride + kernel_size]
            out[i, j] = np.mean(patch)
    return out


# ---------------------------------------------------------------------------
# Fully connected layer
# ---------------------------------------------------------------------------

def linear(x: np.ndarray, weight: np.ndarray, bias: Optional[np.ndarray] = None) -> np.ndarray:
    """
    x:      (N, in_features)
    weight: (out_features, in_features)
    bias:   (out_features,)
    """
    out = x @ weight.T
    if bias is not None:
        out += bias
    return out


# ---------------------------------------------------------------------------
# Manual 2D convolution via im2col + matmul (tensor view)
# ---------------------------------------------------------------------------

def im2col(x: np.ndarray, kernel_size: int, stride: int = 1, padding: int = 0) -> np.ndarray:
    """
    x: (C, H, W)
    returns: (C*kH*kW, out_h*out_w)
    """
    c, h, w = x.shape
    if padding > 0:
        x = np.pad(x, ((0, 0), (padding, padding), (padding, padding)), mode="constant")
        _, h, w = x.shape

    out_h = (h - kernel_size) // stride + 1
    out_w = (w - kernel_size) // stride + 1

    cols = []
    for i in range(0, h - kernel_size + 1, stride):
        for j in range(0, w - kernel_size + 1, stride):
            patch = x[:, i:i + kernel_size, j:j + kernel_size].reshape(-1)
            cols.append(patch)
    return np.stack(cols, axis=1)


def conv2d_im2col(x: np.ndarray, weight: np.ndarray, bias: Optional[np.ndarray] = None,
                  stride: int = 1, padding: int = 0) -> np.ndarray:
    """
    x:      (C_in, H, W)
    weight: (C_out, C_in, k, k)
    """
    c_out, c_in, k, _ = weight.shape
    col = im2col(x, k, stride=stride, padding=padding)
    w_mat = weight.reshape(c_out, -1)
    out = w_mat @ col

    _, h, w = x.shape
    if padding > 0:
        h += 2 * padding
        w += 2 * padding
    out_h = (h - k) // stride + 1
    out_w = (w - k) // stride + 1

    out = out.reshape(c_out, out_h, out_w)
    if bias is not None:
        out += bias.reshape(-1, 1, 1)
    return out


# ---------------------------------------------------------------------------
# Simple attention score (bonus)
# ---------------------------------------------------------------------------

def scaled_dot_product_attention(q: np.ndarray, k: np.ndarray, v: np.ndarray) -> np.ndarray:
    """
    q: (seq, d_k)
    k: (seq, d_k)
    v: (seq, d_v)
    returns: (seq, d_v)
    """
    d_k = q.shape[-1]
    scores = q @ k.T / np.sqrt(d_k)
    attn = softmax(scores, axis=-1)
    return attn @ v


if __name__ == "__main__":
    x = np.array([[1.0, 2.0, 3.0],
                  [4.0, 5.0, 6.0]], dtype=np.float64)
    print("softmax:", softmax(x, axis=-1))
    print("max_pool2d:\n", max_pool2d(x, kernel_size=2, stride=1))

    inp = np.random.randn(2, 4, 4)
    w = np.random.randn(3, 2, 3, 3)
    print("conv2d_im2col shape:", conv2d_im2col(inp, w, padding=1).shape)
