#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "engine/component.h"

namespace components {

enum class LBStrategy : uint8_t {
    kRoundRobin,
};

// Load Balancer — distributes incoming requests across downstream servers.
// Standard settings, no user code needed. Just connect it between client and servers.
class LoadBalancer : public engine::Component {
public:
    explicit LoadBalancer(engine::ComponentId id);

    std::vector<engine::Event> HandleEvent(const engine::Event& event,
                                           engine::Timestamp current_time) override;
    lang::ScriptValue GetApiObject() override;
    void Reset() override;

    // Configure downstream targets (called during graph wiring)
    void AddTarget(const engine::ComponentId& target_id);
    void SetStrategy(LBStrategy strategy);

private:
    engine::ComponentId PickTarget();

    LBStrategy strategy_ = LBStrategy::kRoundRobin;
    std::vector<engine::ComponentId> targets_;
    size_t rr_index_ = 0;

    // Track in-flight requests: request_id → original sender
    std::unordered_map<uint64_t, engine::ComponentId> inflight_;
    uint64_t next_id_ = 1;
};

}  // namespace components
