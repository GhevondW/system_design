#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/event.h"
#include "lang/value.h"

namespace engine {

enum class ComponentType : uint8_t {
    kClient,
    kHttpServer,
    kDatabase,
    kCache,
    kLoadBalancer,
};

enum class ComponentState : uint8_t {
    kCreated,
    kInitialized,
    kRunning,
    kStopped,
};

[[nodiscard]] std::string_view ComponentTypeName(ComponentType type);

class Component {
public:
    Component(ComponentId id, ComponentType type);
    virtual ~Component() = default;

    [[nodiscard]] const ComponentId& GetId() const { return id_; }
    [[nodiscard]] ComponentType GetType() const { return type_; }
    [[nodiscard]] ComponentState GetState() const { return state_; }

    void SetState(ComponentState state) { state_ = state; }

    // Process an incoming event, return any resulting events to enqueue
    virtual std::vector<Event> HandleEvent(const Event& event, Timestamp current_time) = 0;

    // Get injectable API for the scripting environment
    virtual lang::ScriptValue GetApiObject() = 0;

    // Reset to initial state
    virtual void Reset() = 0;

private:
    ComponentId id_;
    ComponentType type_;
    ComponentState state_ = ComponentState::kCreated;
};

}  // namespace engine
