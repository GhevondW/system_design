#pragma once

#include <string>
#include <vector>

#include "engine/component.h"

namespace components {

class Client : public engine::Component {
public:
    explicit Client(engine::ComponentId id);

    std::vector<engine::Event> HandleEvent(const engine::Event& event,
                                           engine::Timestamp current_time) override;
    lang::ScriptValue GetApiObject() override;
    void Reset() override;

    // Create a request event to send
    engine::Event CreateRequest(const std::string& target_id, const std::string& method,
                                const std::string& path, lang::ScriptValue body,
                                engine::Timestamp timestamp);

    // Store received responses
    [[nodiscard]] const std::vector<engine::NetworkResponse>& GetResponses() const;

private:
    std::vector<engine::NetworkResponse> responses_;
    uint64_t next_request_id_ = 1;
};

}  // namespace components
