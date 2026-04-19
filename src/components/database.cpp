#include "components/database.h"

#include <algorithm>

namespace components {

Database::Database(engine::ComponentId id) : Component(std::move(id), engine::ComponentType::kDatabase) {}

std::vector<engine::Event> Database::HandleEvent(const engine::Event& /*event*/,
                                                  engine::Timestamp /*current_time*/) {
    return {};
}

lang::ScriptValue Database::GetApiObject() {
    return lang::ScriptValue(lang::ScriptMap{});
}

void Database::Reset() {
    for (auto& [name, table] : tables_) {
        table.rows.clear();
    }
}

void Database::CreateTable(const std::string& name, const std::vector<Column>& columns) {
    tables_[name] = Table{columns, {}};
}

void Database::CreateTable(const std::string& name, const std::vector<std::string>& column_names) {
    std::vector<Column> columns;
    columns.reserve(column_names.size());
    for (const auto& col_name : column_names) {
        columns.push_back(Column{col_name, ColumnType::kAny});
    }
    tables_[name] = Table{std::move(columns), {}};
}

// --- Schema introspection ---

bool Database::HasTable(const std::string& name) const {
    return tables_.contains(name);
}

const std::vector<Column>* Database::GetSchema(const std::string& table) const {
    auto it = tables_.find(table);
    if (it == tables_.end()) return nullptr;
    return &it->second.columns;
}

// --- CRUD Operations ---

lang::ScriptValue Database::Insert(const std::string& table, const lang::ScriptMap& row) {
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return lang::ScriptValue::Error("Table not found: " + table);
    }

    // Validate all provided columns exist in schema and types match
    auto err = ValidateColumns(it->second, row, "Insert");
    if (err.IsError()) return err;

    it->second.rows.push_back(row);
    return lang::ScriptValue(lang::ScriptMap{{"inserted", lang::ScriptValue(true)}});
}

lang::ScriptValue Database::Find(const std::string& table, const lang::ScriptMap& filter) {
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return lang::ScriptValue::Error("Table not found: " + table);
    }

    auto err = ValidateFilterColumns(it->second, filter);
    if (err.IsError()) return err;

    lang::ScriptList results;
    for (const auto& row : it->second.rows) {
        if (MatchesFilter(row, filter)) {
            results.push_back(lang::ScriptValue(lang::ScriptMap(row)));
        }
    }
    return lang::ScriptValue::List(std::move(results));
}

lang::ScriptValue Database::FindOne(const std::string& table, const lang::ScriptMap& filter) {
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return lang::ScriptValue::Error("Table not found: " + table);
    }

    auto err = ValidateFilterColumns(it->second, filter);
    if (err.IsError()) return err;

    for (const auto& row : it->second.rows) {
        if (MatchesFilter(row, filter)) {
            return lang::ScriptValue(lang::ScriptMap(row));
        }
    }
    return lang::ScriptValue::Null();
}

lang::ScriptValue Database::Update(const std::string& table, const lang::ScriptMap& filter,
                                    const lang::ScriptMap& updates) {
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return lang::ScriptValue::Error("Table not found: " + table);
    }

    auto err = ValidateFilterColumns(it->second, filter);
    if (err.IsError()) return err;

    err = ValidateColumns(it->second, updates, "Update");
    if (err.IsError()) return err;

    int64_t count = 0;
    for (auto& row : it->second.rows) {
        if (MatchesFilter(row, filter)) {
            for (const auto& [key, val] : updates) {
                row[key] = val;
            }
            count++;
        }
    }
    return lang::ScriptValue(lang::ScriptMap{{"updated", lang::ScriptValue(count)}});
}

lang::ScriptValue Database::Delete(const std::string& table, const lang::ScriptMap& filter) {
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return lang::ScriptValue::Error("Table not found: " + table);
    }

    auto err = ValidateFilterColumns(it->second, filter);
    if (err.IsError()) return err;

    auto& rows = it->second.rows;
    auto original_size = static_cast<int64_t>(rows.size());
    rows.erase(
        std::remove_if(rows.begin(), rows.end(),
                       [&](const lang::ScriptMap& row) { return MatchesFilter(row, filter); }),
        rows.end());
    auto deleted = original_size - static_cast<int64_t>(rows.size());
    return lang::ScriptValue(lang::ScriptMap{{"deleted", lang::ScriptValue(deleted)}});
}

lang::ScriptValue Database::All(const std::string& table) {
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return lang::ScriptValue::Error("Table not found: " + table);
    }

    lang::ScriptList results;
    for (const auto& row : it->second.rows) {
        results.push_back(lang::ScriptValue(lang::ScriptMap(row)));
    }
    return lang::ScriptValue::List(std::move(results));
}

lang::ScriptValue Database::Count(const std::string& table, const lang::ScriptMap& filter) {
    auto it = tables_.find(table);
    if (it == tables_.end()) {
        return lang::ScriptValue::Error("Table not found: " + table);
    }

    auto err = ValidateFilterColumns(it->second, filter);
    if (err.IsError()) return err;

    int64_t count = 0;
    for (const auto& row : it->second.rows) {
        if (MatchesFilter(row, filter)) {
            count++;
        }
    }
    return lang::ScriptValue(count);
}

// --- Private: Validation ---

lang::ScriptValue Database::ValidateColumns(const Table& table, const lang::ScriptMap& data,
                                             const std::string& operation) const {
    for (const auto& [key, val] : data) {
        // Check column exists in schema
        auto col_it = std::find_if(table.columns.begin(), table.columns.end(),
                                    [&](const Column& c) { return c.name == key; });
        if (col_it == table.columns.end()) {
            std::string valid_cols;
            for (size_t i = 0; i < table.columns.size(); ++i) {
                if (i > 0) valid_cols += ", ";
                valid_cols += table.columns[i].name;
            }
            return lang::ScriptValue::Error(
                operation + ": unknown column '" + key + "'. Valid columns: [" + valid_cols + "]");
        }

        // Validate type if not kAny
        if (col_it->type != ColumnType::kAny && !ValidateType(col_it->type, val)) {
            return lang::ScriptValue::Error(
                operation + ": column '" + key + "' expects " + TypeName(col_it->type) +
                " but got " + std::string(val.TypeName()));
        }
    }
    return lang::ScriptValue::Null();  // no error
}

lang::ScriptValue Database::ValidateFilterColumns(const Table& table,
                                                   const lang::ScriptMap& filter) const {
    for (const auto& [key, val] : filter) {
        auto col_it = std::find_if(table.columns.begin(), table.columns.end(),
                                    [&](const Column& c) { return c.name == key; });
        if (col_it == table.columns.end()) {
            std::string valid_cols;
            for (size_t i = 0; i < table.columns.size(); ++i) {
                if (i > 0) valid_cols += ", ";
                valid_cols += table.columns[i].name;
            }
            return lang::ScriptValue::Error(
                "Filter: unknown column '" + key + "'. Valid columns: [" + valid_cols + "]");
        }
    }
    return lang::ScriptValue::Null();  // no error
}

bool Database::ValidateType(ColumnType expected, const lang::ScriptValue& value) const {
    switch (expected) {
        case ColumnType::kString: return value.IsString();
        case ColumnType::kInt: return value.IsInt();
        case ColumnType::kFloat: return value.IsFloat() || value.IsInt();  // int promotes to float
        case ColumnType::kBool: return value.IsBool();
        case ColumnType::kAny: return true;
    }
    return true;
}

std::string Database::TypeName(ColumnType type) const {
    switch (type) {
        case ColumnType::kString: return "string";
        case ColumnType::kInt: return "int";
        case ColumnType::kFloat: return "float";
        case ColumnType::kBool: return "bool";
        case ColumnType::kAny: return "any";
    }
    return "unknown";
}

// --- Private: Filtering ---

bool Database::MatchesFilter(const lang::ScriptMap& row, const lang::ScriptMap& filter) const {
    for (const auto& [key, val] : filter) {
        auto it = row.find(key);
        if (it == row.end() || !it->second.Equals(val)) {
            return false;
        }
    }
    return true;
}

}  // namespace components
