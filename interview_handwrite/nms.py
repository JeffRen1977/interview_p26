"""
Non-Maximum Suppression for object detection boxes.
Interview focus: IoU, score sorting, greedy suppression, optional class-aware NMS.
"""

import numpy as np


def compute_iou(box_a: np.ndarray, box_b: np.ndarray) -> float:
    """
    box: [x1, y1, x2, y2], x2/y2 are exclusive or inclusive depending on convention.
    Here we use inclusive coordinates.
    """
    x1 = max(box_a[0], box_b[0])
    y1 = max(box_a[1], box_b[1])
    x2 = min(box_a[2], box_b[2])
    y2 = min(box_a[3], box_b[3])

    inter_w = max(0.0, x2 - x1 + 1)
    inter_h = max(0.0, y2 - y1 + 1)
    inter_area = inter_w * inter_h

    area_a = (box_a[2] - box_a[0] + 1) * (box_a[3] - box_a[1] + 1)
    area_b = (box_b[2] - box_b[0] + 1) * (box_b[3] - box_b[1] + 1)
    union_area = area_a + area_b - inter_area
    if union_area <= 0:
        return 0.0
    return inter_area / union_area


def nms(boxes: np.ndarray, scores: np.ndarray, iou_threshold: float = 0.5) -> list[int]:
    """
    boxes:  (N, 4) [x1, y1, x2, y2]
    scores: (N,)
    returns indices of kept boxes
    """
    if len(boxes) == 0:
        return []

    order = scores.argsort()[::-1]  # high score first
    keep = []

    while order.size > 0:
        i = order[0]
        keep.append(int(i))

        if order.size == 1:
            break

        rest = order[1:]
        ious = np.array([compute_iou(boxes[i], boxes[j]) for j in rest])
        order = rest[ious <= iou_threshold]

    return keep


def nms_vectorized(boxes: np.ndarray, scores: np.ndarray,
                   iou_threshold: float = 0.5) -> list[int]:
    """Same logic, but IoU computed in batch (faster, cleaner in interviews)."""
    if len(boxes) == 0:
        return []

    x1, y1, x2, y2 = boxes[:, 0], boxes[:, 1], boxes[:, 2], boxes[:, 3]
    areas = (x2 - x1 + 1) * (y2 - y1 + 1)
    order = scores.argsort()[::-1]
    keep = []

    while order.size > 0:
        i = order[0]
        keep.append(int(i))
        if order.size == 1:
            break

        xx1 = np.maximum(x1[i], x1[order[1:]])
        yy1 = np.maximum(y1[i], y1[order[1:]])
        xx2 = np.minimum(x2[i], x2[order[1:]])
        yy2 = np.minimum(y2[i], y2[order[1:]])

        w = np.maximum(0.0, xx2 - xx1 + 1)
        h = np.maximum(0.0, yy2 - yy1 + 1)
        inter = w * h
        iou = inter / (areas[i] + areas[order[1:]] - inter)

        order = order[1:][iou <= iou_threshold]

    return keep


def nms_per_class(boxes: np.ndarray, scores: np.ndarray, class_ids: np.ndarray,
                  iou_threshold: float = 0.5) -> list[int]:
    """Class-aware NMS: suppress only within the same class."""
    keep_all = []
    for cls in np.unique(class_ids):
        idx = np.where(class_ids == cls)[0]
        kept = nms_vectorized(boxes[idx], scores[idx], iou_threshold)
        keep_all.extend(idx[kept].tolist())
    return keep_all


def soft_nms(boxes: np.ndarray, scores: np.ndarray,
             iou_threshold: float = 0.5, sigma: float = 0.5,
             score_threshold: float = 0.001) -> list[int]:
    """
    Soft-NMS (bonus): decay scores of overlapping boxes instead of hard removal.
    Good talking point for crowded scenes.
    """
    boxes = boxes.astype(np.float64).copy()
    scores = scores.astype(np.float64).copy()
    keep = []

    while scores.size > 0:
        max_idx = int(np.argmax(scores))
        keep.append(max_idx)

        if scores.size == 1:
            break

        # swap selected box to front for easier indexing
        boxes[[0, max_idx]] = boxes[[max_idx, 0]]
        scores[[0, max_idx]] = scores[[max_idx, 0]]

        ious = np.array([compute_iou(boxes[0], boxes[i]) for i in range(1, len(boxes))])

        # linear weighting
        decay = np.ones_like(ious)
        mask = ious > iou_threshold
        decay[mask] = 1.0 - ious[mask]

        # gaussian weighting (alternative)
        # decay = np.exp(-(ious ** 2) / sigma)

        scores[1:] *= decay
        boxes = boxes[1:]
        scores = scores[1:]

        valid = scores > score_threshold
        boxes = boxes[valid]
        scores = scores[valid]

    return keep


if __name__ == "__main__":
    boxes = np.array([
        [100, 100, 210, 210],
        [105, 105, 215, 215],
        [300, 300, 420, 420],
        [110, 110, 220, 220],
    ], dtype=np.float64)
    scores = np.array([0.9, 0.75, 0.8, 0.6])

    print("nms keep indices:", nms_vectorized(boxes, scores, iou_threshold=0.5))
