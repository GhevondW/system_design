#include <gtest/gtest.h>

#include "components/cache.h"
#include "components/client.h"
#include "components/database.h"
#include "components/http_server.h"
#include "engine/graph.h"

namespace {

// --- Database ---

TEST(Database, CreateTableAndInsert) {
    components::Database db("db1");
    db.CreateTable("cars", {"plate", "owner"});

    auto result = db.Execute(
        "INSERT INTO cars (plate, owner) VALUES (?, ?)",
        lang::ScriptValue::List({lang::ScriptValue("ABC123"), lang::ScriptValue("Alice")}));

    EXPECT_TRUE(result.IsMap());
    EXPECT_EQ(result.AsMap().at("affected_rows").AsInt(), 1);
}

TEST(Database, QueryAfterInsert) {
    components::Database db("db1");
    db.CreateTable("cars", {"plate", "owner"});

    db.Execute("INSERT INTO cars (plate, owner) VALUES (?, ?)",
               lang::ScriptValue::List({lang::ScriptValue("ABC123"), lang::ScriptValue("Alice")}));

    auto result = db.Query("SELECT * FROM cars WHERE plate = ?",
                           lang::ScriptValue::List({lang::ScriptValue("ABC123")}));

    ASSERT_TRUE(result.IsList());
    ASSERT_EQ(result.AsList().size(), 1);
    EXPECT_EQ(result.AsList()[0].AsMap().at("owner").AsString(), "Alice");
}

TEST(Database, QueryNoMatch) {
    components::Database db("db1");
    db.CreateTable("cars", {"plate"});

    auto result = db.Query("SELECT * FROM cars WHERE plate = ?",
                           lang::ScriptValue::List({lang::ScriptValue("NONE")}));
    ASSERT_TRUE(result.IsList());
    EXPECT_EQ(result.AsList().size(), 0);
}

TEST(Database, QueryAllRows) {
    components::Database db("db1");
    db.CreateTable("cars", {"plate"});
    db.Execute("INSERT INTO cars (plate) VALUES (?)",
               lang::ScriptValue::List({lang::ScriptValue("A")}));
    db.Execute("INSERT INTO cars (plate) VALUES (?)",
               lang::ScriptValue::List({lang::ScriptValue("B")}));

    auto result = db.Query("SELECT * FROM cars", lang::ScriptValue::Null());
    ASSERT_TRUE(result.IsList());
    EXPECT_EQ(result.AsList().size(), 2);
}

TEST(Database, Delete) {
    components::Database db("db1");
    db.CreateTable("cars", {"plate"});
    db.Execute("INSERT INTO cars (plate) VALUES (?)",
               lang::ScriptValue::List({lang::ScriptValue("A")}));
    db.Execute("INSERT INTO cars (plate) VALUES (?)",
               lang::ScriptValue::List({lang::ScriptValue("B")}));

    db.Execute("DELETE FROM cars WHERE plate = ?",
               lang::ScriptValue::List({lang::ScriptValue("A")}));

    auto result = db.Query("SELECT * FROM cars", lang::ScriptValue::Null());
    EXPECT_EQ(result.AsList().size(), 1);
}

TEST(Database, Reset) {
    components::Database db("db1");
    db.CreateTable("cars", {"plate"});
    db.Execute("INSERT INTO cars (plate) VALUES (?)",
               lang::ScriptValue::List({lang::ScriptValue("A")}));
    db.Reset();

    auto result = db.Query("SELECT * FROM cars", lang::ScriptValue::Null());
    EXPECT_TRUE(result.IsError());  // table doesn't exist after reset
}

// --- Cache ---

TEST(Cache, SetAndGet) {
    components::Cache cache("cache1");
    cache.Set("key1", lang::ScriptValue("value1"));

    auto val = cache.Get("key1");
    EXPECT_EQ(val.AsString(), "value1");
}

TEST(Cache, GetMiss) {
    components::Cache cache("cache1");
    auto val = cache.Get("nonexistent");
    EXPECT_TRUE(val.IsNull());
}

TEST(Cache, Has) {
    components::Cache cache("cache1");
    cache.Set("key1", lang::ScriptValue(42));
    EXPECT_TRUE(cache.Has("key1"));
    EXPECT_FALSE(cache.Has("key2"));
}

TEST(Cache, Del) {
    components::Cache cache("cache1");
    cache.Set("key1", lang::ScriptValue("val"));
    EXPECT_TRUE(cache.Del("key1"));
    EXPECT_FALSE(cache.Has("key1"));
    EXPECT_FALSE(cache.Del("nonexistent"));
}

TEST(Cache, Reset) {
    components::Cache cache("cache1");
    cache.Set("key1", lang::ScriptValue("val"));
    cache.Reset();
    EXPECT_FALSE(cache.Has("key1"));
}

// --- Client ---

TEST(Client, CreateRequest) {
    components::Client client("client1");
    auto event =
        client.CreateRequest("server1", "POST", "/register", lang::ScriptValue("body"), 0);

    auto* req = std::get_if<engine::NetworkRequest>(&event);
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->header.from, "client1");
    EXPECT_EQ(req->header.to, "server1");
    EXPECT_EQ(req->method, "POST");
    EXPECT_EQ(req->path, "/register");
}

TEST(Client, ReceivesResponses) {
    components::Client client("client1");
    engine::Event response = engine::NetworkResponse{
        engine::EventHeader{"server1", "client1", 5, 1001}, 200, lang::ScriptValue("ok"), 1};

    client.HandleEvent(response, 5);
    ASSERT_EQ(client.GetResponses().size(), 1);
    EXPECT_EQ(client.GetResponses()[0].status, 200);
}

// --- HttpServer ---

TEST(HttpServer, HandleRequest_MatchingRoute) {
    components::HttpServer server("server1");
    server.AddRoute("GET", "/hello", [](lang::ScriptValue /*req*/) {
        return lang::ScriptValue(lang::ScriptMap{
            {"status", lang::ScriptValue(200)},
            {"body", lang::ScriptValue("world")},
        });
    });

    auto result = server.HandleRequest("GET", "/hello", lang::ScriptValue::Null());
    EXPECT_TRUE(result.IsMap());
    EXPECT_EQ(result.AsMap().at("status").AsInt(), 200);
}

TEST(HttpServer, HandleRequest_NoMatchingRoute) {
    components::HttpServer server("server1");
    auto result = server.HandleRequest("GET", "/missing", lang::ScriptValue::Null());
    EXPECT_TRUE(result.IsMap());
    EXPECT_EQ(result.AsMap().at("status").AsInt(), 404);
}

TEST(HttpServer, HandleEvent_NetworkRequest) {
    components::HttpServer server("server1");
    server.AddRoute("GET", "/test", [](lang::ScriptValue /*req*/) {
        return lang::ScriptValue(lang::ScriptMap{
            {"status", lang::ScriptValue(200)},
            {"body", lang::ScriptValue("ok")},
        });
    });

    engine::Event request = engine::NetworkRequest{
        engine::EventHeader{"client1", "server1", 0, 1}, "GET", "/test",
        lang::ScriptValue::Null()};

    auto responses = server.HandleEvent(request, 0);
    ASSERT_EQ(responses.size(), 1);

    auto* resp = std::get_if<engine::NetworkResponse>(&responses[0]);
    ASSERT_NE(resp, nullptr);
    EXPECT_EQ(resp->status, 200);
    EXPECT_EQ(resp->header.to, "client1");
}

// --- Graph ---

TEST(Graph, AddAndGetComponent) {
    engine::Graph graph;
    graph.AddComponent(std::make_unique<components::Database>("db1"));

    auto* comp = graph.GetComponent("db1");
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->GetType(), engine::ComponentType::kDatabase);
}

TEST(Graph, GetComponent_NotFound) {
    engine::Graph graph;
    EXPECT_EQ(graph.GetComponent("nonexistent"), nullptr);
}

TEST(Graph, Connections) {
    engine::Graph graph;
    graph.AddComponent(std::make_unique<components::HttpServer>("server1"));
    graph.AddComponent(std::make_unique<components::Database>("db1"));
    graph.AddConnection(engine::Connection{"server1", "db1", "db"});

    auto conns = graph.GetConnectionsFrom("server1");
    ASSERT_EQ(conns.size(), 1);
    EXPECT_EQ(conns[0]->to, "db1");
    EXPECT_EQ(conns[0]->alias, "db");
}

}  // namespace
