"""
Bilinear interpolation for image resize / grid_sample style sampling.
Interview focus: fractional coordinates, 4-neighbor weights, boundary handling.
"""

import numpy as np


def bilinear_sample(image: np.ndarray, x: float, y: float) -> float:
    """
    Sample one pixel from a single-channel image at fractional (x, y).
    Coordinate convention: (0,0) is top-left pixel center or top-left corner
    depending on team convention — clarify in interview.

    Here:
      - integer coordinate (i, j) refers to image[i, j]
      - fractional x along width (col), y along height (row)
    """
    h, w = image.shape

    if x < 0 or y < 0 or x > w - 1 or y > h - 1:
        return 0.0  # zero padding outside; mention alternatives in interview

    x0 = int(np.floor(x))
    y0 = int(np.floor(y))
    x1 = min(x0 + 1, w - 1)
    y1 = min(y0 + 1, h - 1)

    dx = x - x0
    dy = y - y0

    v00 = image[y0, x0]
    v01 = image[y0, x1]
    v10 = image[y1, x0]
    v11 = image[y1, x1]

    # bilinear weights
    w00 = (1 - dx) * (1 - dy)
    w01 = dx * (1 - dy)
    w10 = (1 - dx) * dy
    w11 = dx * dy

    return w00 * v00 + w01 * v01 + w10 * v10 + w11 * v11


def resize_bilinear(image: np.ndarray, out_h: int, out_w: int) -> np.ndarray:
    """
    Resize image to (out_h, out_w) using bilinear interpolation.
    image: (H, W) or (H, W, C)
    """
    if image.ndim == 2:
        return _resize_bilinear_2d(image, out_h, out_w)

    # channel-last
    h, w, c = image.shape
    out = np.zeros((out_h, out_w, c), dtype=np.float64)
    for ch in range(c):
        out[:, :, ch] = _resize_bilinear_2d(image[:, :, ch], out_h, out_w)
    return out


def _resize_bilinear_2d(image: np.ndarray, out_h: int, out_w: int) -> np.ndarray:
    in_h, in_w = image.shape
    output = np.zeros((out_h, out_w), dtype=np.float64)

    # map output pixel center to input coordinate
    scale_y = in_h / out_h
    scale_x = in_w / out_w

    for i in range(out_h):
        for j in range(out_w):
            # center-aligned mapping (common in vision frameworks)
            src_y = (i + 0.5) * scale_y - 0.5
            src_x = (j + 0.5) * scale_x - 0.5
            output[i, j] = bilinear_sample(image, src_x, src_y)

    return output


def grid_sample_2d(input_map: np.ndarray, grid: np.ndarray) -> np.ndarray:
    """
    PyTorch grid_sample style (2D, single channel).

    input_map: (H, W)
    grid:      (out_h, out_w, 2), each entry is normalized [-1, 1] coordinate
               grid[..., 0] = x, grid[..., 1] = y
    """
    in_h, in_w = input_map.shape
    out_h, out_w, _ = grid.shape
    output = np.zeros((out_h, out_w), dtype=np.float64)

    for i in range(out_h):
        for j in range(out_w):
            gx, gy = grid[i, j]
            # normalized [-1,1] -> pixel coordinate
            x = (gx + 1) * 0.5 * (in_w - 1)
            y = (gy + 1) * 0.5 * (in_h - 1)
            output[i, j] = bilinear_sample(input_map, x, y)

    return output


if __name__ == "__main__":
    img = np.array([
        [0, 1, 2],
        [3, 4, 5],
        [6, 7, 8],
    ], dtype=np.float64)

    print("sample at (1.2, 1.8):", bilinear_sample(img, 1.2, 1.8))
    print("resize 3x3 -> 5x5:\n", resize_bilinear(img, 5, 5))
