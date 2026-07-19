// Interview whiteboard: type-safe Event Dispatcher (Publish-Subscribe).
//
// Core idea: use EventType as the key (std::type_index), not string names.
// Different callback signatures are type-erased behind HandlerWrapper so one
// map can hold many event kinds.
//
// Whiteboard talking points:
// 1. Type erasure: HandlerWrapper base + EventHandler<T> holds
//    std::function<void(const T&)>; store unique_ptr<HandlerWrapper> in one map.
// 2. Compile-time safety: static_assert(is_base_of_v<Event, EventType>).
// 3. No string keys: typeid(EventType) → O(1) lookup, no hash of "OnPlayerKill".
// 4. Subscribe returns SubscriptionId; unsubscribe(id) removes that listener.
// 5. Unsubscribe during dispatch is deferred (mark dead, compact after) so we
//    never destroy a handler while its callback is on the stack.
//
// Follow-ups interviewers ask:
// - RAII Subscription guard that unsubscribes in destructor.
// - Thread safety: coarse mutex, or per-type mutex, or SPSC into one thread.
// - Performance: std::function may allocate; small-callback SSO / intrusive list.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

struct Event {
    virtual ~Event() = default;
};

struct PlayerScoreEvent : public Event {
    std::string player_name;
    int score;

    PlayerScoreEvent(std::string name, int s)
        : player_name(std::move(name)), score(s) {}
};

struct SystemWarningEvent : public Event {
    int error_code;
    std::string message;

    SystemWarningEvent(int code, std::string msg)
        : error_code(code), message(std::move(msg)) {}
};

using SubscriptionId = std::uint64_t;

class EventDispatcher {
 private:
    class HandlerWrapper {
     public:
        explicit HandlerWrapper(SubscriptionId id) : id_(id) {}
        virtual ~HandlerWrapper() = default;

        SubscriptionId id() const { return id_; }
        bool alive() const { return alive_; }
        void kill() { alive_ = false; }

     private:
        SubscriptionId id_;
        bool alive_ = true;
    };

    template <typename T>
    class EventHandler : public HandlerWrapper {
     public:
        std::function<void(const T&)> callback;

        EventHandler(SubscriptionId id, std::function<void(const T&)> cb)
            : HandlerWrapper(id), callback(std::move(cb)) {}
    };

    std::unordered_map<std::type_index,
                       std::vector<std::unique_ptr<HandlerWrapper>>>
        listeners_;
    SubscriptionId next_id_ = 1;
    int dispatch_depth_ = 0;
    bool compact_pending_ = false;

    void compact_dead_handlers() {
        for (auto& [type, vec] : listeners_) {
            (void)type;
            vec.erase(std::remove_if(vec.begin(),
                                     vec.end(),
                                     [](const std::unique_ptr<HandlerWrapper>& h) {
                                         return !h || !h->alive();
                                     }),
                      vec.end());
        }
        compact_pending_ = false;
    }

 public:
    EventDispatcher() = default;

    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    // Returns an id the caller must keep to unsubscribe later.
    template <typename EventType, typename Func>
    SubscriptionId subscribe(Func&& callback) {
        static_assert(std::is_base_of_v<Event, EventType>,
                      "EventType must derive from Event");

        const SubscriptionId id = next_id_++;
        auto handler = std::make_unique<EventHandler<EventType>>(
            id,
            std::function<void(const EventType&)>(
                std::forward<Func>(callback)));
        listeners_[typeid(EventType)].push_back(std::move(handler));
        return id;
    }

    // Idempotent: unknown / already-removed ids return false.
    bool unsubscribe(SubscriptionId id) {
        for (auto& [type, vec] : listeners_) {
            (void)type;
            for (auto& wrapper : vec) {
                if (!wrapper || wrapper->id() != id) {
                    continue;
                }
                wrapper->kill();
                if (dispatch_depth_ == 0) {
                    compact_dead_handlers();
                } else {
                    // Handler may be mid-callback; destroy after dispatch.
                    compact_pending_ = true;
                }
                return true;
            }
        }
        return false;
    }

    template <typename EventType>
    void dispatch(const EventType& event) {
        static_assert(std::is_base_of_v<Event, EventType>,
                      "EventType must derive from Event");

        auto it = listeners_.find(typeid(EventType));
        if (it == listeners_.end()) {
            return;
        }

        ++dispatch_depth_;
        for (const auto& wrapper : it->second) {
            if (!wrapper || !wrapper->alive()) {
                continue;
            }
            auto* handler =
                static_cast<EventHandler<EventType>*>(wrapper.get());
            if (handler && handler->callback) {
                handler->callback(event);
            }
        }
        --dispatch_depth_;

        if (dispatch_depth_ == 0 && compact_pending_) {
            compact_dead_handlers();
        }
    }

    template <typename EventType>
    size_t listener_count() const {
        auto it = listeners_.find(typeid(EventType));
        if (it == listeners_.end()) {
            return 0;
        }
        size_t n = 0;
        for (const auto& h : it->second) {
            if (h && h->alive()) {
                ++n;
            }
        }
        return n;
    }
};

class AchievementSystem {
 public:
    int notifications = 0;

    void onPlayerScore(const PlayerScoreEvent& event) {
        ++notifications;
        std::cout << "[Achievement] player=" << event.player_name
                  << " score=" << event.score << "\n";
    }
};

bool test_event_dispatcher() {
    EventDispatcher dispatcher;
    AchievementSystem achievements;

    int ui_hits = 0;
    int warn_hits = 0;

    const auto ui_id = dispatcher.subscribe<PlayerScoreEvent>(
        [&ui_hits](const PlayerScoreEvent& event) {
            ++ui_hits;
            std::cout << "[UI] " << event.player_name << " -> " << event.score
                      << "\n";
        });

    const auto ach_id = dispatcher.subscribe<PlayerScoreEvent>(
        [&achievements](const PlayerScoreEvent& event) {
            achievements.onPlayerScore(event);
        });

    dispatcher.subscribe<SystemWarningEvent>(
        [&warn_hits](const SystemWarningEvent& event) {
            ++warn_hits;
            std::cout << "[Warn] code=" << event.error_code
                      << " msg=" << event.message << "\n";
        });

    assert(dispatcher.listener_count<PlayerScoreEvent>() == 2);
    assert(dispatcher.listener_count<SystemWarningEvent>() == 1);

    dispatcher.dispatch(PlayerScoreEvent("Alice", 1500));
    assert(ui_hits == 1);
    assert(achievements.notifications == 1);
    assert(warn_hits == 0);

    // Unsubscribe UI listener; achievement listener remains.
    assert(dispatcher.unsubscribe(ui_id));
    assert(!dispatcher.unsubscribe(ui_id));  // idempotent
    assert(dispatcher.listener_count<PlayerScoreEvent>() == 1);

    dispatcher.dispatch(PlayerScoreEvent("Bob", 2000));
    assert(ui_hits == 1);  // UI no longer called
    assert(achievements.notifications == 2);

    dispatcher.dispatch(SystemWarningEvent(504, "Gateway Timeout"));
    assert(warn_hits == 1);

    // Unsubscribe during dispatch: deferred compact, still safe.
    const auto self_unsub_id = dispatcher.subscribe<PlayerScoreEvent>(
        [&](const PlayerScoreEvent&) {
            assert(dispatcher.unsubscribe(ach_id));
        });
    dispatcher.dispatch(PlayerScoreEvent("Carol", 1));
    assert(dispatcher.listener_count<PlayerScoreEvent>() == 1);  // only self
    assert(dispatcher.unsubscribe(self_unsub_id));
    assert(dispatcher.listener_count<PlayerScoreEvent>() == 0);

    struct UnusedEvent : public Event {};
    dispatcher.dispatch(UnusedEvent{});

    return true;
}

int main() {
    assert(test_event_dispatcher());
    std::cout << "18_event_dispatcher: ok\n";
    return 0;
}
