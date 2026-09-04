#include <catch2/catch_test_macros.hpp>
#include "core/error.hpp"
#include "core/event.hpp"
#include "core/types.hpp"
#include "llm/provider.hpp"

#include <stdexcept>

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

TEST_CASE("Result value()/error() throw instead of UB on the wrong variant") {
    auto ok = Result<int>::ok(42);
    CHECK(ok.ok());
    CHECK(static_cast<bool>(ok));
    CHECK(ok.value() == 42);
    CHECK(ok.value_or(0) == 42);
    auto failed = Result<int>::fail(Error{ErrorKind::Internal, "boom"});
    CHECK(!failed.ok());
    CHECK(failed.value_or(7) == 7);
    CHECK(failed.error().kind == ErrorKind::Internal);
    CHECK_THROWS_AS(failed.value(), std::runtime_error);
    CHECK_THROWS_AS(ok.error(), std::runtime_error);
}
