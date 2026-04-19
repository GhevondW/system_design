#include "bridge/bridge.h"

#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "components/cache.h"
#include "components/client.h"
#include "components/database.h"
#include "components/http_server.h"
#include "components/load_balancer.h"
#include "engine/simulation_engine.h"
#include "engine/test_runner.h"

namespace {

using json = nlohmann::json;

// Engine instance storage
std::unordered_map<int, std::unique_ptr<engine::SimulationEngine>> g_engines;
int g_next_handle = 1;

// Allocate a string that the caller must free with engine_free_string
char* AllocString(const std::string& str) {
    char* result = new char[str.size() + 1];
    std::memcpy(result, str.c_str(), str.size() + 1);
    return result;
}

char* JsonResult(const json& j) {
    return AllocString(j.dump());
}

char* ErrorResult(const std::string& message) {
    json result = {{"error", message}};
    return AllocString(result.dump());
}

engine::SimulationEngine* GetEngine(int handle) {
    auto it = g_engines.find(handle);
    if (it == g_engines.end()) return nullptr;
    return it->second.get();
}

// Build graph from JSON
void BuildGraphFromJson(engine::SimulationEngine& eng, const json& graph_json) {
    engine::Graph graph;

    if (graph_json.contains("components")) {
        for (const auto& comp : graph_json["components"]) {
            std::string id = comp["id"];
            std::string type = comp["type"];

            if (type == "Client") {
                graph.AddComponent(std::make_unique<components::Client>(id));
            } else if (type == "HttpServer") {
                auto server = std::make_unique<components::HttpServer>(id);
                if (comp.contains("routes")) {
                    for (const auto& r : comp["routes"]) {
                        server->AddRoute(
                            r["method"].get<std::string>(),
                            r["path"].get<std::string>(),
                            r["handler"].get<std::string>());
                    }
                }
                graph.AddComponent(std::move(server));
            } else if (type == "Database") {
                auto db = std::make_unique<components::Database>(id);
                if (comp.contains("tables")) {
                    for (const auto& [name, cols_json] : comp["tables"].items()) {
                        std::vector<components::Column> columns;
                        for (const auto& c : cols_json) {
                            if (c.is_string()) {
                                // Simple format: ["col1", "col2"]
                                columns.push_back({c.get<std::string>(), components::ColumnType::kAny});
                            } else if (c.is_object()) {
                                // Typed format: [{"name": "col1", "type": "string"}, ...]
                                std::string col_name = c["name"].get<std::string>();
                                auto col_type = components::ColumnType::kAny;
                                if (c.contains("type")) {
                                    std::string type_str = c["type"].get<std::string>();
                                    if (type_str == "string") col_type = components::ColumnType::kString;
                                    else if (type_str == "int") col_type = components::ColumnType::kInt;
                                    else if (type_str == "float") col_type = components::ColumnType::kFloat;
                                    else if (type_str == "bool") col_type = components::ColumnType::kBool;
                                }
                                columns.push_back({col_name, col_type});
                            }
                        }
                        db->CreateTable(name, columns);
                    }
                }
                graph.AddComponent(std::move(db));
            } else if (type == "Cache") {
                graph.AddComponent(std::make_unique<components::Cache>(id));
            } else if (type == "LoadBalancer") {
                graph.AddComponent(std::make_unique<components::LoadBalancer>(id));
            }
        }
    }

    if (graph_json.contains("connections")) {
        for (const auto& conn : graph_json["connections"]) {
            graph.AddConnection(engine::Connection{
                conn["from"].get<std::string>(),
                conn["to"].get<std::string>(),
                conn["alias"].get<std::string>(),
            });
        }
    }

    eng.SetGraph(std::move(graph));
}

}  // namespace

extern "C" {

int engine_create() {
    int handle = g_next_handle++;
    g_engines[handle] = std::make_unique<engine::SimulationEngine>();
    return handle;
}

void engine_destroy(int handle) {
    g_engines.erase(handle);
}

const char* engine_load_graph(int handle, const char* graph_json) {
    auto* eng = GetEngine(handle);
    if (!eng) return ErrorResult("Invalid engine handle");

    try {
        auto j = json::parse(graph_json);
        BuildGraphFromJson(*eng, j);
        return JsonResult({{"ok", true}});
    } catch (const std::exception& e) {
        return ErrorResult(e.what());
    }
}

const char* engine_load_code(int handle, const char* component_id, const char* code) {
    auto* eng = GetEngine(handle);
    if (!eng) return ErrorResult("Invalid engine handle");

    try {
        eng->LoadCode(component_id, code);
        return JsonResult({{"ok", true}});
    } catch (const std::exception& e) {
        return ErrorResult(e.what());
    }
}

const char* engine_run_all(int handle, const char* test_cases_json) {
    auto* eng = GetEngine(handle);
    if (!eng) return ErrorResult("Invalid engine handle");

    try {
        auto tests_json = json::parse(test_cases_json);
        auto test_cases = engine::TestRunner::ParseTestCases(tests_json);

        engine::TestRunner runner(*eng);
        auto suite_result = runner.RunAll(test_cases);

        json result = json::object();
        result["total"] = suite_result.total;
        result["passed"] = suite_result.passed;
        result["failed"] = suite_result.failed;
        result["all_passed"] = suite_result.AllPassed();

        json test_results = json::array();
        for (const auto& tr : suite_result.results) {
            json test_json = json::object();
            test_json["name"] = tr.test_name;
            test_json["passed"] = tr.passed;
            if (!tr.error_message.empty()) test_json["error"] = tr.error_message;
            if (!tr.expected.empty()) test_json["expected"] = tr.expected;
            if (!tr.actual.empty()) test_json["actual"] = tr.actual;
            test_json["logs"] = tr.logs;
            test_results.push_back(test_json);
        }
        result["results"] = test_results;

        return JsonResult(result);
    } catch (const std::exception& e) {
        return ErrorResult(e.what());
    }
}

const char* engine_step_event(int handle) {
    auto* eng = GetEngine(handle);
    if (!eng) return ErrorResult("Invalid engine handle");

    try {
        auto snapshot = eng->StepEvent();
        json result = {
            {"current_time", snapshot.current_time},
            {"queue_size", snapshot.queue_size},
            {"events_processed", snapshot.events_processed},
            {"finished", snapshot.finished},
            {"logs", snapshot.logs},
        };
        return JsonResult(result);
    } catch (const std::exception& e) {
        return ErrorResult(e.what());
    }
}

void engine_reset(int handle) {
    auto* eng = GetEngine(handle);
    if (eng) eng->Reset();
}

const char* engine_get_state(int handle) {
    auto* eng = GetEngine(handle);
    if (!eng) return ErrorResult("Invalid engine handle");

    json state = {
        {"current_time", eng->GetClock().Now()},
        {"logs", eng->GetLogs()},
    };
    return JsonResult(state);
}

const char* engine_get_logs(int handle) {
    auto* eng = GetEngine(handle);
    if (!eng) return ErrorResult("Invalid engine handle");

    return JsonResult(eng->GetLogs());
}

void engine_free_string(const char* ptr) {
    delete[] ptr;
}

int engine_version() {
    return 1;
}

}  // extern "C"
