#pragma once

#include <cstdint>
#include <optional>
#include <queue>
#include <vector>

#include "engine/event.h"

namespace engine {

class EventQueue {
public:
    void Push(Event event);
    [[nodiscard]] std::optional<Event> Pop();
    [[nodiscard]] bool Empty() const;
    [[nodiscard]] size_t Size() const;
    [[nodiscard]] std::optional<Timestamp> PeekTimestamp() const;
    void Clear();

private:
    struct EventCompare {
        bool operator()(const Event& a, const Event& b) const {
            return GetTimestamp(a) > GetTimestamp(b);  // min-heap
        }
    };

    std::priority_queue<Event, std::vector<Event>, EventCompare> queue_;
};

// Latency config per connection type
struct LatencyConfig {
    Timestamp network_latency = 1;   // ms
    Timestamp db_latency = 5;        // ms
    Timestamp cache_latency = 1;     // ms
    Timestamp queue_latency = 2;     // ms
};

}  // namespace engine
