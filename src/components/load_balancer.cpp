#include "components/load_balancer.h"

namespace components {

LoadBalancer::LoadBalancer(engine::ComponentId id)
    : Component(std::move(id), engine::ComponentType::kLoadBalancer) {}

std::vector<engine::Event> LoadBalancer::HandleEvent(const engine::Event& event,
                                                      engine::Timestamp current_time) {
    std::vector<engine::Event> result;

    if (auto* req = std::get_if<engine::NetworkRequest>(&event)) {
        // Forward request to a downstream target
        if (targets_.empty()) return result;

        auto target = PickTarget();
        auto fwd_id = next_id_++;
        inflight_[fwd_id] = req->header.from;  // remember who sent it

        result.push_back(engine::NetworkRequest{
            engine::EventHeader{GetId(), target, current_time, fwd_id},
            req->method, req->path, req->body,
        });
    } else if (auto* resp = std::get_if<engine::NetworkResponse>(&event)) {
        // Forward response back to the original sender
        auto it = inflight_.find(resp->request_id);
        auto original_sender = (it != inflight_.end()) ? it->second : resp->header.from;
        if (it != inflight_.end()) inflight_.erase(it);

        result.push_back(engine::NetworkResponse{
            engine::EventHeader{GetId(), original_sender, current_time, resp->header.id},
            resp->status, resp->body, resp->request_id,
        });
    }

    return result;
}

lang::ScriptValue LoadBalancer::GetApiObject() {
    return lang::ScriptValue(lang::ScriptMap{});
}

void LoadBalancer::Reset() {
    rr_index_ = 0;
    inflight_.clear();
}

void LoadBalancer::AddTarget(const engine::ComponentId& target_id) {
    targets_.push_back(target_id);
}

void LoadBalancer::SetStrategy(LBStrategy strategy) {
    strategy_ = strategy;
}

engine::ComponentId LoadBalancer::PickTarget() {
    if (targets_.empty()) return "";
    auto& target = targets_[rr_index_ % targets_.size()];
    rr_index_++;
    return target;
}

}  // namespace components
