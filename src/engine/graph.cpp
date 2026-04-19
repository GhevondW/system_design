#include "engine/graph.h"

namespace engine {

void Graph::AddComponent(std::unique_ptr<Component> component) {
    auto id = component->GetId();
    components_[id] = std::move(component);
}

void Graph::AddConnection(Connection connection) {
    connections_.push_back(std::move(connection));
}

Component* Graph::GetComponent(const ComponentId& id) const {
    auto it = components_.find(id);
    if (it != components_.end()) return it->second.get();
    return nullptr;
}

const std::vector<Connection>& Graph::GetConnections() const {
    return connections_;
}

const std::unordered_map<std::string, std::unique_ptr<Component>>& Graph::GetComponents() const {
    return components_;
}

std::vector<const Connection*> Graph::GetConnectionsFrom(const ComponentId& id) const {
    std::vector<const Connection*> result;
    for (const auto& conn : connections_) {
        if (conn.from == id) result.push_back(&conn);
    }
    return result;
}

void Graph::Clear() {
    components_.clear();
    connections_.clear();
}

}  // namespace engine
