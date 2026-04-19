#include "engine/test_runner.h"

#include <gtest/gtest.h>

#include "components/cache.h"
#include "components/client.h"
#include "components/database.h"
#include "components/http_server.h"

namespace {

engine::SimulationEngine SetupEngine() {
    engine::SimulationEngine sim;

    engine::Graph graph;
    graph.AddComponent(std::make_unique<components::Client>("client"));
    graph.AddComponent(std::make_unique<components::HttpServer>("server"));
    graph.AddComponent(std::make_unique<components::Database>("db"));
    graph.AddComponent(std::make_unique<components::Cache>("cache"));

    graph.AddConnection(engine::Connection{"server", "db", "db"});
    graph.AddConnection(engine::Connection{"server", "cache", "cache"});

    sim.SetGraph(std::move(graph));

    auto* db = dynamic_cast<components::Database*>(sim.GetGraph().GetComponent("db"));
    db->CreateTable("cars", std::vector<std::string>{"plate", "owner"});

    sim.LoadCode("server", R"SYSLANG(
        fn handle_register(req) {
            let existing = db.FindOne("cars", { plate: req.body.plate });
            if (existing != null) {
                return { status: 409, body: "Already registered" };
            }
            db.Insert("cars", { plate: req.body.plate, owner: req.body.owner });
            return { status: 201, body: { plate: req.body.plate, registered: true } };
        }
    )SYSLANG");

    auto* server =
        dynamic_cast<components::HttpServer*>(sim.GetGraph().GetComponent("server"));
    auto& interp = sim.GetInterpreter();
    server->AddRoute("POST", "/register", [&](lang::ScriptValue req) {
        return interp.GetGlobalEnv()
            ->Get("handle_register")
            .value()
            .AsNativeFunction()(std::vector<lang::ScriptValue>{req});
    });

    return sim;
}

TEST(TestRunner, PassingTest) {
    auto sim = SetupEngine();
    engine::TestRunner runner(sim);

    engine::TestCase test;
    test.name = "Register a new car";
    test.steps.push_back(engine::TestStep{
        "client", "server", "POST", "/register",
        lang::ScriptValue(lang::ScriptMap{
            {"plate", lang::ScriptValue("ABC123")},
            {"owner", lang::ScriptValue("Alice")},
        }),
        0});
    test.expectations.push_back(engine::TestExpectation{
        201,
        {{"registered", lang::ScriptValue(true)}},
    });
    test.db_count_checks.push_back({"cars", 1});

    auto result = runner.RunTest(test);
    EXPECT_TRUE(result.passed) << result.error_message;
}

TEST(TestRunner, FailingTest_WrongStatus) {
    auto sim = SetupEngine();
    engine::TestRunner runner(sim);

    engine::TestCase test;
    test.name = "Expect wrong status";
    test.steps.push_back(engine::TestStep{
        "client", "server", "POST", "/register",
        lang::ScriptValue(lang::ScriptMap{
            {"plate", lang::ScriptValue("ABC123")},
            {"owner", lang::ScriptValue("Alice")},
        }),
        0});
    test.expectations.push_back(engine::TestExpectation{200, {}});

    auto result = runner.RunTest(test);
    EXPECT_FALSE(result.passed);
    EXPECT_FALSE(result.error_message.empty());
}

TEST(TestRunner, RunAll) {
    auto sim = SetupEngine();
    engine::TestRunner runner(sim);

    std::vector<engine::TestCase> tests;

    engine::TestCase test1;
    test1.name = "Register car";
    test1.steps.push_back(engine::TestStep{
        "client", "server", "POST", "/register",
        lang::ScriptValue(lang::ScriptMap{
            {"plate", lang::ScriptValue("ABC123")},
            {"owner", lang::ScriptValue("Alice")},
        }),
        0});
    test1.expectations.push_back(engine::TestExpectation{201, {}});
    tests.push_back(std::move(test1));

    engine::TestCase test2;
    test2.name = "Register same car";
    test2.steps.push_back(engine::TestStep{
        "client", "server", "POST", "/register",
        lang::ScriptValue(lang::ScriptMap{
            {"plate", lang::ScriptValue("XYZ789")},
            {"owner", lang::ScriptValue("Bob")},
        }),
        0});
    test2.expectations.push_back(engine::TestExpectation{201, {}});
    tests.push_back(std::move(test2));

    auto suite = runner.RunAll(tests);
    EXPECT_EQ(suite.total, 2);
    EXPECT_EQ(suite.passed, 2);
    EXPECT_TRUE(suite.AllPassed());
}

TEST(TestRunner, ParseTestCases) {
    nlohmann::json json = nlohmann::json::parse(R"JSON([
        {
            "name": "Register a car",
            "steps": [
                {
                    "client": "client",
                    "target": "server",
                    "method": "POST",
                    "path": "/register",
                    "body": {"plate": "ABC123", "owner": "Alice"}
                }
            ],
            "expectations": [
                {"status": 201, "body": {"registered": true}}
            ],
            "db_checks": {"cars": 1}
        }
    ])JSON");

    auto cases = engine::TestRunner::ParseTestCases(json);
    ASSERT_EQ(cases.size(), 1);
    EXPECT_EQ(cases[0].name, "Register a car");
    EXPECT_EQ(cases[0].steps.size(), 1);
    EXPECT_EQ(cases[0].steps[0].method, "POST");
    EXPECT_EQ(cases[0].expectations.size(), 1);
    EXPECT_EQ(cases[0].expectations[0].expected_status, 201);
    EXPECT_EQ(cases[0].db_count_checks.size(), 1);
}

}  // namespace
