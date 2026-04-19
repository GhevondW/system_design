#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "lang/value.h"

namespace engine {

using ComponentId = std::string;
using Timestamp = int64_t;  // virtual milliseconds

struct EventHeader {
    ComponentId from;
    ComponentId to;
    Timestamp timestamp = 0;
    uint64_t id = 0;  // unique event ID for tracking
};

struct NetworkRequest {
    EventHeader header;
    std::string method;
    std::string path;
    lang::ScriptValue body;
};

struct NetworkResponse {
    EventHeader header;
    int status = 200;
    lang::ScriptValue body;
    uint64_t request_id = 0;  // links to original request
};

struct QueueMessage {
    EventHeader header;
    std::string topic;
    lang::ScriptValue payload;
};

struct QueueAck {
    EventHeader header;
    uint64_t message_id = 0;
};

struct CacheOp {
    EventHeader header;
    enum class Operation : uint8_t { kGet, kSet, kDel, kHas };
    Operation operation;
    std::string key;
    lang::ScriptValue value;
    int64_t ttl = 0;  // seconds, 0 = no expiry
};

struct CacheResult {
    EventHeader header;
    bool hit = false;
    lang::ScriptValue value;
};

struct TimerFired {
    EventHeader header;
    std::string callback_name;
};

struct Failure {
    EventHeader header;
    enum class Type : uint8_t { kNetworkDrop, kCrash, kTimeout };
    Type type;
};

using Event = std::variant<NetworkRequest, NetworkResponse, QueueMessage, QueueAck, CacheOp,
                           CacheResult, TimerFired, Failure>;

// Get the header from any event variant
[[nodiscard]] const EventHeader& GetHeader(const Event& event);
[[nodiscard]] Timestamp GetTimestamp(const Event& event);

}  // namespace engine
