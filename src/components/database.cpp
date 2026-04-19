#include "components/database.h"

#include <algorithm>
#include <sstream>

namespace components {

Database::Database(engine::ComponentId id) : Component(std::move(id), engine::ComponentType::kDatabase) {}

std::vector<engine::Event> Database::HandleEvent(const engine::Event& /*event*/,
                                                  engine::Timestamp /*current_time*/) {
    // Database responds synchronously in the interpreter via injected API
    return {};
}

lang::ScriptValue Database::GetApiObject() {
    return lang::ScriptValue(lang::ScriptMap{});  // placeholder, methods are injected separately
}

void Database::Reset() {
    tables_.clear();
}

void Database::CreateTable(const std::string& name, const std::vector<std::string>& columns) {
    tables_[name] = Table{columns, {}};
}

// Simple SQL parser for: SELECT, INSERT, UPDATE, DELETE
lang::ScriptValue Database::Query(const std::string& sql, const lang::ScriptValue& params) {
    // Very basic SQL parsing
    std::istringstream iss(sql);
    std::string cmd;
    iss >> cmd;

    // Normalize to uppercase
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    if (cmd == "SELECT") {
        // SELECT * FROM table WHERE col = ?
        std::string star, from_kw, table_name;
        iss >> star >> from_kw >> table_name;

        auto table_it = tables_.find(table_name);
        if (table_it == tables_.end()) {
            return lang::ScriptValue::Error("Table not found: " + table_name);
        }

        const auto& table = table_it->second;

        // Check for WHERE clause
        std::string where_kw;
        if (iss >> where_kw) {
            std::transform(where_kw.begin(), where_kw.end(), where_kw.begin(), ::toupper);
            if (where_kw == "WHERE") {
                std::string col, eq;
                iss >> col >> eq;  // col = ?

                // Get the parameter value
                lang::ScriptValue param_val;
                if (params.IsList() && !params.AsList().empty()) {
                    param_val = params.AsList()[0];
                }

                lang::ScriptList results;
                for (const auto& row : table.rows) {
                    auto it = row.find(col);
                    if (it != row.end() && it->second.Equals(param_val)) {
                        results.push_back(lang::ScriptValue(lang::ScriptMap(row)));
                    }
                }
                return lang::ScriptValue::List(std::move(results));
            }
        }

        // No WHERE — return all rows
        lang::ScriptList results;
        for (const auto& row : table.rows) {
            results.push_back(lang::ScriptValue(lang::ScriptMap(row)));
        }
        return lang::ScriptValue::List(std::move(results));
    }

    return lang::ScriptValue::Error("Unsupported query: " + sql);
}

lang::ScriptValue Database::Execute(const std::string& sql, const lang::ScriptValue& params) {
    std::istringstream iss(sql);
    std::string cmd;
    iss >> cmd;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    if (cmd == "INSERT") {
        // INSERT INTO table (col1, col2) VALUES (?, ?)
        std::string into_kw, table_name;
        iss >> into_kw >> table_name;

        auto table_it = tables_.find(table_name);
        if (table_it == tables_.end()) {
            return lang::ScriptValue::Error("Table not found: " + table_name);
        }

        auto& table = table_it->second;

        // Parse column list from SQL: (col1, col2)
        std::string rest;
        std::getline(iss, rest);
        std::vector<std::string> columns;

        auto paren_start = rest.find('(');
        auto paren_end = rest.find(')');
        if (paren_start != std::string::npos && paren_end != std::string::npos) {
            std::string col_list = rest.substr(paren_start + 1, paren_end - paren_start - 1);
            std::istringstream col_stream(col_list);
            std::string col;
            while (std::getline(col_stream, col, ',')) {
                // Trim whitespace
                col.erase(0, col.find_first_not_of(" \t"));
                col.erase(col.find_last_not_of(" \t") + 1);
                if (!col.empty()) columns.push_back(col);
            }
        }

        // Build row from params
        lang::ScriptMap row;
        if (params.IsList()) {
            const auto& vals = params.AsList();
            for (size_t i = 0; i < columns.size() && i < vals.size(); ++i) {
                row[columns[i]] = vals[i];
            }
        }

        table.rows.push_back(std::move(row));
        return lang::ScriptValue(lang::ScriptMap{{"affected_rows", lang::ScriptValue(1)}});
    }

    if (cmd == "DELETE") {
        // DELETE FROM table WHERE col = ?
        std::string from_kw, table_name, where_kw, col, eq;
        iss >> from_kw >> table_name >> where_kw >> col >> eq;

        auto table_it = tables_.find(table_name);
        if (table_it == tables_.end()) {
            return lang::ScriptValue::Error("Table not found: " + table_name);
        }

        auto& table = table_it->second;

        lang::ScriptValue param_val;
        if (params.IsList() && !params.AsList().empty()) {
            param_val = params.AsList()[0];
        }

        auto original_size = static_cast<int64_t>(table.rows.size());
        table.rows.erase(
            std::remove_if(table.rows.begin(), table.rows.end(),
                           [&](const lang::ScriptMap& row) {
                               auto it = row.find(col);
                               return it != row.end() && it->second.Equals(param_val);
                           }),
            table.rows.end());

        auto deleted = original_size - static_cast<int64_t>(table.rows.size());
        return lang::ScriptValue(
            lang::ScriptMap{{"affected_rows", lang::ScriptValue(deleted)}});
    }

    if (cmd == "UPDATE") {
        // UPDATE table SET col = ? WHERE col = ?
        std::string table_name, set_kw, set_col, eq1, q1, where_kw, where_col, eq2;
        iss >> table_name >> set_kw >> set_col >> eq1 >> q1 >> where_kw >> where_col >> eq2;

        auto table_it = tables_.find(table_name);
        if (table_it == tables_.end()) {
            return lang::ScriptValue::Error("Table not found: " + table_name);
        }

        auto& table = table_it->second;
        // Remove trailing comma from set_col if present
        if (!set_col.empty() && set_col.back() == ',') set_col.pop_back();

        lang::ScriptValue set_val, where_val;
        if (params.IsList() && params.AsList().size() >= 2) {
            set_val = params.AsList()[0];
            where_val = params.AsList()[1];
        }

        int64_t count = 0;
        for (auto& row : table.rows) {
            auto it = row.find(where_col);
            if (it != row.end() && it->second.Equals(where_val)) {
                row[set_col] = set_val;
                count++;
            }
        }

        return lang::ScriptValue(lang::ScriptMap{{"affected_rows", lang::ScriptValue(count)}});
    }

    return lang::ScriptValue::Error("Unsupported statement: " + sql);
}

}  // namespace components
