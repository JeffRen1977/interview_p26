# 流式包解析器（Framing + CRC + 重同步）

**清单上原本没有。** 这是嵌入式电面里最常见的「非 LeetCode」题：字节从 UART / SPI / CSI-2 embedded data lane 以**任意长度的块**到达，你不能假设块边界就是包边界。

实现与自测：[`packet_parser.cpp`](./packet_parser.cpp)

```bash
c++ -std=c++17 -Wall -Wextra -O2 packet_parser.cpp -o packet_parser && ./packet_parser
```

---

## 1. 帧格式

```
[SYNC 0xA5 0x5A][LEN u16 LE][SEQ u8][PAYLOAD LEN 字节][CRC16 LE 覆盖 LEN..PAYLOAD]
```

## 2. 正确解法是**显式状态机**

```
Sync0 → Sync1 → Len0 → Len1 → Seq → Payload → Crc0 → Crc1 → (交付) → Sync0
```

每字节 O(1)，固定内存，**没有无限增长的累积 buffer，也没有每来一个字节就 memmove 整条流**。这是评分点。很多人第一反应是「攒到一个大 vector 里再找 SYNC」——那是 O(N²) 且内存无界，嵌入式上不可接受。

## 3. 四个必踩的坑

**① 分块边界。** 唯一有说服力的测试：同一个包在**每一个可能的位置**切成两段喂进去，结果必须完全一致。再加一个「一次喂一字节」。`.cpp` 里两个都做了。

**② `0xA5 0xA5 0x5A`。** 在 `Sync1` 状态收到 `0xA5` 时**要留在 `Sync1`**，不能退回 `Sync0`。否则前缀多一个 `0xA5` 整个包就丢了。这是最常见的实现 bug。

**③ 长度字段被污染。** `LEN` 本身没有 CRC 保护。收到 `0xFFFF` 就照单全收的话，解析器会吞掉后面所有真包直到超时。必须有 `MAX_PAYLOAD` 上限，超了立刻退回 `Sync0`。

**④ CRC 错误之后要能继续。** 丢弃这个包、计数、回到 `Sync0`，**不要重置整个连接**。测试里 CRC 错一个包之后紧跟一个好包，必须收得到。

## 4. CRC-16/CCITT-FALSE

多项式 `0x1021`，初值 `0xFFFF`，不反射，不异或输出。已知向量：`CRC("123456789") == 0x29B1`——背下来，白板上能自验。

逐位实现每字节 8 次循环；查表版 512 字节 ROM 换 8 倍速度。**先写逐位版本，然后说出查表方案**，不要一上来优化。

## 5. 追问清单

- **为什么不用转义（byte stuffing / SLIP）？** 转义不需要长度字段，抗污染更强，但会让长度变化（最坏 2 倍膨胀），DMA 定长接收就没法做了。二选一，说清 trade-off。
- **零拷贝：** 真实驱动里 payload 直接落进 DMA ring，解析器只吐 (offset, len)，不做 `memcpy`。
- **ISR 里做多少？** 只把字节推进 ring buffer / 无锁队列，状态机放到线程里跑。CRC 绝不在 ISR 里算。
- **背压：** 上层来不及消费时是丢最老还是丢最新？相机遥测流丢最老，控制指令流丢最新。
- **和相机的关系：** CSI-2 的 short/long packet 就是这个结构——DI（datatype+VC）、WC（word count）、ECC 保护包头、payload 末尾 checksum。你可以直接类比过去。
