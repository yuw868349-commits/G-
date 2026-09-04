#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "core/orchestrator.hpp"
#include "llm/fake_provider.hpp"
#include "tools/builtin.hpp"

using namespace swiftagent;

TEST_CASE("end-to-end run drives a real task to completion") {
    FakeProvider provider;
    provider.script({
        {{"plan", "read the data"}, {"tool_calls", nlohmann::json::array({
            nlohmann::json{{"name", "read_file"}, {"arguments", nlohmann::json{{"path", "/dev/null"}}}}
        })}},
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });
    Orchestrator orch(provider, 1024);
    orch.register_builtin();
    Budget budget;
    budget.max_turns = 4;
    budget.active = true;
    auto result = orch.run("summarize this folder", budget);
    REQUIRE(result.ok());
    auto& r = result.value();
    CHECK(r.completed);
    CHECK(r.turns == 2);
    CHECK(r.final_output == "DONE");
    CHECK(orch.replay().size() > 0);
    auto events = orch.replay().events();
    bool saw_task_ended = false;
    for (const auto& e : events) {
        if (e.kind == EventKind::TaskEnded) {
            saw_task_ended = true;
        }
    }
    CHECK(saw_task_ended);
}

TEST_CASE("end-to-end run respects budget when model never yields") {
    FakeProvider provider;
    provider.script({
        {{"plan", "loop"}, {"tool_calls", nlohmann::json::array({
            nlohmann::json{{"name", "read_file"}, {"arguments", nlohmann::json{{"path", "/x"}}}}
        })}},
        {{"plan", "loop"}, {"tool_calls", nlohmann::json::array({
            nlohmann::json{{"name", "read_file"}, {"arguments", nlohmann::json{{"path", "/x"}}}}
        })}},
        {{"plan", "loop"}, {"tool_calls", nlohmann::json::array({
            nlohmann::json{{"name", "read_file"}, {"arguments", nlohmann::json{{"path", "/x"}}}}
        })}}
    });
    Orchestrator orch(provider, 1024);
    orch.register_builtin();
    Budget budget;
    budget.max_turns = 2;
    budget.active = true;
    auto result = orch.run("any task", budget);
    REQUIRE(result.ok());
    auto& r = result.value();
    CHECK(r.bounded);
    CHECK(r.turns == 2);
}
