#include "engine/simulation_engine.h"

#include <gtest/gtest.h>

#include "components/cache.h"
#include "components/client.h"
#include "components/database.h"
#include "components/http_server.h"

namespace {

engine::SimulationEngine BuildCarRegistrationEngine() {
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

    return sim;
}

TEST(SimulationEngine, BasicRequestResponse) {
    auto sim = BuildCarRegistrationEngine();

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

    auto* server = dynamic_cast<components::HttpServer*>(sim.GetGraph().GetComponent("server"));
    auto& interp = sim.GetInterpreter();
    server->AddRoute("POST", "/register", [&](lang::ScriptValue req) {
        return interp.GetGlobalEnv()->Get("handle_register").value().AsNativeFunction()(std::vector<lang::ScriptValue>{req});
    });

    auto* client = dynamic_cast<components::Client*>(sim.GetGraph().GetComponent("client"));
    auto body = lang::ScriptValue(lang::ScriptMap{
        {"plate", lang::ScriptValue("ABC123")},
        {"owner", lang::ScriptValue("Alice")},
    });
    sim.EnqueueEvent(client->CreateRequest("server", "POST", "/register", std::move(body), 0));

    auto result = sim.RunAll();
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.events_processed, 0);

    ASSERT_EQ(client->GetResponses().size(), 1);
    EXPECT_EQ(client->GetResponses()[0].status, 201);

    auto* db = dynamic_cast<components::Database*>(sim.GetGraph().GetComponent("db"));
    auto rows = db->Find("cars", {{"plate", lang::ScriptValue("ABC123")}});
    ASSERT_TRUE(rows.IsList());
    EXPECT_EQ(rows.AsList().size(), 1);
}

TEST(SimulationEngine, DuplicateRegistration) {
    auto sim = BuildCarRegistrationEngine();

    sim.LoadCode("server", R"SYSLANG(
        fn handle_register(req) {
            let existing = db.FindOne("cars", { plate: req.body.plate });
            if (existing != null) {
                return { status: 409, body: "Already registered" };
            }
            db.Insert("cars", { plate: req.body.plate, owner: req.body.owner });
            return { status: 201, body: { plate: req.body.plate } };
        }
    )SYSLANG");

    auto* server = dynamic_cast<components::HttpServer*>(sim.GetGraph().GetComponent("server"));
    auto& interp = sim.GetInterpreter();
    server->AddRoute("POST", "/register", [&](lang::ScriptValue req) {
        return interp.GetGlobalEnv()->Get("handle_register").value().AsNativeFunction()(std::vector<lang::ScriptValue>{req});
    });

    auto* client = dynamic_cast<components::Client*>(sim.GetGraph().GetComponent("client"));

    auto body1 = lang::ScriptValue(lang::ScriptMap{
        {"plate", lang::ScriptValue("ABC123")},
        {"owner", lang::ScriptValue("Alice")},
    });
    sim.EnqueueEvent(client->CreateRequest("server", "POST", "/register", std::move(body1), 0));
    sim.RunAll();
    EXPECT_EQ(client->GetResponses()[0].status, 201);

    auto body2 = lang::ScriptValue(lang::ScriptMap{
        {"plate", lang::ScriptValue("ABC123")},
        {"owner", lang::ScriptValue("Bob")},
    });
    sim.EnqueueEvent(client->CreateRequest("server", "POST", "/register", std::move(body2), 10));
    sim.RunAll();
    ASSERT_EQ(client->GetResponses().size(), 2);
    EXPECT_EQ(client->GetResponses()[1].status, 409);
}

TEST(SimulationEngine, StepEvent) {
    auto sim = BuildCarRegistrationEngine();

    sim.LoadCode("server", R"SYSLANG(
        fn handle_get(req) {
            return { status: 200, body: "ok" };
        }
    )SYSLANG");

    auto* server = dynamic_cast<components::HttpServer*>(sim.GetGraph().GetComponent("server"));
    auto& interp = sim.GetInterpreter();
    server->AddRoute("GET", "/test", [&](lang::ScriptValue req) {
        return interp.GetGlobalEnv()->Get("handle_get").value().AsNativeFunction()(std::vector<lang::ScriptValue>{req});
    });

    auto* client = dynamic_cast<components::Client*>(sim.GetGraph().GetComponent("client"));
    sim.EnqueueEvent(
        client->CreateRequest("server", "GET", "/test", lang::ScriptValue::Null(), 0));

    auto snap1 = sim.StepEvent();
    EXPECT_EQ(snap1.events_processed, 1);
    EXPECT_FALSE(snap1.finished);

    auto snap2 = sim.StepEvent();
    EXPECT_EQ(snap2.events_processed, 2);
    EXPECT_TRUE(snap2.finished);

    EXPECT_EQ(client->GetResponses().size(), 1);
}

TEST(SimulationEngine, Reset) {
    auto sim = BuildCarRegistrationEngine();

    sim.LoadCode("server", R"SYSLANG(
        fn handle_get(req) { return { status: 200, body: "ok" }; }
    )SYSLANG");

    auto* server = dynamic_cast<components::HttpServer*>(sim.GetGraph().GetComponent("server"));
    auto& interp = sim.GetInterpreter();
    server->AddRoute("GET", "/test", [&](lang::ScriptValue req) {
        return interp.GetGlobalEnv()->Get("handle_get").value().AsNativeFunction()(std::vector<lang::ScriptValue>{req});
    });

    auto* client = dynamic_cast<components::Client*>(sim.GetGraph().GetComponent("client"));
    sim.EnqueueEvent(
        client->CreateRequest("server", "GET", "/test", lang::ScriptValue::Null(), 0));
    sim.RunAll();

    EXPECT_EQ(client->GetResponses().size(), 1);

    sim.Reset();
    EXPECT_EQ(client->GetResponses().size(), 0);
    EXPECT_EQ(sim.GetClock().Now(), 0);
}

TEST(SimulationEngine, CacheIntegration) {
    auto sim = BuildCarRegistrationEngine();

    sim.LoadCode("server", R"SYSLANG(
        fn handle_get(req) {
            let cached = cache.get("mykey");
            if (cached != null) {
                return { status: 200, body: cached };
            }
            cache.set("mykey", "myvalue", 3600);
            return { status: 200, body: "miss" };
        }
    )SYSLANG");

    auto* server = dynamic_cast<components::HttpServer*>(sim.GetGraph().GetComponent("server"));
    auto& interp = sim.GetInterpreter();
    server->AddRoute("GET", "/test", [&](lang::ScriptValue req) {
        return interp.GetGlobalEnv()->Get("handle_get").value().AsNativeFunction()(std::vector<lang::ScriptValue>{req});
    });

    auto* client = dynamic_cast<components::Client*>(sim.GetGraph().GetComponent("client"));

    sim.EnqueueEvent(
        client->CreateRequest("server", "GET", "/test", lang::ScriptValue::Null(), 0));
    sim.RunAll();
    EXPECT_EQ(client->GetResponses().size(), 1);

    sim.EnqueueEvent(
        client->CreateRequest("server", "GET", "/test", lang::ScriptValue::Null(), 10));
    sim.RunAll();
    ASSERT_EQ(client->GetResponses().size(), 2);
}

}  // namespace
