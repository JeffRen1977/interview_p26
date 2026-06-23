"""
2D image convolution (valid / same padding).
Interview focus: boundary handling, channel-wise conv, stride.
"""

from typing import Optional

import numpy as np


def conv2d_single_channel(image: np.ndarray, kernel: np.ndarray,
                          stride: int = 1, padding: int = 0) -> np.ndarray:
    """
    image: (H, W)
    kernel: (kH, kW), no flip needed if kernel is already defined as in DL frameworks
    """
    if padding > 0:
        image = np.pad(image, ((padding, padding), (padding, padding)), mode="constant")

    h, w = image.shape
    kh, kw = kernel.shape
    out_h = (h - kh) // stride + 1
    out_w = (w - kw) // stride + 1

    output = np.zeros((out_h, out_w), dtype=np.float64)
    for i in range(out_h):
        for j in range(out_w):
            region = image[i * stride:i * stride + kh, j * stride:j * stride + kw]
            output[i, j] = np.sum(region * kernel)
    return output


def conv2d_multi_channel(input_tensor: np.ndarray, kernels: np.ndarray,
                         bias: Optional[np.ndarray] = None,
                         stride: int = 1, padding: int = 0) -> np.ndarray:
    """
    Standard conv2d without dilation/groups.

    input_tensor: (C_in, H, W)
    kernels:      (C_out, C_in, kH, kW)
    bias:         (C_out,) optional
  output:       (C_out, H_out, W_out)
    """
    c_out, c_in, kh, kw = kernels.shape
    assert input_tensor.shape[0] == c_in

    if padding > 0:
        input_tensor = np.pad(
            input_tensor,
            ((0, 0), (padding, padding), (padding, padding)),
            mode="constant",
        )

    _, h, w = input_tensor.shape
    out_h = (h - kh) // stride + 1
    out_w = (w - kw) // stride + 1
    output = np.zeros((c_out, out_h, out_w), dtype=np.float64)

    for oc in range(c_out):
        for ic in range(c_in):
            output[oc] += conv2d_single_channel(
                input_tensor[ic], kernels[oc, ic], stride=stride, padding=0
            )
        if bias is not None:
            output[oc] += bias[oc]
    return output


def conv2d_optimized(input_tensor: np.ndarray, kernels: np.ndarray,
                     stride: int = 1, padding: int = 0) -> np.ndarray:
    """
    Vectorized version using sliding-window view (good to mention in interview).
    """
    c_in, h, w = input_tensor.shape
    c_out, _, kh, kw = kernels.shape

    if padding > 0:
        input_tensor = np.pad(
            input_tensor,
            ((0, 0), (padding, padding), (padding, padding)),
            mode="constant",
        )
        _, h, w = input_tensor.shape

    out_h = (h - kh) // stride + 1
    out_w = (w - kw) // stride + 1

    # Build im2col matrix: each column is one receptive field
    cols = []
    for i in range(0, h - kh + 1, stride):
        for j in range(0, w - kw + 1, stride):
            patch = input_tensor[:, i:i + kh, j:j + kw].reshape(-1)
            cols.append(patch)
    col_matrix = np.stack(cols, axis=1)  # (C_in*kH*kW, out_h*out_w)

    weight_matrix = kernels.reshape(c_out, -1)  # (C_out, C_in*kH*kW)
    out = weight_matrix @ col_matrix
    return out.reshape(c_out, out_h, out_w)


if __name__ == "__main__":
    img = np.array([
        [1, 2, 3, 0],
        [0, 1, 2, 3],
        [3, 0, 1, 2],
        [2, 3, 0, 1],
    ], dtype=np.float64)

    edge_kernel = np.array([
        [-1, -1, -1],
        [-1,  8, -1],
        [-1, -1, -1],
    ], dtype=np.float64)

    print("single channel conv:")
    print(conv2d_single_channel(img, edge_kernel, padding=1))
