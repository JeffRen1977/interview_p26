// Injectable clock. Tests freeze time with FakeClock; production uses WallClock.
// Do not change this file during the interview.

#pragma once

#include <chrono>

class Clock {
public:
    virtual ~Clock() = default;
    virtual double now() const = 0;
};

class WallClock : public Clock {
public:
    double now() const override {
        using namespace std::chrono;
        return duration<double>(system_clock::now().time_since_epoch()).count();
    }
};

// Deterministic clock. `now` is a unix-ish timestamp, not "seconds since start".
class FakeClock : public Clock {
public:
    explicit FakeClock(double now = 0.0) : now_(now) {}

    double now() const override { return now_; }

    void advance(double seconds) { now_ += seconds; }

private:
    double now_;
};
