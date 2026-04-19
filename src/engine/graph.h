#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/component.h"

namespace engine {

struct Connection {
    ComponentId from;
    ComponentId to;
    std::string alias;  // variable name injected into 'from' component's scope
};

class Graph {
public:
    void AddComponent(std::unique_ptr<Component> component);
    void AddConnection(Connection connection);

    [[nodiscard]] Component* GetComponent(const ComponentId& id) const;
    [[nodiscard]] const std::vector<Connection>& GetConnections() const;
    [[nodiscard]] const std::unordered_map<std::string, std::unique_ptr<Component>>& GetComponents()
        const;

    // Get all connections from a specific component
    [[nodiscard]] std::vector<const Connection*> GetConnectionsFrom(const ComponentId& id) const;

    void Clear();

private:
    std::unordered_map<std::string, std::unique_ptr<Component>> components_;
    std::vector<Connection> connections_;
};

}  // namespace engine
