#include "engine/event_queue.h"

namespace engine {

void EventQueue::Push(Event event) {
    queue_.push(std::move(event));
}

std::optional<Event> EventQueue::Pop() {
    if (queue_.empty()) return std::nullopt;
    auto event = std::move(const_cast<Event&>(queue_.top()));
    queue_.pop();
    return event;
}

bool EventQueue::Empty() const {
    return queue_.empty();
}

size_t EventQueue::Size() const {
    return queue_.size();
}

std::optional<Timestamp> EventQueue::PeekTimestamp() const {
    if (queue_.empty()) return std::nullopt;
    return GetTimestamp(queue_.top());
}

void EventQueue::Clear() {
    while (!queue_.empty()) queue_.pop();
}

}  // namespace engine
