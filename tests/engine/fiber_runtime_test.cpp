#include "engine/simulation_engine.h"

#include <gtest/gtest.h>

#include "components/cache.h"
#include "components/client.h"
#include "components/database.h"
#include "components/http_server.h"

namespace {

engine::SimulationEngine BuildAsyncEngine() {
    engine::SimulationEngine sim;
    sim.SetAsyncMode(true);

    engine::Graph graph;
    graph.AddComponent(std::make_unique<components::Client>("client"));
    graph.AddComponent(std::make_unique<components::HttpServer>("server"));
    graph.AddComponent(std::make_unique<components::Database>("db"));
    graph.AddComponent(std::make_unique<components::Cache>("cache"));

    graph.AddConnection(engine::Connection{"server", "db", "db"});
    graph.AddConnection(engine::Connection{"server", "cache", "cache"});

    sim.SetGraph(std::move(graph));

    auto* db = dynamic_cast<components::Database*>(sim.GetGraph().GetComponent("db"));
    db->CreateTable("cars", {"plate", "owner"});

    return sim;
}

TEST(FiberRuntime, AsyncMode_BasicRequestResponse) {
    auto sim = BuildAsyncEngine();

    sim.LoadCode("server", R"SYSLANG(
        fn handle_register(req) {
            let existing = db.query("SELECT * FROM cars WHERE plate = ?", [req.body.plate]);
            if (existing.size() > 0) {
                return { status: 409, body: "Already registered" };
            }
            db.execute("INSERT INTO cars (plate, owner) VALUES (?, ?)", [req.body.plate, req.body.owner]);
            return { status: 201, body: { plate: req.body.plate, registered: true } };
        }
    )SYSLANG");

    auto* server = dynamic_cast<components::HttpServer*>(sim.GetGraph().GetComponent("server"));
    auto& interp = sim.GetInterpreter();
    server->AddRoute("POST", "/register", [&](lang::ScriptValue req) {
        return interp.GetGlobalEnv()
            ->Get("handle_register")
            .value()
            .AsNativeFunction()(std::vector<lang::ScriptValue>{req});
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

    ASSERT_GE(client->GetResponses().size(), 1);
    EXPECT_EQ(client->GetResponses()[0].status, 201);

    auto* db = dynamic_cast<components::Database*>(sim.GetGraph().GetComponent("db"));
    auto rows = db->Query("SELECT * FROM cars WHERE plate = ?",
                          lang::ScriptValue::List({lang::ScriptValue("ABC123")}));
    ASSERT_TRUE(rows.IsList());
    EXPECT_EQ(rows.AsList().size(), 1);
}

TEST(FiberRuntime, AsyncMode_CacheIntegration) {
    auto sim = BuildAsyncEngine();

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
        return interp.GetGlobalEnv()
            ->Get("handle_get")
            .value()
            .AsNativeFunction()(std::vector<lang::ScriptValue>{req});
    });

    auto* client = dynamic_cast<components::Client*>(sim.GetGraph().GetComponent("client"));

    // First request — cache miss
    sim.EnqueueEvent(
        client->CreateRequest("server", "GET", "/test", lang::ScriptValue::Null(), 0));
    sim.RunAll();
    EXPECT_GE(client->GetResponses().size(), 1);

    // Second request — cache hit
    sim.EnqueueEvent(
        client->CreateRequest("server", "GET", "/test", lang::ScriptValue::Null(), 10));
    sim.RunAll();
    EXPECT_GE(client->GetResponses().size(), 2);
}

TEST(FiberRuntime, AsyncMode_SyncModeStillWorks) {
    // Verify that sync mode (default) still works the same
    engine::SimulationEngine sim;
    EXPECT_FALSE(sim.IsAsyncMode());

    engine::Graph graph;
    graph.AddComponent(std::make_unique<components::Client>("client"));
    graph.AddComponent(std::make_unique<components::HttpServer>("server"));
    graph.AddComponent(std::make_unique<components::Database>("db"));

    graph.AddConnection(engine::Connection{"server", "db", "db"});
    sim.SetGraph(std::move(graph));

    auto* db = dynamic_cast<components::Database*>(sim.GetGraph().GetComponent("db"));
    db->CreateTable("items", {"name"});

    sim.LoadCode("server", R"SYSLANG(
        fn handle(req) {
            db.execute("INSERT INTO items (name) VALUES (?)", [req.body.name]);
            return { status: 200, body: "ok" };
        }
    )SYSLANG");

    auto* server = dynamic_cast<components::HttpServer*>(sim.GetGraph().GetComponent("server"));
    auto& interp = sim.GetInterpreter();
    server->AddRoute("POST", "/add", [&](lang::ScriptValue req) {
        return interp.GetGlobalEnv()->Get("handle").value().AsNativeFunction()(
            std::vector<lang::ScriptValue>{req});
    });

    auto* client = dynamic_cast<components::Client*>(sim.GetGraph().GetComponent("client"));
    auto body = lang::ScriptValue(lang::ScriptMap{{"name", lang::ScriptValue("test")}});
    sim.EnqueueEvent(client->CreateRequest("server", "POST", "/add", std::move(body), 0));

    auto result = sim.RunAll();
    EXPECT_TRUE(result.success);
    EXPECT_EQ(client->GetResponses()[0].status, 200);
}

}  // namespace
