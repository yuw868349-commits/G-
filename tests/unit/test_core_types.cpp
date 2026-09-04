#include <catch2/catch_test_macros.hpp>
#include "core/error.hpp"
#include "core/event.hpp"
#include "core/types.hpp"

using namespace swiftagent;

TEST_CASE("error has category") {
    auto e = Error{ErrorKind::Timeout, "tool timed out"};
    CHECK(e.kind == ErrorKind::Timeout);
    CHECK(e.message == "tool timed out");
}

TEST_CASE("turn outcome stores tool progress") {
    TurnOutcome out{};
    out.plan = "reorganize files";
    out.has_tool_use = true;
    out.tool_count = 3;
    REQUIRE(out.is_valid_turn());
}

TEST_CASE("progress score is clamped") {
    auto s = ProgressScore{};
    CHECK(s.score >= 0.0);
    CHECK(s.score <= 1.0);
}

TEST_CASE("events carry sequence numbers and kind") {
    Event ev{EventKind::ToolCalled, 7};
    CHECK(ev.kind == EventKind::ToolCalled);
    CHECK(ev.sequence == 7);
    ev.payload["tool"] = "read_file";
    CHECK(ev.payload["tool"] == "read_file");
}
