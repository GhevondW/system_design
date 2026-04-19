#include "engine/simulation_engine.h"

#include <cortex/tiny_fiber/tiny_fiber.hpp>

#include "components/cache.h"
#include "components/database.h"
#include "components/http_server.h"
#include "lang/lexer.h"
#include "lang/parser.h"

namespace engine {

namespace tf = cortex::tiny_fiber;

SimulationEngine::SimulationEngine() = default;

void SimulationEngine::SetGraph(Graph graph) {
    graph_ = std::move(graph);
    wired_ = false;
}

void SimulationEngine::EnsureWired() {
    if (!wired_) {
        if (async_mode_) {
            WireAsyncConnections();
        } else {
            WireConnections();
        }
        wired_ = true;
    }
}

void SimulationEngine::LoadCode(const ComponentId& component_id, const std::string& source) {
    EnsureWired();
    lang::Lexer lexer(source);
    auto tokens = lexer.Tokenize();
    lang::Parser parser(std::move(tokens));
    auto result = parser.Parse();
    if (!result.has_value()) {
        throw std::runtime_error("Parse error in component '" + component_id +
                                 "': " + result.error().message);
    }
    loaded_programs_.push_back(std::move(result.value()));
    interpreter_.Execute(loaded_programs_.back());

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

    while ((!event_queue_.Empty() || fiber_runtime_.HasActiveFibers()) &&
           events_processed_ < max_events) {
        if (!event_queue_.Empty()) {
            auto event = event_queue_.Pop();
            if (!event.has_value()) break;

            clock_.AdvanceTo(GetTimestamp(*event));
            ProcessEvent(*event);
            events_processed_++;

            // After processing, run fibers and collect any new events they produce
            DrainFiberEvents();
        } else if (fiber_runtime_.HasActiveFibers()) {
            // Fibers are waiting but no events — deadlock or bug
            break;
        }
    }

    return SimulationResult{true, interpreter_.GetLogs(), clock_.Now(), events_processed_};
}

StateSnapshot SimulationEngine::StepEvent() {
    EnsureWired();

    bool finished = event_queue_.Empty() && !fiber_runtime_.HasActiveFibers();
    if (finished) {
        return StateSnapshot{clock_.Now(), 0, events_processed_, true, interpreter_.GetLogs()};
    }

    if (!event_queue_.Empty()) {
        auto event = event_queue_.Pop();
        if (event.has_value()) {
            clock_.AdvanceTo(GetTimestamp(*event));
            ProcessEvent(*event);
            events_processed_++;
            DrainFiberEvents();
        }
    }

    finished = event_queue_.Empty() && !fiber_runtime_.HasActiveFibers();
    return StateSnapshot{clock_.Now(), event_queue_.Size(), events_processed_, finished,
                         interpreter_.GetLogs()};
}

void SimulationEngine::Reset() {
    event_queue_.Clear();
    clock_.Reset();
    interpreter_.ClearLogs();
    fiber_runtime_.Reset();
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

FiberRuntime& SimulationEngine::GetFiberRuntime() {
    return fiber_runtime_;
}

void SimulationEngine::SetAsyncMode(bool enabled) {
    async_mode_ = enabled;
    wired_ = false;  // re-wire with new mode
}

bool SimulationEngine::IsAsyncMode() const {
    return async_mode_;
}

// --- Private: Sync wiring (direct calls, no fibers) ---

void SimulationEngine::WireConnections() {
    for (const auto& conn : graph_.GetConnections()) {
        auto* target = graph_.GetComponent(conn.to);
        if (!target) continue;

        if (target->GetType() == ComponentType::kDatabase) {
            auto* db = dynamic_cast<components::Database*>(target);
            if (!db) continue;

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

// --- Private: Async wiring (fibers yield on cross-component calls) ---
//
// In async mode, each cross-component call (db.query, cache.get, etc.)
// yields the fiber via tf::Yield(). This allows other concurrent request
// handlers to interleave — exposing race conditions and concurrency bugs.
// The actual DB/cache operation still executes immediately (in-memory),
// but the yield simulates the latency of a real network round-trip.

void SimulationEngine::WireAsyncConnections() {
    for (const auto& conn : graph_.GetConnections()) {
        auto* target = graph_.GetComponent(conn.to);
        if (!target) continue;

        if (target->GetType() == ComponentType::kDatabase) {
            auto* db = dynamic_cast<components::Database*>(target);
            if (!db) continue;

            interpreter_.InjectVariable(conn.alias,
                lang::ScriptValue(lang::ScriptMap{
                    {"query", lang::ScriptValue(lang::NativeFunction{
                        [db](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.empty()) return lang::ScriptValue::Error("query requires SQL");
                            auto params = args.size() > 1 ? args[1] : lang::ScriptValue::Null();
                            // Yield to simulate DB latency — other fibers run here
                            tf::Yield();
                            return db->Query(args[0].AsString(), params);
                        }})},
                    {"execute", lang::ScriptValue(lang::NativeFunction{
                        [db](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.empty()) return lang::ScriptValue::Error("execute requires SQL");
                            auto params = args.size() > 1 ? args[1] : lang::ScriptValue::Null();
                            tf::Yield();
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
                            tf::Yield();
                            return cache->Get(args[0].AsString());
                        }})},
                    {"set", lang::ScriptValue(lang::NativeFunction{
                        [cache](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.size() < 2) return lang::ScriptValue::Error("set requires key and value");
                            int64_t ttl = args.size() > 2 ? args[2].AsInt() : 0;
                            tf::Yield();
                            cache->Set(args[0].AsString(), args[1], ttl);
                            return lang::ScriptValue::Null();
                        }})},
                    {"del", lang::ScriptValue(lang::NativeFunction{
                        [cache](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.empty()) return lang::ScriptValue::Error("del requires key");
                            tf::Yield();
                            return lang::ScriptValue(cache->Del(args[0].AsString()));
                        }})},
                    {"has", lang::ScriptValue(lang::NativeFunction{
                        [cache](std::vector<lang::ScriptValue> args) -> lang::ScriptValue {
                            if (args.empty()) return lang::ScriptValue::Error("has requires key");
                            tf::Yield();
                            return lang::ScriptValue(cache->Has(args[0].AsString()));
                        }})},
                }));
        }
    }
}

// --- Private ---

void SimulationEngine::ProcessEvent(const Event& event) {
    const auto& header = GetHeader(event);
    auto* target = graph_.GetComponent(header.to);
    if (!target) return;

    if (async_mode_) {
        // In async mode, spawn a fiber for request handling
        if (auto* req = std::get_if<NetworkRequest>(&event)) {
            auto* server = dynamic_cast<components::HttpServer*>(target);
            if (server) {
                auto req_copy = *req;
                auto current_time = clock_.Now();
                fiber_runtime_.SpawnHandler([this, server, req_copy, current_time]() {
                    auto events = server->HandleEvent(req_copy, current_time);
                    for (auto& e : events) {
                        fiber_runtime_.EmitEvent(std::move(e));
                    }
                });
                return;
            }
        }
    }

    // Default: synchronous event handling
    auto new_events = target->HandleEvent(event, clock_.Now());
    for (auto& e : new_events) {
        event_queue_.Push(std::move(e));
    }
}

void SimulationEngine::DrainFiberEvents() {
    if (!async_mode_) return;

    // Run all spawned fibers to completion
    fiber_runtime_.RunAll();

    // Collect events generated by fibers and enqueue them
    auto events = fiber_runtime_.TakeGeneratedEvents();
    for (auto& e : events) {
        auto& header = const_cast<EventHeader&>(GetHeader(e));
        if (header.timestamp == 0) {
            header.timestamp = clock_.Now() + latency_config_.network_latency;
        }
        event_queue_.Push(std::move(e));
    }
}

}  // namespace engine
