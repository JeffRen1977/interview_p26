# Meta Camera — coding drills

Runnable C++ for the Embedded coding loop. Full map: [`../coding_interview_guide.md`](../coding_interview_guide.md).

| Topic | Drill | Files |
|-------|--------|--------|
| Bit / bytes | Packed RAW10 unpack | [mipi_raw10_unpack.md](./mipi_raw10_unpack.md), [unpack_mipi_raw10.cpp](./unpack_mipi_raw10.cpp) |
| Bit / bytes | Circular shift | [bit_rotation.md](./bit_rotation.md), [bit_rotation.cpp](./bit_rotation.cpp) |
| Bit / bytes | Endianness detect / swap | [endianness.md](./endianness.md), [endianness.cpp](./endianness.cpp) |
| Systems / memory | Aligned malloc / free | [aligned_malloc.md](./aligned_malloc.md), [aligned_malloc.cpp](./aligned_malloc.cpp) |
| Systems / memory | memcpy / memmove (overlap) | [memmove.md](./memmove.md), [memmove.cpp](./memmove.cpp) |
| Image | 3×3 box / Gaussian filter | [box_filter.md](./box_filter.md), [box_filter.cpp](./box_filter.cpp) |
| Image | Histogram + CDF equalize | [histogram.md](./histogram.md), [histogram.cpp](./histogram.cpp) |

```bash
cd Meta_Camera/code
for f in unpack_mipi_raw10 bit_rotation endianness aligned_malloc memmove box_filter histogram; do
  out=$f
  [ "$f" = memmove ] && out=memmove_drill
  c++ -std=c++17 -Wall -Wextra -O2 ${f}.cpp -o $out && ./$out
done
```
