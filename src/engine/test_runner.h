#pragma once

#include <string>
#include <vector>

#include "engine/simulation_engine.h"
#include "engine/test_result.h"

#include <nlohmann/json.hpp>

namespace engine {

// A single test step: send a request and check the response
struct TestStep {
    std::string client_id;
    std::string target_id;
    std::string method;
    std::string path;
    lang::ScriptValue body;
    Timestamp timestamp = 0;
};

struct TestExpectation {
    int expected_status = 200;
    std::vector<std::pair<std::string, lang::ScriptValue>> body_checks;  // field -> expected value
};

struct TestCase {
    std::string name;
    std::vector<TestStep> steps;
    std::vector<TestExpectation> expectations;  // one per step
    // Post-execution assertions
    std::vector<std::pair<std::string, int>> db_count_checks;  // table -> expected row count
    std::vector<std::pair<std::string, bool>> cache_checks;    // key -> expected has()
};

class TestRunner {
public:
    explicit TestRunner(SimulationEngine& engine);

    // Run a single test case
    [[nodiscard]] TestResult RunTest(const TestCase& test);

    // Run all test cases
    [[nodiscard]] TestSuiteResult RunAll(const std::vector<TestCase>& tests);

    // Parse test cases from JSON
    [[nodiscard]] static std::vector<TestCase> ParseTestCases(const nlohmann::json& json);

private:
    SimulationEngine& engine_;
};

}  // namespace engine
