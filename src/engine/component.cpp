#include "engine/component.h"

namespace engine {

std::string_view ComponentTypeName(ComponentType type) {
    switch (type) {
        case ComponentType::kClient: return "Client";
        case ComponentType::kHttpServer: return "HttpServer";
        case ComponentType::kDatabase: return "Database";
        case ComponentType::kCache: return "Cache";
        case ComponentType::kLoadBalancer: return "LoadBalancer";
    }
    return "Unknown";
}

Component::Component(ComponentId id, ComponentType type) : id_(std::move(id)), type_(type) {}

}  // namespace engine
