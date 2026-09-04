#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "core/context_manager.hpp"

using namespace swiftagent;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("working set keeps token budget bounded") {
    ContextManager cm(256);
    FactStore& store = cm.store();
    std::string big(2000, 'x');
    cm.record_tool_result(store.append("tool_result", big));
    auto ws = cm.render_working_set(TurnContext{});
    CHECK(cm.size_tokens(ws.render) <= 256);
}

TEST_CASE("precise recall returns verbatim store content") {
    ContextManager cm(1024);
    FactStore& store = cm.store();
    auto id = store.append("tool_result", R"({"port":8080})");
    auto ws = cm.render_working_set(TurnContext{});
    CHECK(ws.fact_blocks.empty());
    auto recalled = cm.recall(id);
    REQUIRE(recalled.has_value());
    CHECK(recalled->content == R"({"port":8080})");
    CHECK(recalled->mode == RecallMode::Precise);
}

TEST_CASE("missing recall degrades to full digest render, never paraphrase") {
    ContextManager cm(1024);
    auto result = cm.recall("does-not-exist");
    REQUIRE(result.has_value());
    CHECK(result->mode == RecallMode::Degraded);
    CHECK(result->content.empty());
}

TEST_CASE("monotonic digest keeps hard facts as hard keys") {
    ContextManager cm(1024);
    FactStore& store = cm.store();
    cm.record_tool_result(store.append("tool_result", R"({"port":8080})"));
    cm.record_tool_result(store.append("tool_result", R"({"path":"/tmp/a"})"));
    cm.rebuild_digest();
    auto d = cm.current_digest();
    CHECK(d.contains("port"));
    CHECK(d.at("port") == "8080");
}
