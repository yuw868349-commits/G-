#include "core/orchestrator.hpp"

#include <chrono>
#include <utility>

#include "tools/builtin.hpp"

namespace swiftagent {

namespace {

std::vector<ToolCall> extract_tool_calls(const nlohmann::json& raw) {
    std::vector<ToolCall> calls;
    if (!raw.is_object() || !raw.contains("tool_calls") || !raw["tool_calls"].is_array()) {
        return calls;
    }
    std::uint64_t ordinal = 0;
    for (const auto& tc : raw["tool_calls"]) {
        ToolCall call;
        call.name = tc.value("name", "");
        call.arguments = tc.contains("arguments") ? tc["arguments"].dump() : "{}";
        call.ordinal = ordinal++;
        if (!call.name.empty()) {
            calls.push_back(std::move(call));
        }
    }
    return calls;
}

} // namespace

Orchestrator::Orchestrator(Provider& provider, std::size_t token_budget)
    : provider_(provider),
      executor_(registry_),
      context_(token_budget) {}

void Orchestrator::register_builtin() {
    swiftagent::register_builtin_tools(registry_);
}

void Orchestrator::attach_observer(std::shared_ptr<EventSink> sink) {
    replay_.subscribe(std::move(sink));
}

Result<RunResult> Orchestrator::run(const std::string& task, const Budget& budget) {
    RunResult result;
    auto wall_start = std::chrono::steady_clock::now();
    replay_.record(EventKind::TurnStarted, {{"task", task}});
    Messages messages;
    messages.push_back(Message{"user", task, std::nullopt});
    std::uint32_t max_turns = budget.active ? budget.max_turns : 64u;
    for (std::uint32_t turn = 0; turn < max_turns; ++turn) {
        result.turns = turn + 1;
        replay_.record(EventKind::ModelRequested, {{"turn", turn}});
        auto response = provider_.complete(messages);
        if (!response.ok()) {
            replay_.record(EventKind::Degraded, {{"error", response.error().message}});
            telemetry_.set_wall_clock_ms(
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - wall_start).count());
            return Result<RunResult>::fail(response.error());
        }
        auto& resp = response.value();
        replay_.record(EventKind::ModelResponded, {{"plan", resp.outcome.plan}});
        if (resp.raw.contains("usage")) {
            const auto& usage = resp.raw["usage"];
            telemetry_.record_token(
                usage.value("prompt_tokens", 0u),
                usage.value("completion_tokens", 0u));
        }
        if (!resp.outcome.has_tool_use) {
            result.completed = true;
            result.final_output = resp.outcome.plan;
            replay_.record(EventKind::TaskEnded, {{"turn", turn}});
            telemetry_.set_wall_clock_ms(
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - wall_start).count());
            return Result<RunResult>::ok(std::move(result));
        }
        auto calls = extract_tool_calls(resp.raw);
        replay_.record(EventKind::ToolCalled, {{"count", calls.size()}});
        TurnContext turn_ctx;
        turn_ctx.goal = task;
        auto working = context_.render_working_set(turn_ctx);
        replay_.record(EventKind::WorkingSetRendered,
                       {{"tokens", working.token_count}});
        auto records = executor_.execute(calls);
        for (const auto& rec : records) {
            telemetry_.record_tool_call(rec.result.ok);
            replay_.record(EventKind::ToolFinished,
                           {{"tool", rec.tool_name}, {"ok", rec.result.ok}});
            if (!rec.result.ok) {
                cascade_.record_failure(Tier::Small, Role::Chore);
            }
            std::string body = rec.result.output.is_null()
                                   ? rec.result.error_message
                                   : rec.result.output.dump();
            auto fact_id = context_.store().append("tool_result", body);
            context_.record_tool_result(fact_id);
            turn_ctx.recent_tool_results.push_back(fact_id);
            messages.push_back(Message{
                "tool",
                body,
                nlohmann::json{{"name", rec.tool_name}, {"ok", rec.result.ok}}
            });
        }
    }
    result.bounded = true;
    replay_.record(EventKind::BudgetHit, {{"turns", result.turns}});
    telemetry_.set_wall_clock_ms(
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - wall_start).count());
    return Result<RunResult>::ok(std::move(result));
}

} // namespace swiftagent
