/*Core Logic of NMS
Sort all bounding boxes by their confidence scores in descending order.

Select the box with the highest score, add it to the final output list, and remove it from candidate pools.

Compute the Intersection over Union (IoU) between this box and all remaining candidate boxes.

Suppress (discard) any candidate box whose IoU is greater than a specified threshold (iou_threshold).

Repeat steps 2–4 until no candidate boxes remain.
*/  
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

// Structure to represent a Bounding Box
struct BoundingBox {
    float x1, y1, x2, y2; // Top-left and bottom-right coordinates
    float score;           // Confidence score
    int class_id;          // Optional: class ID for multi-class NMS
};

// Function to compute Intersection over Union (IoU) of two boxes
float computeIoU(const BoundingBox& boxA, const BoundingBox& boxB) {
    // 1. Calculate intersection coordinates
    float interX1 = std::max(boxA.x1, boxB.x1);
    float interY1 = std::max(boxA.y1, boxB.y1);
    float interX2 = std::min(boxA.x2, boxB.x2);
    float interY2 = std::min(boxA.y2, boxB.y2);

    // Calculate intersection area (clamp negative values to 0)
    float interWidth  = std::max(0.0f, interX2 - interX1);
    float interHeight = std::max(0.0f, interY2 - interY1);
    float interArea   = interWidth * interHeight;

    // 2. Calculate individual areas
    float areaA = (boxA.x2 - boxA.x1) * (boxA.y2 - boxA.y1);
    float areaB = (boxB.x2 - boxB.x1) * (boxB.y2 - boxB.y1);

    // 3. Calculate Union area and IoU
    float unionArea = areaA + areaB - interArea;
    if (unionArea <= 0.0f) return 0.0f; // Avoid division by zero

    return interArea / unionArea;
}

// Single-class Non-Maximum Suppression (NMS)
std::vector<BoundingBox> nms(std::vector<BoundingBox>& boxes, float iou_threshold) {
    std::vector<BoundingBox> keep;
    if (boxes.empty()) return keep;

    // Step 1: Sort boxes by score in descending order
    std::sort(boxes.begin(), boxes.end(), [](const BoundingBox& a, const BoundingBox& b) {
        return a.score > b.score;
    });

    // Track suppressed boxes to avoid redundant comparisons
    std::vector<bool> suppressed(boxes.size(), false);

    // Step 2 & 3: Iterate through sorted boxes
    for (size_t i = 0; i < boxes.size(); ++i) {
        if (suppressed[i]) continue;

        // Current highest score box is kept
        keep.push_back(boxes[i]);

        // Suppress all subsequent overlapping boxes
        for (size_t j = i + 1; j < boxes.size(); ++j) {
            if (suppressed[j]) continue;

            float iou = computeIoU(boxes[i], boxes[j]);
            if (iou > iou_threshold) {
                suppressed[j] = true; // Discard box j
            }
        }
    }

    return keep;
}

int main() {
    // Example test case: 3 overlapping bounding boxes
    std::vector<BoundingBox> boxes = {
        {10.0f, 10.0f, 50.0f, 50.0f, 0.90f, 0}, // Box 0: High score
        {12.0f, 12.0f, 48.0f, 48.0f, 0.85f, 0}, // Box 1: High overlap with Box 0 (Should be suppressed)
        {100.0f, 100.0f, 150.0f, 150.0f, 0.75f, 0} // Box 2: Far away (Should be kept)
    };

    float iou_threshold = 0.5f;
    std::vector<BoundingBox> result = nms(boxes, iou_threshold);

    std::cout << "Remaining boxes after NMS: " << result.size() << std::endl;
    for (size_t i = 0; i < result.size(); ++i) {
        std::cout << "Box " << i << " Score: " << result[i].score 
                  << " [( " << result[i].x1 << ", " << result[i].y1 << " ), ( " 
                  << result[i].x2 << ", " << result[i].y2 << " )]\n";
    }

    return 0;
}
