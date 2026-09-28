// Drive the limiter from many tenant worker threads and check no oversell.
//
//     make demo

#include "memory_store.h"
#include "ratelimiter.h"
#include "request.h"

#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

class Barrier {
public:
    explicit Barrier(int count) : remaining_(count), total_(count) {}
    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        const int generation = generation_;
        if (--remaining_ == 0) {
            remaining_ = total_;
            ++generation_;
            cv_.notify_all();
        } else {
            cv_.wait(lock, [&] { return generation_ != generation; });
        }
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int remaining_;
    int total_;
    int generation_ = 0;
};

struct Tally {
    int allow = 0;
    int degrade = 0;
    int deny = 0;
};

}  // namespace

int main() {
    MemoryStore store;
    RateLimiter limiter(store, 10, 1000);
    limiter.set_rule("/search", 20, 10000, 16);
    limiter.set_rule("/upload", 8, 10000, 6);

    const std::vector<std::string> tenants{"acme", "globex", "initech"};
    const int workers_per_tenant = 4;
    const int requests_per_worker = 15;
    const std::string endpoint = "/search";
    const int limit = 20;

    Barrier barrier(static_cast<int>(tenants.size()) * workers_per_tenant);
    std::mutex tally_lock;
    std::map<std::string, Tally> tallies;
    for (const std::string& tenant : tenants) tallies[tenant] = Tally{};

    std::vector<std::thread> threads;
    for (const std::string& tenant : tenants) {
        for (int w = 0; w < workers_per_tenant; ++w) {
            threads.emplace_back([&, tenant] {
                barrier.wait();
                Tally local;
                for (int i = 0; i < requests_per_worker; ++i) {
                    switch (limiter.allow(Request{tenant, endpoint, i, 1})) {
                        case Decision::Allow: ++local.allow; break;
                        case Decision::Degrade: ++local.degrade; break;
                        case Decision::Deny: ++local.deny; break;
                    }
                }
                std::lock_guard<std::mutex> guard(tally_lock);
                Tally& total = tallies[tenant];
                total.allow += local.allow;
                total.degrade += local.degrade;
                total.deny += local.deny;
            });
        }
    }
    for (auto& thread : threads) thread.join();

    std::cout << "Multi-tenant RateLimiter (thread-safe)\n";
    std::cout << "  tenants=acme,globex,initech  workers/tenant=" << workers_per_tenant
              << "  requests/worker=" << requests_per_worker << "  endpoint=" << endpoint << "\n";
    std::cout << "  rule: limit=" << limit << "  degrade_threshold=16  window_ms=10000\n\n";
    std::cout << "  " << std::left << std::setw(10) << "tenant" << std::right << std::setw(8)
              << "allow" << std::setw(9) << "degrade" << std::setw(7) << "deny" << std::setw(10)
              << "accepted" << std::setw(7) << "limit" << "\n";

    bool ok = true;
    for (const std::string& tenant : tenants) {
        const Tally& t = tallies.at(tenant);
        const int accepted = t.allow + t.degrade;
        if (accepted != limit) ok = false;
        std::cout << "  " << std::left << std::setw(10) << tenant << std::right << std::setw(8)
                  << t.allow << std::setw(9) << t.degrade << std::setw(7) << t.deny
                  << std::setw(10) << accepted << std::setw(7) << limit << "\n";
    }

    std::cout << "\n";
    if (ok) {
        std::cout << "Thread-safety check: each tenant accepted exactly its limit; no oversell.\n";
        return 0;
    }
    std::cerr << "Thread-safety check FAILED: a tenant oversold or undersold quota.\n";
    return 1;
}
