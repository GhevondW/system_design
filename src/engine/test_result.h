#pragma once

#include <string>
#include <vector>

#include "lang/value.h"

namespace engine {

struct TestResult {
    std::string test_name;
    bool passed = false;
    std::string expected;
    std::string actual;
    std::string error_message;
    std::vector<std::string> logs;
};

struct TestSuiteResult {
    int total = 0;
    int passed = 0;
    int failed = 0;
    std::vector<TestResult> results;

    [[nodiscard]] bool AllPassed() const { return failed == 0; }
};

}  // namespace engine
