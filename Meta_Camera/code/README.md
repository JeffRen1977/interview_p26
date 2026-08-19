# Meta Camera — coding drills

Runnable C++ for the Embedded coding loop. 节奏与规则：[`../coding_interview_guide.md`](../coding_interview_guide.md)。补充题与出题预测：[`../additional_questions.md`](../additional_questions.md)。

## 原有题（官方四专题）

| Topic | Drill | Files |
|-------|--------|--------|
| Bit / bytes | Packed RAW10 unpack | [mipi_raw10_unpack.md](./mipi_raw10_unpack.md), [unpack_mipi_raw10.cpp](./unpack_mipi_raw10.cpp) |
| Bit / bytes | Circular shift | [bit_rotation.md](./bit_rotation.md), [bit_rotation.cpp](./bit_rotation.cpp) |
| Bit / bytes | Endianness detect / swap | [endianness.md](./endianness.md), [endianness.cpp](./endianness.cpp) |
| Systems / memory | Aligned malloc / free | [aligned_malloc.md](./aligned_malloc.md), [aligned_malloc.cpp](./aligned_malloc.cpp) |
| Systems / memory | memcpy / memmove (overlap) | [memmove.md](./memmove.md), [memmove.cpp](./memmove.cpp) |
| Image | 3×3 box / Gaussian filter | [box_filter.md](./box_filter.md), [box_filter.cpp](./box_filter.cpp) |
| Image | Histogram + CDF equalize | [histogram.md](./histogram.md), [histogram.cpp](./histogram.cpp) |

## 补充题（原清单缺口）

| Topic | Drill | 为什么补 | Files |
|-------|--------|----------|--------|
| Image | 2D strided crop / rotate 90 / flip | 清单上有，无代码 | [strided_crop_rotate.md](./strided_crop_rotate.md), [.cpp](./strided_crop_rotate.cpp) |
| Image | NV12 → RGB (stride + chroma siting) | 清单上有，无代码 | [nv12_to_rgb.md](./nv12_to_rgb.md), [.cpp](./nv12_to_rgb.cpp) |
| Image | Bayer bilinear demosaic | 只有 Python 版 | [bayer_demosaic.md](./bayer_demosaic.md), [.cpp](./bayer_demosaic.cpp) |
| Image | Integral image + O(1) box + AE zones | box_filter 的必然追问 | [integral_image.md](./integral_image.md), [.cpp](./integral_image.cpp) |
| **Tracking** | **Camera–IMU 时间戳对齐 / 插值 / 时钟偏移** | **清单完全没有；Meta tracking 核心** | [timestamp_sync.md](./timestamp_sync.md), [.cpp](./timestamp_sync.cpp) |
| **Tracking** | **IR LED 连通域 + 亚像素质心** | **清单完全没有；controller tracking 核心** | [blob_centroid.md](./blob_centroid.md), [.cpp](./blob_centroid.cpp) |
| **Embedded** | **流式包解析器 + CRC + 重同步** | **清单完全没有；嵌入式电面最高频** | [packet_parser.md](./packet_parser.md), [.cpp](./packet_parser.cpp) |
| Systems / memory | 定长帧池 + 引用计数回收 | 指南列为高频，无代码 | [frame_pool.md](./frame_pool.md), [.cpp](./frame_pool.cpp) |
| Systems | ISP DAG 拓扑排序 + buffer 峰值 | 清单上有，无代码 | [isp_graph_topo.md](./isp_graph_topo.md), [.cpp](./isp_graph_topo.cpp) |
| Math | 定点 Q 格式 / 整数 sqrt / gamma LUT | 只在表格里出现过 | [fixed_point.md](./fixed_point.md), [.cpp](./fixed_point.cpp) |

## 全部构建并自测

```bash
cd Meta_Camera/code
for f in *.cpp; do
  b="${f%.cpp}"
  c++ -std=c++17 -Wall -Wextra -O2 -pthread "$f" -o "/tmp/drill_$b" && "/tmp/drill_$b" || echo "FAIL $b"
done
```

每个 `.cpp` 都是自包含的，`main()` 只跑断言测试，成功打印 `<name>: ok`。
