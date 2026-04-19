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
    db.CreateTable("cars", std::vector<std::string>{"plate", "owner"});

    auto result = db.Insert("cars", {{"plate", lang::ScriptValue("ABC123")},
                                      {"owner", lang::ScriptValue("Alice")}});

    EXPECT_TRUE(result.IsMap());
    EXPECT_TRUE(result.AsMap().at("inserted").IsTruthy());
}

TEST(Database, FindAfterInsert) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<std::string>{"plate", "owner"});

    db.Insert("cars", {{"plate", lang::ScriptValue("ABC123")},
                        {"owner", lang::ScriptValue("Alice")}});

    auto result = db.Find("cars", {{"plate", lang::ScriptValue("ABC123")}});

    ASSERT_TRUE(result.IsList());
    ASSERT_EQ(result.AsList().size(), 1);
    EXPECT_EQ(result.AsList()[0].AsMap().at("owner").AsString(), "Alice");
}

TEST(Database, FindOneAfterInsert) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<std::string>{"plate", "owner"});

    db.Insert("cars", {{"plate", lang::ScriptValue("ABC123")},
                        {"owner", lang::ScriptValue("Alice")}});

    auto result = db.FindOne("cars", {{"plate", lang::ScriptValue("ABC123")}});
    ASSERT_TRUE(result.IsMap());
    EXPECT_EQ(result.AsMap().at("owner").AsString(), "Alice");
}

TEST(Database, FindOneNoMatch) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<std::string>{"plate"});

    auto result = db.FindOne("cars", {{"plate", lang::ScriptValue("NONE")}});
    EXPECT_TRUE(result.IsNull());
}

TEST(Database, FindNoMatch) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<std::string>{"plate"});

    auto result = db.Find("cars", {{"plate", lang::ScriptValue("NONE")}});
    ASSERT_TRUE(result.IsList());
    EXPECT_EQ(result.AsList().size(), 0);
}

TEST(Database, All) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<std::string>{"plate"});
    db.Insert("cars", {{"plate", lang::ScriptValue("A")}});
    db.Insert("cars", {{"plate", lang::ScriptValue("B")}});

    auto result = db.All("cars");
    ASSERT_TRUE(result.IsList());
    EXPECT_EQ(result.AsList().size(), 2);
}

TEST(Database, Delete) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<std::string>{"plate"});
    db.Insert("cars", {{"plate", lang::ScriptValue("A")}});
    db.Insert("cars", {{"plate", lang::ScriptValue("B")}});

    auto result = db.Delete("cars", {{"plate", lang::ScriptValue("A")}});
    EXPECT_EQ(result.AsMap().at("deleted").AsInt(), 1);

    auto remaining = db.All("cars");
    EXPECT_EQ(remaining.AsList().size(), 1);
}

TEST(Database, Update) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<std::string>{"plate", "owner"});
    db.Insert("cars", {{"plate", lang::ScriptValue("A")}, {"owner", lang::ScriptValue("Alice")}});

    auto result = db.Update("cars",
                             {{"plate", lang::ScriptValue("A")}},
                             {{"owner", lang::ScriptValue("Bob")}});
    EXPECT_EQ(result.AsMap().at("updated").AsInt(), 1);

    auto row = db.FindOne("cars", {{"plate", lang::ScriptValue("A")}});
    EXPECT_EQ(row.AsMap().at("owner").AsString(), "Bob");
}

TEST(Database, Count) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<std::string>{"plate", "owner"});
    db.Insert("cars", {{"plate", lang::ScriptValue("A")}, {"owner", lang::ScriptValue("Alice")}});
    db.Insert("cars", {{"plate", lang::ScriptValue("B")}, {"owner", lang::ScriptValue("Alice")}});
    db.Insert("cars", {{"plate", lang::ScriptValue("C")}, {"owner", lang::ScriptValue("Bob")}});

    auto result = db.Count("cars", {{"owner", lang::ScriptValue("Alice")}});
    EXPECT_EQ(result.AsInt(), 2);
}

TEST(Database, Reset) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<std::string>{"plate"});
    db.Insert("cars", {{"plate", lang::ScriptValue("A")}});
    db.Reset();

    auto result = db.All("cars");
    ASSERT_TRUE(result.IsList());
    EXPECT_EQ(result.AsList().size(), 0);  // rows cleared, schema preserved
}

// --- Schema Validation ---

TEST(Database, InsertUnknownColumn_ReturnsError) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<std::string>{"plate", "owner"});

    auto result = db.Insert("cars", {{"plate", lang::ScriptValue("A")},
                                      {"color", lang::ScriptValue("red")}});
    EXPECT_TRUE(result.IsError());
}

TEST(Database, FindUnknownColumn_ReturnsError) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<std::string>{"plate", "owner"});

    auto result = db.Find("cars", {{"color", lang::ScriptValue("red")}});
    EXPECT_TRUE(result.IsError());
}

TEST(Database, InsertNonexistentTable_ReturnsError) {
    components::Database db("db1");
    auto result = db.Insert("users", {{"name", lang::ScriptValue("Alice")}});
    EXPECT_TRUE(result.IsError());
}

TEST(Database, TypedSchema_ValidInsert) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<components::Column>{
        {"plate", components::ColumnType::kString},
        {"year", components::ColumnType::kInt},
    });

    auto result = db.Insert("cars", {{"plate", lang::ScriptValue("ABC")},
                                      {"year", lang::ScriptValue(int64_t{2020})}});
    EXPECT_FALSE(result.IsError());
}

TEST(Database, TypedSchema_WrongType_ReturnsError) {
    components::Database db("db1");
    db.CreateTable("cars", std::vector<components::Column>{
        {"plate", components::ColumnType::kString},
        {"year", components::ColumnType::kInt},
    });

    // year should be int, not string
    auto result = db.Insert("cars", {{"plate", lang::ScriptValue("ABC")},
                                      {"year", lang::ScriptValue("twenty")}});
    EXPECT_TRUE(result.IsError());
}

TEST(Database, TypedSchema_FloatAcceptsInt) {
    components::Database db("db1");
    db.CreateTable("metrics", std::vector<components::Column>{
        {"value", components::ColumnType::kFloat},
    });

    // int should promote to float
    auto result = db.Insert("metrics", {{"value", lang::ScriptValue(int64_t{42})}});
    EXPECT_FALSE(result.IsError());
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
