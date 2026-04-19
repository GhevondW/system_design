#include "components/client.h"

namespace components {

Client::Client(engine::ComponentId id)
    : Component(std::move(id), engine::ComponentType::kClient) {}

std::vector<engine::Event> Client::HandleEvent(const engine::Event& event,
                                               engine::Timestamp /*current_time*/) {
    // Store responses
    if (auto* resp = std::get_if<engine::NetworkResponse>(&event)) {
        responses_.push_back(*resp);
    }
    return {};
}

lang::ScriptValue Client::GetApiObject() {
    return lang::ScriptValue(lang::ScriptMap{});
}

void Client::Reset() {
    responses_.clear();
    next_request_id_ = 1;
}

engine::Event Client::CreateRequest(const std::string& target_id, const std::string& method,
                                    const std::string& path, lang::ScriptValue body,
                                    engine::Timestamp timestamp) {
    engine::NetworkRequest req;
    req.header.from = GetId();
    req.header.to = target_id;
    req.header.timestamp = timestamp;
    req.header.id = next_request_id_++;
    req.method = method;
    req.path = path;
    req.body = std::move(body);
    return req;
}

const std::vector<engine::NetworkResponse>& Client::GetResponses() const {
    return responses_;
}

}  // namespace components
