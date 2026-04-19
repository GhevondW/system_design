#include "engine/event.h"

namespace engine {

const EventHeader& GetHeader(const Event& event) {
    return std::visit([](const auto& e) -> const EventHeader& { return e.header; }, event);
}

Timestamp GetTimestamp(const Event& event) {
    return GetHeader(event).timestamp;
}

}  // namespace engine
