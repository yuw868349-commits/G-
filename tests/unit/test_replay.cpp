#include <catch2/catch_test_macros.hpp>
#include "core/replay.hpp"

using namespace praxis;

namespace {

class CapturingSink final : public EventSink {
public:
    void on_event(const Event& event) override {
        received.push_back(event.kind);
    }
    std::vector<EventKind> received;
};

} // namespace

TEST_CASE("replay records events with monotonic sequence") {
    Replay replay;
    replay.record(EventKind::TurnStarted);
    replay.record(EventKind::ModelResponded);
    CHECK(replay.size() == 2);
    auto events = replay.events();
    CHECK(events[0].sequence == 1);
    CHECK(events[1].sequence == 2);
}

TEST_CASE("replay notifies subscribers") {
    Replay replay;
    auto sink = std::make_shared<CapturingSink>();
    replay.subscribe(sink);
    replay.record(EventKind::ToolCalled);
    CHECK(sink->received.size() == 1);
    CHECK(sink->received[0] == EventKind::ToolCalled);
}

TEST_CASE("replay replay_into replays recorded events") {
    Replay replay;
    replay.record(EventKind::TurnStarted);
    replay.record(EventKind::TaskEnded);
    CapturingSink sink;
    replay.replay_into(sink);
    CHECK(sink.received.size() == 2);
}
