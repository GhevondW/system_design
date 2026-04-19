#include "engine/simulation_engine.h"

#include "components/cache.h"
#include "components/database.h"
#include "components/http_server.h"
#include "lang/lexer.h"
#include "lang/parser.h"

namespace engine {

SimulationEngine::SimulationEngine() = default;

void SimulationEngine::SetGraph(Graph graph) {
    graph_ = std::move(graph);
    wired_ = false;
}

void SimulationEngine::EnsureWired() {
    if (!wired_) {
        WireConnections();
        wired_ = true;
    }
}

void SimulationEngine::LoadCode(const ComponentId& component_id, const std::string& source) {
    EnsureWired();
    // Parse and execute the code in the interpreter — this defines functions
    lang::Lexer lexer(source);
    auto tokens = lexer.Tokenize();
    lang::Parser parser(std::move(tokens));
    auto result = parser.Parse();
    if (!result.has_value()) {
        throw std::runtime_error("Parse error in component '" + component_id +
                                 "': " + result.error().message);
    }
    // Store the AST to keep it alive — closures capture raw pointers into it
    loaded_programs_.push_back(std::move(result.value()));
    interpreter_.Execute(loaded_programs_.back());

    // If it's an HTTP server, wire up route handlers from the parsed functions
    auto* comp = graph_.GetComponent(component_id);
    if (comp && comp->GetType() == ComponentType::kHttpServer) {
        auto* server = dynamic_cast<components::HttpServer*>(comp);
        if (server) {
            server->SetInterpreter(&interpreter_);
        }
    }
}

SimulationResult SimulationEngine::RunAll(int max_events) {
    EnsureWired();
    events_processed_ = 0;

    while (!event_queue_.Empty() && events_processed_ < max_events) {
        auto event = event_queue_.Pop();
        if (!event.has_value()) break;

        clock_.AdvanceTo(GetTimestamp(*event));
        ProcessEvent(*event);
        events_processed_++;
    }

    return SimulationResult{
        true,
        interpreter_.GetLogs(),
        clock_.Now(),
        events_processed_,
    };
}

StateSnapshot SimulationEngine::StepEvent() {
    EnsureWired();
    if (event_queue_.Empty()) {
        return StateSnapshot{clock_.Now(), 0, events_processed_, true, interpreter_.GetLogs()};
    }

    auto event = event_queue_.Pop();
    if (!event.has_value()) {
        return StateSnapshot{clock_.Now(), 0, events_processed_, true, interpreter_.GetLogs()};
    }

    clock_.AdvanceTo(GetTimestamp(*event));
    ProcessEvent(*event);
    events_processed_++;

    return StateSnapshot{
        clock_.Now(),
        event_queue_.Size(),
        events_processed_,
        event_queue_.Empty(),
        interpreter_.GetLogs(),
    };
}

void SimulationEngine::Reset() {
    event_queue_.Clear();
    clock_.Reset();
    interpreter_.ClearLogs();
    events_processed_ = 0;

    for (auto& [id, comp] : graph_.GetComponents()) {
        comp->Reset();
    }
}

void SimulationEngine::EnqueueEvent(Event event) {
    event_queue_.Push(std::move(event));
}

const SimulationClock& SimulationEngine::GetClock() const {
    return clock_;
}

const std::vector<std::string>& SimulationEngine::GetLogs() const {
    return interpreter_.GetLogs();
}

lang::Interpreter& SimulationEngine::GetInterpreter() {
    return interpreter_;
}

Graph& SimulationEngine::GetGraph() {
    return graph_;
}

// --- Private ---

void SimulationEngine::WireConnections() {
    for (const auto& conn : graph_.GetConnections()) {
        auto* target = graph_.GetComponent(conn.to);
        if (!target) continue;

        // Inject connected component's API into the interpreter
        if (target->GetType() == ComponentType::kDatabase) {
            auto* db = dynamic_cast<components::Database*>(target);
            if (!db) continue;

            // Inject db object with query/execute/createTable methods
            auto db_obj = lang::ScriptValue(lang::ScriptMap{});

            interpreter_.InjectVariable(conn.alias,
                lang::ScriptValue(lang::NativeFunction{[db](std::vector<lang::ScriptValue> /*args*/) {
                    // This is the db object itself — methods are called via CallMethod
                    return db->GetApiObject();
                }}));

            // Register individual methods that the interpreter can call
            // We create a map-like object with callable methods
            auto db_api = lang::ScriptValue(lang::ScriptMap{});

            // The interpreter's CallMethod will handle db.query(), db.execute(), etc.
            // We need to inject the db as a special value that the interpreter recognizes
            // For now, inject native functions directly
            interpreter_.InjectVariable(conn.alias,
                lang::ScriptValue(lang::ScriptMap{
                    {"_type", lang::ScriptValue("database")},
                    {"_id", lang::ScriptValue(conn.to)},
                }));

            // Register db methods as callable
            std::string alias = conn.alias;
            interpreter_.InjectVariable(alias + "_query",
                lang::ScriptValue(lang::NativeFunction{
                    [db](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                        if (args.size() < 1) return lang::ScriptValue::Error("query requires SQL");
                        auto params = args.size() > 1 ? args[1] : lang::ScriptValue::Null();
                        return db->Query(args[0].AsString(), params);
                    }}));

            interpreter_.InjectVariable(alias + "_execute",
                lang::ScriptValue(lang::NativeFunction{
                    [db](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                        if (args.size() < 1) return lang::ScriptValue::Error("execute requires SQL");
                        auto params = args.size() > 1 ? args[1] : lang::ScriptValue::Null();
                        return db->Execute(args[0].AsString(), params);
                    }}));

            interpreter_.InjectVariable(alias + "_createTable",
                lang::ScriptValue(lang::NativeFunction{
                    [db](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                        if (args.size() < 2) return lang::ScriptValue::Error("createTable requires name and columns");
                        std::vector<std::string> cols;
                        if (args[1].IsList()) {
                            for (const auto& c : args[1].AsList()) {
                                cols.push_back(c.AsString());
                            }
                        }
                        db->CreateTable(args[0].AsString(), cols);
                        return lang::ScriptValue::Null();
                    }}));

            // Override the alias itself to be an object with methods
            // We use a map with NativeFunction values for methods
            interpreter_.InjectVariable(conn.alias,
                lang::ScriptValue(lang::ScriptMap{
                    {"query", lang::ScriptValue(lang::NativeFunction{
                        [db](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.empty()) return lang::ScriptValue::Error("query requires SQL");
                            auto params = args.size() > 1 ? args[1] : lang::ScriptValue::Null();
                            return db->Query(args[0].AsString(), params);
                        }})},
                    {"execute", lang::ScriptValue(lang::NativeFunction{
                        [db](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.empty()) return lang::ScriptValue::Error("execute requires SQL");
                            auto params = args.size() > 1 ? args[1] : lang::ScriptValue::Null();
                            return db->Execute(args[0].AsString(), params);
                        }})},
                    {"createTable", lang::ScriptValue(lang::NativeFunction{
                        [db](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.size() < 2) return lang::ScriptValue::Error("createTable requires name and columns");
                            std::vector<std::string> cols;
                            if (args[1].IsList()) {
                                for (const auto& c : args[1].AsList()) cols.push_back(c.AsString());
                            }
                            db->CreateTable(args[0].AsString(), cols);
                            return lang::ScriptValue::Null();
                        }})},
                }));
        }

        if (target->GetType() == ComponentType::kCache) {
            auto* cache = dynamic_cast<components::Cache*>(target);
            if (!cache) continue;

            interpreter_.InjectVariable(conn.alias,
                lang::ScriptValue(lang::ScriptMap{
                    {"get", lang::ScriptValue(lang::NativeFunction{
                        [cache](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.empty()) return lang::ScriptValue::Error("get requires key");
                            return cache->Get(args[0].AsString());
                        }})},
                    {"set", lang::ScriptValue(lang::NativeFunction{
                        [cache](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.size() < 2) return lang::ScriptValue::Error("set requires key and value");
                            int64_t ttl = args.size() > 2 ? args[2].AsInt() : 0;
                            cache->Set(args[0].AsString(), args[1], ttl);
                            return lang::ScriptValue::Null();
                        }})},
                    {"del", lang::ScriptValue(lang::NativeFunction{
                        [cache](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.empty()) return lang::ScriptValue::Error("del requires key");
                            return lang::ScriptValue(cache->Del(args[0].AsString()));
                        }})},
                    {"has", lang::ScriptValue(lang::NativeFunction{
                        [cache](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.empty()) return lang::ScriptValue::Error("has requires key");
                            return lang::ScriptValue(cache->Has(args[0].AsString()));
                        }})},
                }));
        }
    }
}

void SimulationEngine::ProcessEvent(const Event& event) {
    const auto& header = GetHeader(event);
    auto* target = graph_.GetComponent(header.to);
    if (!target) return;

    auto new_events = target->HandleEvent(event, clock_.Now());
    for (auto& e : new_events) {
        event_queue_.Push(std::move(e));
    }
}

}  // namespace engine
