#include "engine/test_runner.h"

#include "components/cache.h"
#include "components/client.h"
#include "components/database.h"

namespace engine {

TestRunner::TestRunner(SimulationEngine& engine) : engine_(engine) {}

TestResult TestRunner::RunTest(const TestCase& test) {
    TestResult result;
    result.test_name = test.name;

    engine_.Reset();

    // Execute each step
    for (size_t i = 0; i < test.steps.size(); ++i) {
        const auto& step = test.steps[i];

        auto* client =
            dynamic_cast<components::Client*>(engine_.GetGraph().GetComponent(step.client_id));
        if (!client) {
            result.error_message = "Client '" + step.client_id + "' not found";
            return result;
        }

        engine_.EnqueueEvent(client->CreateRequest(step.target_id, step.method, step.path,
                                                    step.body, step.timestamp));
        engine_.RunAll();

        // Check expectations
        if (i < test.expectations.size()) {
            const auto& expect = test.expectations[i];
            const auto& responses = client->GetResponses();

            if (responses.empty()) {
                result.error_message = "No response received for step " + std::to_string(i);
                return result;
            }

            const auto& last_response = responses.back();

            if (last_response.status != expect.expected_status) {
                result.error_message =
                    "Step " + std::to_string(i) + ": expected status " +
                    std::to_string(expect.expected_status) + " but got " +
                    std::to_string(last_response.status);
                result.expected = std::to_string(expect.expected_status);
                result.actual = std::to_string(last_response.status);
                return result;
            }

            // Check body fields
            for (const auto& [field, expected_val] : expect.body_checks) {
                if (last_response.body.IsMap()) {
                    auto& body_map = last_response.body.AsMap();
                    auto it = body_map.find(field);
                    if (it == body_map.end()) {
                        result.error_message =
                            "Step " + std::to_string(i) + ": body missing field '" + field + "'";
                        return result;
                    }
                    if (!it->second.Equals(expected_val)) {
                        result.error_message =
                            "Step " + std::to_string(i) + ": body." + field + " expected " +
                            expected_val.ToString() + " but got " + it->second.ToString();
                        result.expected = expected_val.ToString();
                        result.actual = it->second.ToString();
                        return result;
                    }
                }
            }
        }
    }

    // Post-execution: DB count checks
    for (const auto& [table_name, expected_count] : test.db_count_checks) {
        // Find the first database component
        for (const auto& [id, comp] : engine_.GetGraph().GetComponents()) {
            if (comp->GetType() == ComponentType::kDatabase) {
                auto* db = dynamic_cast<components::Database*>(comp.get());
                auto rows = db->Query("SELECT * FROM " + table_name, lang::ScriptValue::Null());
                if (rows.IsList()) {
                    auto actual_count = static_cast<int>(rows.AsList().size());
                    if (actual_count != expected_count) {
                        result.error_message = "DB check: " + table_name + " expected " +
                                               std::to_string(expected_count) + " rows but got " +
                                               std::to_string(actual_count);
                        result.expected = std::to_string(expected_count);
                        result.actual = std::to_string(actual_count);
                        return result;
                    }
                }
                break;
            }
        }
    }

    // Post-execution: Cache checks
    for (const auto& [key, expected_has] : test.cache_checks) {
        for (const auto& [id, comp] : engine_.GetGraph().GetComponents()) {
            if (comp->GetType() == ComponentType::kCache) {
                auto* cache = dynamic_cast<components::Cache*>(comp.get());
                if (cache->Has(key) != expected_has) {
                    result.error_message = "Cache check: key '" + key + "' expected " +
                                           std::string(expected_has ? "present" : "absent") +
                                           " but was " +
                                           std::string(cache->Has(key) ? "present" : "absent");
                    return result;
                }
                break;
            }
        }
    }

    result.passed = true;
    result.logs = engine_.GetLogs();
    return result;
}

TestSuiteResult TestRunner::RunAll(const std::vector<TestCase>& tests) {
    TestSuiteResult suite;
    suite.total = static_cast<int>(tests.size());

    for (const auto& test : tests) {
        auto result = RunTest(test);
        if (result.passed) {
            suite.passed++;
        } else {
            suite.failed++;
        }
        suite.results.push_back(std::move(result));
    }

    return suite;
}

std::vector<TestCase> TestRunner::ParseTestCases(const nlohmann::json& json) {
    std::vector<TestCase> cases;

    for (const auto& tc : json) {
        TestCase test;
        test.name = tc.value("name", "unnamed");

        if (tc.contains("steps")) {
            for (const auto& step_json : tc["steps"]) {
                TestStep step;
                step.client_id = step_json.value("client", "client");
                step.target_id = step_json.value("target", "server");
                step.method = step_json.value("method", "GET");
                step.path = step_json.value("path", "/");
                step.timestamp = step_json.value("timestamp", 0);

                if (step_json.contains("body")) {
                    // Convert JSON body to ScriptValue
                    const auto& body = step_json["body"];
                    if (body.is_object()) {
                        lang::ScriptMap map;
                        for (auto& [k, v] : body.items()) {
                            if (v.is_string()) map[k] = lang::ScriptValue(v.get<std::string>());
                            else if (v.is_number_integer()) map[k] = lang::ScriptValue(v.get<int64_t>());
                            else if (v.is_boolean()) map[k] = lang::ScriptValue(v.get<bool>());
                        }
                        step.body = lang::ScriptValue(std::move(map));
                    }
                }

                test.steps.push_back(std::move(step));
            }
        }

        if (tc.contains("expectations")) {
            for (const auto& exp : tc["expectations"]) {
                TestExpectation expectation;
                expectation.expected_status = exp.value("status", 200);

                if (exp.contains("body")) {
                    for (auto& [k, v] : exp["body"].items()) {
                        if (v.is_string()) {
                            expectation.body_checks.emplace_back(k, lang::ScriptValue(v.get<std::string>()));
                        } else if (v.is_number_integer()) {
                            expectation.body_checks.emplace_back(k, lang::ScriptValue(v.get<int64_t>()));
                        } else if (v.is_boolean()) {
                            expectation.body_checks.emplace_back(k, lang::ScriptValue(v.get<bool>()));
                        }
                    }
                }

                test.expectations.push_back(std::move(expectation));
            }
        }

        if (tc.contains("db_checks")) {
            for (auto& [table, count] : tc["db_checks"].items()) {
                test.db_count_checks.emplace_back(table, count.get<int>());
            }
        }

        if (tc.contains("cache_checks")) {
            for (auto& [key, has] : tc["cache_checks"].items()) {
                test.cache_checks.emplace_back(key, has.get<bool>());
            }
        }

        cases.push_back(std::move(test));
    }

    return cases;
}

}  // namespace engine
