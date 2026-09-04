#include <catch2/catch_test_macros.hpp>
#include "core/dependency_graph.hpp"

using namespace swiftagent;

TEST_CASE("dependency graph detects no conflict for independent calls") {
    DependencyGraph graph;
    ToolCall a{"read_file", R"({"path":"a.txt"})", 0};
    ToolCall b{"read_file", R"({"path":"b.txt"})", 1};
    graph.add_call(a, {"file:read:a.txt"});
    graph.add_call(b, {"file:read:b.txt"});
    CHECK_FALSE(graph.has_conflict(0, 1));
    auto groups = graph.schedule();
    CHECK(groups.size() == 1);
}

TEST_CASE("dependency graph serializes conflicting calls") {
    DependencyGraph graph;
    ToolCall a{"write_file", R"({"path":"x"})", 0};
    ToolCall b{"write_file", R"({"path":"x"})", 1};
    graph.add_call(a, {"file:write:x"});
    graph.add_call(b, {"file:write:x"});
    CHECK(graph.has_conflict(0, 1));
    auto groups = graph.schedule();
    CHECK(groups.size() == 2);
}

TEST_CASE("dependency graph groups weakly connected components") {
    DependencyGraph graph;
    ToolCall a{"write_file", R"({"path":"a"})", 0};
    ToolCall b{"write_file", R"({"path":"a"})", 1};
    ToolCall c{"read_file", R"({"path":"b"})", 2};
    graph.add_call(a, {"file:write:a"});
    graph.add_call(b, {"file:write:a"});
    graph.add_call(c, {"file:read:b"});
    auto comps = graph.weakly_connected_components();
    CHECK(comps.size() == 2);
}
