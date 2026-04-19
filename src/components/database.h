#pragma once

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/component.h"

namespace components {

// In-memory simulated database with basic SQL-like operations
class Database : public engine::Component {
public:
    explicit Database(engine::ComponentId id);

    std::vector<engine::Event> HandleEvent(const engine::Event& event,
                                           engine::Timestamp current_time) override;
    lang::ScriptValue GetApiObject() override;
    void Reset() override;

    // Direct API (used by the interpreter via injected functions)
    lang::ScriptValue Query(const std::string& sql, const lang::ScriptValue& params);
    lang::ScriptValue Execute(const std::string& sql, const lang::ScriptValue& params);
    void CreateTable(const std::string& name, const std::vector<std::string>& columns);

private:
    struct Table {
        std::vector<std::string> columns;
        std::vector<lang::ScriptMap> rows;
    };

    std::unordered_map<std::string, Table> tables_;
};

}  // namespace components
