// Camera <-> IMU timestamp alignment: the drill Meta tracking work actually
// runs on. Merge sorted streams, interpolate a sample at an exact instant,
// estimate the clock offset between two domains, detect dropped frames.
//
// All times are int64_t nanoseconds. Never use float for absolute timestamps:
// a double has 53 mantissa bits, and boot-relative ns burns ~50 of them within
// a few weeks of uptime.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

struct ImuSample {
    int64_t t_ns;
    double gx, gy, gz;   // rad/s
};

struct FrameMeta {
    int64_t sof_ns;        // start-of-frame, sensor clock
    int64_t exposure_ns;   // integration time
    int64_t readout_ns;    // full-frame readout (0 for global shutter)
    uint32_t seq;          // sensor frame counter
};

// ---------------------------------------------------------------------------
// 1. The timestamp that actually matters.
//
// A frame is not an instant. The pose you fuse against must be the *exposure
// midpoint*, not SOF and not EOF. For a rolling-shutter sensor, row y is
// exposed later than row 0, so the midpoint is row-dependent.
// ---------------------------------------------------------------------------

int64_t exposure_midpoint_ns(const FrameMeta& f) {
    return f.sof_ns + f.exposure_ns / 2;
}

// Midpoint for row `y` of `height` on a rolling-shutter sensor.
int64_t exposure_midpoint_row_ns(const FrameMeta& f, int y, int height) {
    const int64_t row_delay = (height > 1)
        ? f.readout_ns * y / (height - 1)
        : 0;
    return f.sof_ns + row_delay + f.exposure_ns / 2;
}

// ---------------------------------------------------------------------------
// 2. Find the bracketing IMU samples for a query time, then interpolate.
//    imu must be sorted ascending by t_ns.
// ---------------------------------------------------------------------------

// Index of the last sample with t_ns <= t, or -1 if t precedes the buffer.
int lower_bracket(const std::vector<ImuSample>& imu, int64_t t) {
    int lo = 0, hi = static_cast<int>(imu.size());     // search in [lo, hi)
    while (lo < hi) {
        const int mid = lo + (hi - lo) / 2;            // no lo+hi overflow
        if (imu[mid].t_ns <= t) lo = mid + 1;
        else hi = mid;
    }
    return lo - 1;
}

// Nearest sample by absolute time distance. Returns -1 on an empty buffer.
int nearest_index(const std::vector<ImuSample>& imu, int64_t t) {
    if (imu.empty()) return -1;
    const int i = lower_bracket(imu, t);
    if (i < 0) return 0;
    if (i + 1 >= static_cast<int>(imu.size())) return static_cast<int>(imu.size()) - 1;
    const int64_t da = t - imu[i].t_ns;
    const int64_t db = imu[i + 1].t_ns - t;
    return (da <= db) ? i : i + 1;
}

// Linear interpolation at time t. Returns false if t falls outside the buffer
// (never extrapolate a gyro across a gap — say so and drop the frame instead).
bool interpolate_imu(const std::vector<ImuSample>& imu, int64_t t,
                     int64_t max_gap_ns, ImuSample* out) {
    if (imu.size() < 2) return false;
    const int i = lower_bracket(imu, t);
    if (i < 0 || i + 1 >= static_cast<int>(imu.size())) return false;  // out of range

    const ImuSample& a = imu[i];
    const ImuSample& b = imu[i + 1];
    const int64_t span = b.t_ns - a.t_ns;
    if (span <= 0) return false;
    if (span > max_gap_ns) return false;   // a dropped IMU burst: refuse to fake it

    // Do the ratio in double only after reducing to a small span.
    const double w = static_cast<double>(t - a.t_ns) / static_cast<double>(span);
    out->t_ns = t;
    out->gx = a.gx + w * (b.gx - a.gx);
    out->gy = a.gy + w * (b.gy - a.gy);
    out->gz = a.gz + w * (b.gz - a.gz);
    return true;
}

// ---------------------------------------------------------------------------
// 3. Merge two sorted streams into one time-ordered event list.
//    Classic "merge k sorted" dressed as sensor fusion input.
// ---------------------------------------------------------------------------

enum class Src { Cam, Imu };
struct Event { int64_t t_ns; Src src; int idx; };

std::vector<Event> merge_streams(const std::vector<FrameMeta>& cam,
                                 const std::vector<ImuSample>& imu) {
    std::vector<Event> out;
    out.reserve(cam.size() + imu.size());
    size_t i = 0, j = 0;
    while (i < cam.size() || j < imu.size()) {
        const bool take_cam =
            (j == imu.size()) ||
            (i < cam.size() && exposure_midpoint_ns(cam[i]) <= imu[j].t_ns);
        if (take_cam) {
            out.push_back({exposure_midpoint_ns(cam[i]), Src::Cam, static_cast<int>(i)});
            ++i;
        } else {
            out.push_back({imu[j].t_ns, Src::Imu, static_cast<int>(j)});
            ++j;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// 4. Clock-offset estimation between two domains.
//
// Camera hardware timestamps live in the sensor/CSI clock; the IMU lives in
// the SoC clock. You observe pairs (t_local, t_remote) polluted by a positive,
// variable transport delay. The *minimum* observed difference is the best
// offset estimate, because the fastest observed sample carries the least
// delay — averaging is wrong, it just averages in the queueing noise.
// ---------------------------------------------------------------------------

class ClockOffsetEstimator {
public:
    explicit ClockOffsetEstimator(size_t window = 64) : window_(window) {}

    void observe(int64_t t_local, int64_t t_remote) {
        diffs_.push_back(t_local - t_remote);
        if (diffs_.size() > window_) diffs_.erase(diffs_.begin());
    }

    bool ready() const { return !diffs_.empty(); }

    // offset such that: t_local ≈ t_remote + offset
    int64_t offset_ns() const {
        return *std::min_element(diffs_.begin(), diffs_.end());
    }

    // Spread of the window — a proxy for how much you can trust the offset.
    int64_t jitter_ns() const {
        auto mm = std::minmax_element(diffs_.begin(), diffs_.end());
        return *mm.second - *mm.first;
    }

    int64_t to_local(int64_t t_remote) const { return t_remote + offset_ns(); }

private:
    size_t window_;
    std::vector<int64_t> diffs_;
};

// ---------------------------------------------------------------------------
// 5. Drop detection. Two independent signals: the sensor's own frame counter,
//    and the timestamp delta. Trust the counter; use the delta as a cross-check
//    that catches a counter that wrapped or was reset by a mode switch.
// ---------------------------------------------------------------------------

struct DropReport { uint32_t missing; bool timestamp_disagrees; };

DropReport detect_drops(const FrameMeta& prev, const FrameMeta& cur,
                        int64_t nominal_period_ns) {
    DropReport rep{0, false};
    rep.missing = cur.seq - prev.seq - 1;     // unsigned wrap is well defined

    const int64_t dt = cur.sof_ns - prev.sof_ns;
    // Round to the nearest whole period; expect exactly 1 for a clean stream.
    const int64_t periods = (dt + nominal_period_ns / 2) / nominal_period_ns;
    const uint32_t missing_by_time =
        (periods > 1) ? static_cast<uint32_t>(periods - 1) : 0;
    rep.timestamp_disagrees = (missing_by_time != rep.missing);
    return rep;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static std::vector<ImuSample> make_imu(int64_t t0, int64_t period, int n) {
    std::vector<ImuSample> v;
    for (int i = 0; i < n; ++i) {
        const double x = static_cast<double>(i);
        v.push_back({t0 + i * period, x, 2 * x, -x});
    }
    return v;
}

static void test_midpoint() {
    FrameMeta f{1000, 200, 0, 7};
    assert(exposure_midpoint_ns(f) == 1100);
    // Rolling shutter, 100 rows, 990 ns readout: last row is 990 ns later.
    FrameMeta rs{1000, 200, 990, 7};
    assert(exposure_midpoint_row_ns(rs, 0, 100) == 1100);
    assert(exposure_midpoint_row_ns(rs, 99, 100) == 1000 + 990 + 100);
    // Global shutter: every row identical.
    assert(exposure_midpoint_row_ns(f, 0, 100) == exposure_midpoint_row_ns(f, 99, 100));
}

static void test_bracket_and_nearest() {
    auto imu = make_imu(1000, 100, 5);   // 1000,1100,1200,1300,1400
    assert(lower_bracket(imu, 999) == -1);
    assert(lower_bracket(imu, 1000) == 0);
    assert(lower_bracket(imu, 1150) == 1);
    assert(lower_bracket(imu, 1400) == 4);
    assert(lower_bracket(imu, 99999) == 4);

    assert(nearest_index(imu, 900) == 0);
    assert(nearest_index(imu, 1140) == 1);
    assert(nearest_index(imu, 1160) == 2);
    assert(nearest_index(imu, 1150) == 1);   // exact tie -> earlier
    assert(nearest_index(imu, 99999) == 4);
    assert(nearest_index({}, 0) == -1);
}

static void test_interpolate() {
    auto imu = make_imu(1000, 100, 5);
    ImuSample s{};
    assert(interpolate_imu(imu, 1150, 1000, &s));
    assert(s.t_ns == 1150);
    assert(std::abs(s.gx - 1.5) < 1e-9);   // between sample 1 (gx=1) and 2 (gx=2)
    assert(std::abs(s.gy - 3.0) < 1e-9);

    // Exactly on a sample.
    assert(interpolate_imu(imu, 1200, 1000, &s) && std::abs(s.gx - 2.0) < 1e-9);
    // Outside the buffer: must refuse, not extrapolate.
    assert(!interpolate_imu(imu, 500, 1000, &s));
    assert(!interpolate_imu(imu, 5000, 1000, &s));
    // Gap larger than the tolerance: must refuse.
    std::vector<ImuSample> gappy{{0, 0, 0, 0}, {1'000'000, 1, 1, 1}};
    assert(!interpolate_imu(gappy, 500'000, 100'000, &s));
}

static void test_merge_is_sorted() {
    std::vector<FrameMeta> cam{{1000, 200, 0, 0}, {2000, 200, 0, 1}};  // mids 1100, 2100
    auto imu = make_imu(950, 400, 4);                                  // 950,1350,1750,2150
    auto ev = merge_streams(cam, imu);
    assert(ev.size() == 6);
    for (size_t i = 1; i < ev.size(); ++i) assert(ev[i - 1].t_ns <= ev[i].t_ns);
    assert(ev[0].src == Src::Imu && ev[0].t_ns == 950);
    assert(ev[1].src == Src::Cam && ev[1].t_ns == 1100);
    assert(ev.back().t_ns == 2150);
}

static void test_clock_offset_takes_the_minimum() {
    ClockOffsetEstimator est(8);
    const int64_t truth = 4200;
    const int64_t delays[] = {900, 130, 4000, 55, 700, 2100};  // always positive
    for (int i = 0; i < 6; ++i) {
        const int64_t t_remote = 10'000 + 1000 * i;
        est.observe(t_remote + truth + delays[i], t_remote);
    }
    assert(est.ready());
    assert(est.offset_ns() == truth + 55);      // best (least delayed) observation
    assert(est.jitter_ns() == 4000 - 55);
    assert(est.to_local(0) == truth + 55);
}

static void test_detect_drops() {
    const int64_t T = 11'111'111;   // 90 fps
    FrameMeta a{0, 100, 0, 10};
    FrameMeta b{T, 100, 0, 11};
    auto clean = detect_drops(a, b, T);
    assert(clean.missing == 0 && !clean.timestamp_disagrees);

    FrameMeta c{3 * T, 100, 0, 13};          // 2 frames lost, both signals agree
    auto lost = detect_drops(a, c, T);
    assert(lost.missing == 2 && !lost.timestamp_disagrees);

    FrameMeta d{3 * T, 100, 0, 11};          // counter says 0, clock says 2
    auto bad = detect_drops(a, d, T);
    assert(bad.missing == 0 && bad.timestamp_disagrees);

    // Counter wrap must not report ~4 billion missing frames.
    FrameMeta hi{0, 100, 0, 0xFFFFFFFFu};
    FrameMeta wrapped{T, 100, 0, 0};
    auto w = detect_drops(hi, wrapped, T);
    assert(w.missing == 0 && !w.timestamp_disagrees);
}

int main() {
    test_midpoint();
    test_bracket_and_nearest();
    test_interpolate();
    test_merge_is_sorted();
    test_clock_offset_takes_the_minimum();
    test_detect_drops();
    std::cout << "timestamp_sync: ok\n";
    return 0;
}
