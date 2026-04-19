#include "bridge/bridge.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

TEST(Bridge, CreateAndDestroy) {
    int handle = engine_create();
    EXPECT_GT(handle, 0);
    engine_destroy(handle);
}

TEST(Bridge, Version) {
    EXPECT_EQ(engine_version(), 1);
}

TEST(Bridge, LoadGraph) {
    int handle = engine_create();

    json graph = {
        {"components",
         {{{"id", "client"}, {"type", "Client"}},
          {{"id", "server"}, {"type", "HttpServer"}},
          {{"id", "db"}, {"type", "Database"}, {"tables", {{"cars", {"plate", "owner"}}}}},
          {{"id", "cache"}, {"type", "Cache"}}}},
        {"connections",
         {{{"from", "server"}, {"to", "db"}, {"alias", "db"}},
          {{"from", "server"}, {"to", "cache"}, {"alias", "cache"}}}},
    };

    const char* result = engine_load_graph(handle, graph.dump().c_str());
    auto result_json = json::parse(result);
    engine_free_string(result);

    EXPECT_TRUE(result_json.contains("ok"));
    EXPECT_TRUE(result_json["ok"].get<bool>());

    engine_destroy(handle);
}

TEST(Bridge, LoadCode) {
    int handle = engine_create();

    json graph = {
        {"components", {{{"id", "server"}, {"type", "HttpServer"}}}},
        {"connections", json::array()},
    };
    const char* r1 = engine_load_graph(handle, graph.dump().c_str());
    engine_free_string(r1);

    const char* r2 = engine_load_code(handle, "server",
                                       "fn handle(req) { return { status: 200 }; }");
    auto result = json::parse(r2);
    engine_free_string(r2);

    EXPECT_TRUE(result["ok"].get<bool>());

    engine_destroy(handle);
}

TEST(Bridge, GetLogs) {
    int handle = engine_create();

    json graph = {
        {"components", {{{"id", "server"}, {"type", "HttpServer"}}}},
        {"connections", json::array()},
    };
    const char* r1 = engine_load_graph(handle, graph.dump().c_str());
    engine_free_string(r1);

    const char* r2 = engine_load_code(handle, "server", "log(\"hello from bridge\");");
    engine_free_string(r2);

    const char* logs_str = engine_get_logs(handle);
    auto logs = json::parse(logs_str);
    engine_free_string(logs_str);

    ASSERT_TRUE(logs.is_array());
    ASSERT_EQ(logs.size(), 1);
    EXPECT_EQ(logs[0].get<std::string>(), "hello from bridge");

    engine_destroy(handle);
}

TEST(Bridge, GetState) {
    int handle = engine_create();

    const char* state_str = engine_get_state(handle);
    auto state = json::parse(state_str);
    engine_free_string(state_str);

    EXPECT_EQ(state["current_time"].get<int>(), 0);

    engine_destroy(handle);
}

TEST(Bridge, InvalidHandle) {
    const char* result = engine_load_graph(999, "{}");
    auto result_json = json::parse(result);
    engine_free_string(result);

    EXPECT_TRUE(result_json.contains("error"));
}

TEST(Bridge, FreeString) {
    // Just verify it doesn't crash
    const char* state = engine_get_state(engine_create());
    engine_free_string(state);
}

}  // namespace
