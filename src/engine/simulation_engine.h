#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/clock.h"
#include "engine/event_queue.h"
#include "engine/graph.h"
#include "lang/interpreter.h"

namespace engine {

struct SimulationResult {
    bool success = false;
    std::vector<std::string> logs;
    Timestamp final_time = 0;
    int events_processed = 0;
};

struct StateSnapshot {
    Timestamp current_time = 0;
    size_t queue_size = 0;
    int events_processed = 0;
    bool finished = false;
    std::vector<std::string> logs;
};

class SimulationEngine {
public:
    SimulationEngine();

    // Setup
    void SetGraph(Graph graph);
    void LoadCode(const ComponentId& component_id, const std::string& source);

    // Execution
    SimulationResult RunAll(int max_events = 10000);
    StateSnapshot StepEvent();
    void Reset();

    // Event injection
    void EnqueueEvent(Event event);

    // State
    [[nodiscard]] const SimulationClock& GetClock() const;
    [[nodiscard]] const std::vector<std::string>& GetLogs() const;
    [[nodiscard]] lang::Interpreter& GetInterpreter();
    [[nodiscard]] Graph& GetGraph();

private:
    void EnsureWired();
    void WireConnections();
    void ProcessEvent(const Event& event);
    bool wired_ = false;
    std::vector<lang::StmtList> loaded_programs_;  // keep ASTs alive for closures

    Graph graph_;
    EventQueue event_queue_;
    SimulationClock clock_;
    lang::Interpreter interpreter_;
    LatencyConfig latency_config_;
    int events_processed_ = 0;
};

}  // namespace engine
