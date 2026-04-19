#include "engine/fiber_runtime.h"

#include <queue>

namespace engine {

namespace tf = cortex::tiny_fiber;

struct FiberRuntime::Impl {
    std::vector<Event> generated_events;
    std::queue<std::function<void()>> pending_spawns;
    uint64_t next_request_id = 1;
};

FiberRuntime::FiberRuntime() : impl_(std::make_unique<Impl>()) {}

FiberRuntime::~FiberRuntime() = default;

FiberRuntime::FiberRuntime(FiberRuntime&&) noexcept = default;
FiberRuntime& FiberRuntime::operator=(FiberRuntime&&) noexcept = default;

void FiberRuntime::SpawnHandler(std::function<void()> handler) {
    impl_->pending_spawns.push(std::move(handler));
}

void FiberRuntime::RunAll() {
    if (impl_->pending_spawns.empty()) return;

    // Collect all pending handlers
    std::vector<std::function<void()>> handlers;
    while (!impl_->pending_spawns.empty()) {
        handlers.push_back(std::move(impl_->pending_spawns.front()));
        impl_->pending_spawns.pop();
    }

    // Run them all as fibers in a Cortex scheduler.
    // Each handler can call tf::Yield() to simulate latency.
    // All fibers run cooperatively until completion.
    auto* self = this;
    tf::Scheduler::Run([&handlers, self]() {
        std::vector<tf::Future<void>> futures;
        futures.reserve(handlers.size());
        for (auto& h : handlers) {
            futures.push_back(tf::Spawn(std::move(h)));
        }
        for (auto& f : futures) {
            f.Wait();
        }
    });
}

bool FiberRuntime::Step() {
    // For step-through mode: run one handler at a time
    if (impl_->pending_spawns.empty()) return false;

    auto handler = std::move(impl_->pending_spawns.front());
    impl_->pending_spawns.pop();

    tf::Scheduler::Run(std::move(handler));

    return !impl_->pending_spawns.empty();
}

bool FiberRuntime::HasActiveFibers() const {
    return !impl_->pending_spawns.empty();
}

std::vector<Event> FiberRuntime::TakeGeneratedEvents() {
    auto events = std::move(impl_->generated_events);
    impl_->generated_events.clear();
    return events;
}

void FiberRuntime::EmitEvent(Event event) {
    impl_->generated_events.push_back(std::move(event));
}

uint64_t FiberRuntime::NextRequestId() {
    return impl_->next_request_id++;
}

void FiberRuntime::Reset() {
    if (!impl_) return;
    impl_->generated_events.clear();
    while (!impl_->pending_spawns.empty()) impl_->pending_spawns.pop();
    impl_->next_request_id = 1;
}

}  // namespace engine
