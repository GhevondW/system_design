#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "engine/component.h"

namespace components {

// Column type for schema validation
enum class ColumnType { kAny, kString, kInt, kFloat, kBool };

struct Column {
    std::string name;
    ColumnType type = ColumnType::kAny;
};

// In-memory simulated relational database with schema validation.
// Operations validate against the defined schema and return errors on violations.
class Database : public engine::Component {
public:
    explicit Database(engine::ComponentId id);

    std::vector<engine::Event> HandleEvent(const engine::Event& event,
                                           engine::Timestamp current_time) override;
    lang::ScriptValue GetApiObject() override;
    void Reset() override;

    // Schema definition
    void CreateTable(const std::string& name, const std::vector<Column>& columns);
    void CreateTable(const std::string& name, const std::vector<std::string>& column_names);

    // CRUD — functional API with schema validation
    lang::ScriptValue Insert(const std::string& table, const lang::ScriptMap& row);
    lang::ScriptValue Find(const std::string& table, const lang::ScriptMap& filter);
    lang::ScriptValue FindOne(const std::string& table, const lang::ScriptMap& filter);
    lang::ScriptValue Update(const std::string& table, const lang::ScriptMap& filter,
                             const lang::ScriptMap& updates);
    lang::ScriptValue Delete(const std::string& table, const lang::ScriptMap& filter);
    lang::ScriptValue All(const std::string& table);
    lang::ScriptValue Count(const std::string& table, const lang::ScriptMap& filter);

    // Schema introspection
    bool HasTable(const std::string& name) const;
    const std::vector<Column>* GetSchema(const std::string& table) const;

private:
    struct Table {
        std::vector<Column> columns;
        std::vector<lang::ScriptMap> rows;
    };

    bool MatchesFilter(const lang::ScriptMap& row, const lang::ScriptMap& filter) const;
    lang::ScriptValue ValidateColumns(const Table& table, const lang::ScriptMap& data,
                                       const std::string& operation) const;
    lang::ScriptValue ValidateFilterColumns(const Table& table, const lang::ScriptMap& filter) const;
    bool ValidateType(ColumnType expected, const lang::ScriptValue& value) const;
    std::string TypeName(ColumnType type) const;

    std::unordered_map<std::string, Table> tables_;
};

}  // namespace components
