// Streaming byte-oriented packet framer with CRC and resync.
// The single most common "embedded, not LeetCode" phone-screen question:
// bytes arrive in arbitrary chunks from a UART / SPI / CSI-2 embedded-data
// lane, and you must not assume a chunk boundary is a packet boundary.
//
// Wire format:
//   [SYNC 0xA5 0x5A][LEN u16 LE][SEQ u8][PAYLOAD LEN bytes][CRC16 LE over LEN..PAYLOAD]

#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <vector>

static const uint8_t SYNC0 = 0xA5;
static const uint8_t SYNC1 = 0x5A;
static const uint16_t MAX_PAYLOAD = 1024;

// CRC-16/CCITT-FALSE, bitwise. A table costs 512 bytes of ROM and is the
// obvious follow-up; say that out loud rather than pre-optimising.
uint16_t crc16_ccitt(const uint8_t* data, size_t len, uint16_t crc = 0xFFFF) {
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

struct ParserStats {
    uint32_t packets_ok = 0;
    uint32_t crc_errors = 0;
    uint32_t resyncs = 0;        // bytes discarded while hunting for SYNC
    uint32_t oversize = 0;
};

// An explicit state machine. No unbounded accumulation buffer, no memmove of
// the whole stream on every byte: O(1) work per byte, fixed memory.
class PacketParser {
public:
    using Sink = std::function<void(uint8_t seq, const uint8_t* payload, uint16_t len)>;

    explicit PacketParser(Sink sink) : sink_(std::move(sink)) { reset(); }

    void reset() {
        state_ = State::Sync0;
        got_ = 0;
        len_ = 0;
        seq_ = 0;
        crc_rx_ = 0;
    }

    // Feed an arbitrary chunk. Safe to call with n == 0.
    void feed(const uint8_t* data, size_t n) {
        for (size_t i = 0; i < n; ++i) step(data[i]);
    }

    const ParserStats& stats() const { return stats_; }

private:
    enum class State { Sync0, Sync1, Len0, Len1, Seq, Payload, Crc0, Crc1 };

    void step(uint8_t b) {
        switch (state_) {
        case State::Sync0:
            if (b == SYNC0) state_ = State::Sync1;
            else ++stats_.resyncs;
            break;

        case State::Sync1:
            if (b == SYNC1) { state_ = State::Len0; }
            else if (b == SYNC0) { ++stats_.resyncs; /* stay: 0xA5 0xA5 0x5A is valid */ }
            else { ++stats_.resyncs; state_ = State::Sync0; }
            break;

        case State::Len0:
            len_ = b;
            state_ = State::Len1;
            break;

        case State::Len1:
            len_ = static_cast<uint16_t>(len_ | (static_cast<uint16_t>(b) << 8));
            if (len_ > MAX_PAYLOAD) {
                // A corrupted length would otherwise make us swallow the next
                // real packet. Drop back to hunting immediately.
                ++stats_.oversize;
                state_ = State::Sync0;
            } else {
                state_ = State::Seq;
            }
            break;

        case State::Seq:
            seq_ = b;
            got_ = 0;
            state_ = (len_ == 0) ? State::Crc0 : State::Payload;
            break;

        case State::Payload:
            payload_[got_++] = b;
            if (got_ == len_) state_ = State::Crc0;
            break;

        case State::Crc0:
            crc_rx_ = b;
            state_ = State::Crc1;
            break;

        case State::Crc1:
            crc_rx_ = static_cast<uint16_t>(crc_rx_ | (static_cast<uint16_t>(b) << 8));
            finish();
            state_ = State::Sync0;
            break;
        }
    }

    void finish() {
        uint8_t hdr[3] = {static_cast<uint8_t>(len_ & 0xFF),
                          static_cast<uint8_t>(len_ >> 8), seq_};
        uint16_t crc = crc16_ccitt(hdr, 3);
        crc = crc16_ccitt(payload_, len_, crc);
        if (crc == crc_rx_) {
            ++stats_.packets_ok;
            if (sink_) sink_(seq_, payload_, len_);
        } else {
            ++stats_.crc_errors;
        }
    }

    Sink sink_;
    State state_;
    uint16_t len_, got_, crc_rx_;
    uint8_t seq_;
    uint8_t payload_[MAX_PAYLOAD];
    ParserStats stats_;
};

// Build a valid frame, for tests and for the "write the encoder too" follow-up.
std::vector<uint8_t> encode(uint8_t seq, const uint8_t* payload, uint16_t len) {
    std::vector<uint8_t> out;
    out.push_back(SYNC0);
    out.push_back(SYNC1);
    out.push_back(static_cast<uint8_t>(len & 0xFF));
    out.push_back(static_cast<uint8_t>(len >> 8));
    out.push_back(seq);
    out.insert(out.end(), payload, payload + len);
    uint8_t hdr[3] = {out[2], out[3], out[4]};
    uint16_t crc = crc16_ccitt(hdr, 3);
    crc = crc16_ccitt(payload, len, crc);
    out.push_back(static_cast<uint8_t>(crc & 0xFF));
    out.push_back(static_cast<uint8_t>(crc >> 8));
    return out;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

struct Captured { uint8_t seq; std::vector<uint8_t> data; };

static void test_single_packet() {
    std::vector<Captured> got;
    PacketParser p([&](uint8_t s, const uint8_t* d, uint16_t n) {
        got.push_back({s, std::vector<uint8_t>(d, d + n)});
    });
    const uint8_t payload[] = {1, 2, 3, 4, 5};
    auto wire = encode(7, payload, 5);
    p.feed(wire.data(), wire.size());
    assert(got.size() == 1 && got[0].seq == 7);
    assert(got[0].data == std::vector<uint8_t>(payload, payload + 5));
    assert(p.stats().packets_ok == 1 && p.stats().crc_errors == 0);
}

static void test_split_at_every_boundary() {
    // The real test: the same packet delivered one byte at a time, and split
    // at every possible position, must parse identically.
    const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    auto wire = encode(3, payload, 4);
    for (size_t cut = 0; cut <= wire.size(); ++cut) {
        std::vector<Captured> got;
        PacketParser p([&](uint8_t s, const uint8_t* d, uint16_t n) {
            got.push_back({s, std::vector<uint8_t>(d, d + n)});
        });
        p.feed(wire.data(), cut);
        p.feed(wire.data() + cut, wire.size() - cut);
        assert(got.size() == 1 && got[0].seq == 3);
        assert(got[0].data == std::vector<uint8_t>(payload, payload + 4));
    }
    // One byte at a time.
    std::vector<Captured> got;
    PacketParser p([&](uint8_t s, const uint8_t* d, uint16_t n) {
        got.push_back({s, std::vector<uint8_t>(d, d + n)});
    });
    for (uint8_t b : wire) p.feed(&b, 1);
    assert(got.size() == 1);
}

static void test_multiple_packets_in_one_chunk() {
    std::vector<Captured> got;
    PacketParser p([&](uint8_t s, const uint8_t* d, uint16_t n) {
        got.push_back({s, std::vector<uint8_t>(d, d + n)});
    });
    std::vector<uint8_t> wire;
    for (uint8_t i = 0; i < 4; ++i) {
        const uint8_t payload[] = {i, static_cast<uint8_t>(i * 2)};
        auto w = encode(i, payload, 2);
        wire.insert(wire.end(), w.begin(), w.end());
    }
    p.feed(wire.data(), wire.size());
    assert(got.size() == 4);
    for (uint8_t i = 0; i < 4; ++i) assert(got[i].seq == i && got[i].data[0] == i);
}

static void test_garbage_before_and_between() {
    std::vector<Captured> got;
    PacketParser p([&](uint8_t s, const uint8_t* d, uint16_t n) {
        got.push_back({s, std::vector<uint8_t>(d, d + n)});
    });
    const uint8_t payload[] = {9, 9};
    auto w = encode(1, payload, 2);
    std::vector<uint8_t> wire = {0x00, 0xFF, 0xA5, 0xA5, 0x11};  // includes a false SYNC0
    wire.insert(wire.end(), w.begin(), w.end());
    wire.insert(wire.end(), {0x42, 0x43});
    wire.insert(wire.end(), w.begin(), w.end());
    p.feed(wire.data(), wire.size());
    assert(got.size() == 2);
    assert(p.stats().packets_ok == 2 && p.stats().resyncs > 0);
}

static void test_double_sync0_prefix() {
    // 0xA5 0xA5 0x5A must still frame: staying in Sync1 on a repeated 0xA5 is
    // the bug most candidates ship.
    std::vector<Captured> got;
    PacketParser p([&](uint8_t s, const uint8_t* d, uint16_t n) {
        got.push_back({s, std::vector<uint8_t>(d, d + n)});
    });
    const uint8_t payload[] = {1};
    auto w = encode(5, payload, 1);
    std::vector<uint8_t> wire = {0xA5};
    wire.insert(wire.end(), w.begin(), w.end());
    p.feed(wire.data(), wire.size());
    assert(got.size() == 1 && got[0].seq == 5);
}

static void test_crc_error_is_counted_not_delivered() {
    std::vector<Captured> got;
    PacketParser p([&](uint8_t s, const uint8_t* d, uint16_t n) {
        got.push_back({s, std::vector<uint8_t>(d, d + n)});
    });
    const uint8_t payload[] = {1, 2, 3};
    auto w = encode(2, payload, 3);
    w[6] ^= 0xFF;                        // flip a payload bit
    p.feed(w.data(), w.size());
    assert(got.empty());
    assert(p.stats().crc_errors == 1 && p.stats().packets_ok == 0);

    // The parser must still be usable afterwards.
    auto good = encode(3, payload, 3);
    p.feed(good.data(), good.size());
    assert(got.size() == 1 && got[0].seq == 3);
}

static void test_oversize_length_is_rejected_and_recovers() {
    std::vector<Captured> got;
    PacketParser p([&](uint8_t s, const uint8_t* d, uint16_t n) {
        got.push_back({s, std::vector<uint8_t>(d, d + n)});
    });
    std::vector<uint8_t> bad = {SYNC0, SYNC1, 0xFF, 0xFF, 0x00};   // len = 65535
    p.feed(bad.data(), bad.size());
    assert(p.stats().oversize == 1);
    const uint8_t payload[] = {7};
    auto good = encode(1, payload, 1);
    p.feed(good.data(), good.size());
    assert(got.size() == 1);
}

static void test_zero_length_payload() {
    std::vector<Captured> got;
    PacketParser p([&](uint8_t s, const uint8_t* d, uint16_t n) {
        got.push_back({s, std::vector<uint8_t>(d, d + n)});
    });
    auto w = encode(11, nullptr, 0);
    p.feed(w.data(), w.size());
    assert(got.size() == 1 && got[0].seq == 11 && got[0].data.empty());
}

static void test_crc_known_vector() {
    // CRC-16/CCITT-FALSE("123456789") == 0x29B1
    const uint8_t v[] = "123456789";
    assert(crc16_ccitt(v, 9) == 0x29B1);
}

int main() {
    test_crc_known_vector();
    test_single_packet();
    test_split_at_every_boundary();
    test_multiple_packets_in_one_chunk();
    test_garbage_before_and_between();
    test_double_sync0_prefix();
    test_crc_error_is_counted_not_delivered();
    test_oversize_length_is_rejected_and_recovers();
    test_zero_length_payload();
    std::cout << "packet_parser: ok\n";
    return 0;
}
