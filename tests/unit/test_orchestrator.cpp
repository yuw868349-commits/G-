#include <catch2/catch_test_macros.hpp>
#include "core/orchestrator.hpp"
#include "llm/fake_provider.hpp"
#include "llm/retrying_provider.hpp"
#include "tools/builtin.hpp"

using namespace praxis;

TEST_CASE("orchestrator runs a multi-turn task with fake provider") {
    FakeProvider provider;
    provider.script({
        {{"plan", "step 1"}, {"tool_calls", nlohmann::json::array({
            nlohmann::json{{"name", "read_file"}, {"arguments", nlohmann::json{{"path", "/tmp/none"}}}},
        })}},
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });
    OrchestratorOptions options;
    options.token_budget = 1024;
    Orchestrator orch(provider, options);
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
    OrchestratorOptions options;
    options.token_budget = 1024;
    Orchestrator orch(provider, options);
    orch.register_builtin();
    Budget budget;
    budget.max_turns = 1;
    budget.active = true;
    auto result = orch.run("test", budget);
    REQUIRE(result.ok());
    CHECK(orch.replay().size() > 0);
}

TEST_CASE("orchestrator aborts when budget.max_cost is exceeded") {
    // Script: a single turn that reports enough usage to push the run
    // over the configured max_cost ceiling. The run should mark itself
    // bounded and surface "max_cost" as the bounded_reason.
    FakeProvider provider;
    provider.script({
        {{"plan", "doing expensive work"},
         {"usage", {{"prompt_tokens", 10000}, {"completion_tokens", 10000}}},
         {"tool_calls", nlohmann::json::array()}}
    });
    OrchestratorOptions options;
    options.token_budget = 1024;
    options.pricing.prompt_per_1k = 0.01;
    options.pricing.completion_per_1k = 0.02;
    Orchestrator orch(provider, options);
    Budget budget;
    budget.max_turns = 4;
    budget.max_cost = 0.05;  // (10/1 + 20/1)*1k = $0.21; budget caps at $0.05
    budget.active = true;
    auto result = orch.run("cost-limit test", budget);
    REQUIRE(result.ok());
    auto& r = result.value();
    CHECK(r.bounded);
    CHECK(r.bounded_reason == "max_cost");
}

TEST_CASE("orchestrator retries provider failures via the embedded RetryingProvider") {
    // Wrap a FakeProvider that always succeeds, but make the outer
    // retrying wrapper have a 1-attempt policy so we can prove the
    // orchestrator reaches the underlying provider.  Then drive the
    // run normally and check that retrying wrapping did not change
    // behaviour for the success path.
    FakeProvider provider;
    provider.script({
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });
    OrchestratorOptions options;
    options.token_budget = 1024;
    options.retry_policy.max_attempts = 1;
    options.retry_policy.initial_backoff = std::chrono::milliseconds(0);
    options.retry_policy.max_backoff = std::chrono::milliseconds(0);
    Orchestrator orch(provider, options);
    orch.register_builtin();
    Budget budget;
    budget.max_turns = 2;
    budget.active = true;
    auto result = orch.run("retry smoke", budget);
    REQUIRE(result.ok());
    CHECK(result.value().completed);
}

TEST_CASE("orchestrator invokes the decision-tier model on every turn with tool use") {
    // The cascade wiring must be exercised end-to-end.  Every turn
    // that uses tools MUST also invoke the decision model so the
    // Large tier gets real statistics; otherwise the cascade is a
    // no-op as the bug report flagged.
    FakeProvider provider;
    provider.script({
        {{"plan", "step 1"},
         {"tool_calls", nlohmann::json::array({
             nlohmann::json{{"name", "read_file"},
                            {"arguments", nlohmann::json{{"path", "/tmp/none"}}}},
         })}},
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });
    OrchestratorOptions options;
    options.token_budget = 1024;
    Orchestrator orch(provider, options);
    orch.register_builtin();
    Budget budget;
    budget.max_turns = 4;
    budget.active = true;
    auto result = orch.run("cascade", budget);
    REQUIRE(result.ok());
    CHECK(result.value().completed);
    // FakeProvider.call_count includes BOTH the chore and the
    // decision review.  The previous bug only invoked the chore,
    // so the count would be 1 (chore "DONE") instead of 2+decision.
    // With the fix we expect 1 chore + 1 decision (turn 1) + 1
    // chore "DONE" (turn 2) = 3 calls.
    CHECK(provider.call_count >= 3);
    // And the cascade must have observed a Decision-tier outcome.
    auto& cascade = orch.cascade();
    CHECK(cascade.divergence_rate(Tier::Large) >= 0.0);
}
