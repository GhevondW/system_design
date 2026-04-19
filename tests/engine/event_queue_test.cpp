#include "engine/event_queue.h"

#include <gtest/gtest.h>

#include "engine/clock.h"

namespace engine {
namespace {

Event MakeRequest(Timestamp ts, const std::string& from, const std::string& to) {
    return NetworkRequest{EventHeader{from, to, ts, 0}, "GET", "/", lang::ScriptValue::Null()};
}

// --- EventQueue ---

TEST(EventQueue, Pop_EmptyQueue_ReturnsNullopt) {
    EventQueue queue;
    EXPECT_EQ(queue.Pop(), std::nullopt);
}

TEST(EventQueue, Push_SingleEvent_CanPop) {
    EventQueue queue;
    queue.Push(MakeRequest(0, "client", "server"));
    EXPECT_FALSE(queue.Empty());
    EXPECT_EQ(queue.Size(), 1);

    auto event = queue.Pop();
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(GetTimestamp(*event), 0);
    EXPECT_TRUE(queue.Empty());
}

TEST(EventQueue, Pop_MultipleEvents_ReturnsInTimestampOrder) {
    EventQueue queue;
    queue.Push(MakeRequest(5, "c", "s"));
    queue.Push(MakeRequest(1, "c", "s"));
    queue.Push(MakeRequest(3, "c", "s"));

    EXPECT_EQ(GetTimestamp(*queue.Pop()), 1);
    EXPECT_EQ(GetTimestamp(*queue.Pop()), 3);
    EXPECT_EQ(GetTimestamp(*queue.Pop()), 5);
    EXPECT_TRUE(queue.Empty());
}

TEST(EventQueue, PeekTimestamp_ReturnsLowest) {
    EventQueue queue;
    queue.Push(MakeRequest(10, "c", "s"));
    queue.Push(MakeRequest(2, "c", "s"));
    queue.Push(MakeRequest(7, "c", "s"));

    auto ts = queue.PeekTimestamp();
    ASSERT_TRUE(ts.has_value());
    EXPECT_EQ(*ts, 2);
}

TEST(EventQueue, PeekTimestamp_EmptyQueue) {
    EventQueue queue;
    EXPECT_FALSE(queue.PeekTimestamp().has_value());
}

TEST(EventQueue, Clear_EmptiesQueue) {
    EventQueue queue;
    queue.Push(MakeRequest(1, "c", "s"));
    queue.Push(MakeRequest(2, "c", "s"));
    queue.Clear();
    EXPECT_TRUE(queue.Empty());
    EXPECT_EQ(queue.Size(), 0);
}

TEST(EventQueue, Size_TracksCorrectly) {
    EventQueue queue;
    EXPECT_EQ(queue.Size(), 0);
    queue.Push(MakeRequest(1, "c", "s"));
    EXPECT_EQ(queue.Size(), 1);
    queue.Push(MakeRequest(2, "c", "s"));
    EXPECT_EQ(queue.Size(), 2);
    [[maybe_unused]] auto _ = queue.Pop();
    EXPECT_EQ(queue.Size(), 1);
}

// --- GetHeader / GetTimestamp ---

TEST(Event, GetHeader_NetworkRequest) {
    Event event = NetworkRequest{EventHeader{"client", "server", 42, 1}, "GET", "/test",
                                 lang::ScriptValue::Null()};
    const auto& header = GetHeader(event);
    EXPECT_EQ(header.from, "client");
    EXPECT_EQ(header.to, "server");
    EXPECT_EQ(header.timestamp, 42);
}

TEST(Event, GetHeader_NetworkResponse) {
    Event event = NetworkResponse{EventHeader{"server", "client", 10, 2}, 200,
                                  lang::ScriptValue("ok"), 1};
    EXPECT_EQ(GetTimestamp(event), 10);
}

TEST(Event, GetHeader_Failure) {
    Event event = Failure{EventHeader{"", "server", 99, 3}, Failure::Type::kCrash};
    EXPECT_EQ(GetTimestamp(event), 99);
    EXPECT_EQ(GetHeader(event).to, "server");
}

// --- SimulationClock ---

TEST(SimulationClock, InitialTimeIsZero) {
    SimulationClock clock;
    EXPECT_EQ(clock.Now(), 0);
}

TEST(SimulationClock, AdvanceTo_ForwardTime) {
    SimulationClock clock;
    clock.AdvanceTo(10);
    EXPECT_EQ(clock.Now(), 10);
    clock.AdvanceTo(20);
    EXPECT_EQ(clock.Now(), 20);
}

TEST(SimulationClock, AdvanceTo_DoesNotGoBackward) {
    SimulationClock clock;
    clock.AdvanceTo(10);
    clock.AdvanceTo(5);
    EXPECT_EQ(clock.Now(), 10);
}

TEST(SimulationClock, Reset_GoesBackToZero) {
    SimulationClock clock;
    clock.AdvanceTo(100);
    clock.Reset();
    EXPECT_EQ(clock.Now(), 0);
}

// --- Integration: Queue + Clock ---

TEST(EventQueueAndClock, ClockAdvancesWithEvents) {
    EventQueue queue;
    SimulationClock clock;

    queue.Push(MakeRequest(5, "c", "s"));
    queue.Push(MakeRequest(10, "c", "s"));
    queue.Push(MakeRequest(15, "c", "s"));

    auto e1 = queue.Pop();
    clock.AdvanceTo(GetTimestamp(*e1));
    EXPECT_EQ(clock.Now(), 5);

    auto e2 = queue.Pop();
    clock.AdvanceTo(GetTimestamp(*e2));
    EXPECT_EQ(clock.Now(), 10);

    auto e3 = queue.Pop();
    clock.AdvanceTo(GetTimestamp(*e3));
    EXPECT_EQ(clock.Now(), 15);
}

}  // namespace
}  // namespace engine
