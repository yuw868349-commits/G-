#include <catch2/catch_test_macros.hpp>
#include "core/orchestrator.hpp"
#include "llm/fake_provider.hpp"
#include "tools/builtin.hpp"

using namespace swiftagent;

TEST_CASE("orchestrator runs a multi-turn task with fake provider") {
    FakeProvider provider;
    provider.script({
        {{"plan", "step 1"}, {"tool_calls", nlohmann::json::array({
            nlohmann::json{{"name", "read_file"}, {"arguments", nlohmann::json{{"path", "/tmp/none"}}}},
        })}},
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });
    Orchestrator orch(provider, 1024);
    orch.register_builtin();
    Budget budget;
    budget.max_turns = 4;
    budget.active = true;
    auto result = orch.run("test", budget);
    REQUIRE(result.ok());
    CHECK(result.value().completed);
    CHECK(result.value().turns == 2);
}

TEST_CASE("orchestrator emits events into replay") {
    FakeProvider provider;
    provider.script({
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });
    Orchestrator orch(provider, 1024);
    orch.register_builtin();
    Budget budget;
    budget.max_turns = 1;
    budget.active = true;
    auto result = orch.run("test", budget);
    REQUIRE(result.ok());
    CHECK(orch.replay().size() > 0);
}
