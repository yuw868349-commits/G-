#include "core/orchestrator.hpp"

#include <chrono>
#include <map>
#include <sstream>
#include <utility>

#include "tools/builtin.hpp"

namespace praxis {

namespace {

// Convert a single raw tool-call entry into a `ToolCall`.
// OpenAI returns `arguments` as a JSON-encoded string; the fake provider
// already passes a parsed object.  Normalize both shapes to the canonical
// JSON-string form that `ToolExecutor` (and the builtin tools) expect.
std::string canonicalize_arguments(const nlohmann::json& raw_args) {
    if (raw_args.is_string()) {
        return raw_args.get<std::string>();
    }
    if (raw_args.is_object() || raw_args.is_array()) {
        return raw_args.dump();
    }
    return "{}";
}

std::vector<ToolCall> extract_tool_calls(const nlohmann::json& raw) {
    std::vector<ToolCall> calls;
    if (!raw.is_object() || !raw.contains("tool_calls") || !raw["tool_calls"].is_array()) {
        return calls;
    }
    std::uint64_t ordinal = 0;
    for (const auto& tc : raw["tool_calls"]) {
        ToolCall call;
        call.name = tc.value("name", "");
        const nlohmann::json& args_raw = tc.value("arguments", nlohmann::json::object());
        call.arguments = canonicalize_arguments(args_raw);
        call.ordinal = ordinal++;
        if (!call.name.empty()) {
            calls.push_back(std::move(call));
        }
    }
    return calls;
}

std::string trim_for_log(const std::string& s, std::size_t cap = 80) {
    if (s.size() <= cap) {
        return s;
    }
    return s.substr(0, cap) + "...";
}

} // namespace

Orchestrator::Orchestrator(Provider& provider, OrchestratorOptions options)
    : options_(std::move(options)),
      executor_(registry_),
      context_(options_.token_budget) {
    // Wrap the user-supplied provider in a RetryingProvider so transient
    // network/HTTP failures are absorbed by the orchestrator instead of
    // taking down the whole run.  The shared_ptr holds a no-op deleter
    // because `provider` has external lifetime.
    auto retrying = std::make_shared<RetryingProvider>(
        std::shared_ptr<Provider>(&provider, [](Provider*) {}),
        options_.retry_policy);
    router_.set(Tier::Small, retrying);
    router_.set(Tier::Large, retrying);
}

Orchestrator::Orchestrator(ProviderRouter& router, OrchestratorOptions options)
    : options_(std::move(options)),
      executor_(registry_),
      context_(options_.token_budget) {
    (void)router;  // The router is owned by the orchestrator; we never read this reference again.
}

void Orchestrator::register_builtin(std::shared_ptr<ToolContext> ctx) {
    if (ctx) {
        tool_ctx_ = std::move(ctx);
    }
    register_builtin_tools(registry_, tool_ctx_);
}

void Orchestrator::attach_observer(std::shared_ptr<EventSink> sink) {
    replay_.subscribe(std::move(sink));
}

void Orchestrator::register_tool(std::unique_ptr<Tool> tool) {
    if (!tool) {
        return;
    }
    registry_.register_tool(std::move(tool));
}

void Orchestrator::invalidate_cache_dependency(const std::string& dep) {
    cache_.invalidate_dependency(dep);
}

void Orchestrator::set_provider(Tier tier, std::shared_ptr<Provider> provider) {
    if (!provider) {
        return;
    }
    // Wrap the new provider in a RetryingProvider so callers that hot-
    // swap a tier (e.g. for failover) keep the same retry semantics as
    // the constructor path.
    auto retrying = std::make_shared<RetryingProvider>(
        std::move(provider), options_.retry_policy);
    router_.set(tier, std::move(retrying));
}

Result<ModelResponse>
Orchestrator::complete_for(Role role, const Messages& messages) {
    Tier tier = cascade_.route_for(role);
    if (!router_.has_tier(tier)) {
        return Result<ModelResponse>::fail(
            Error{ErrorKind::Internal, "no provider for tier " +
                  std::to_string(static_cast<int>(tier))});
    }
    return router_.for_tier(tier).complete(messages);
}

void Orchestrator::observe_outcome(Tier tier, Role role,
                                   const Result<ModelResponse>& result) {
    if (!result.ok()) {
        cascade_.record_failure(tier, role);
        return;
    }
    // Heuristic divergence signal: empty plan + tool use is unusual.
    const auto& resp = result.value();
    bool diverged = resp.outcome.plan.empty() && resp.outcome.has_tool_use;
    cascade_.record_outcome(tier, role, diverged);
}

std::vector<Orchestrator::CallView>
Orchestrator::build_call_views(const std::vector<ToolCall>& calls) const {
    std::vector<CallView> out;
    out.reserve(calls.size());
    for (const auto& c : calls) {
        CallView view;
        view.name = c.name;
        view.args = c.arguments;
        if (auto* tool = registry_.find(c.name)) {
            for (const auto& r : tool->descriptor().declared_resources) {
                view.dependencies.push_back(r);
            }
        }
        out.push_back(std::move(view));
    }
    return out;
}

void Orchestrator::apply_side_effects() {
    if (options_.side_effect_root.empty()) {
        return;
    }
    auto changed = side_effects_.diff(options_.side_effect_root);
    if (changed.empty()) {
        return;
    }
    nlohmann::json payload = nlohmann::json::array();
    for (const auto& p : changed) {
        payload.push_back(p);
    }
    replay_.record(EventKind::WorkingSetRendered,
                   {{"side_effects", std::move(payload)}});
    for (const auto& p : changed) {
        cache_.invalidate_dependency("file:" + p);
    }
}

bool Orchestrator::budget_exhausted(const Budget& budget, double cost,
                                    std::string& reason) const {
    if (!budget.active) {
        return false;
    }
    if (budget.max_cost > 0.0 && cost >= budget.max_cost) {
        reason = "max_cost";
        return true;
    }
    return false;
}

std::string Orchestrator::cache_key(const CallView& view) const {
    std::string key = view.name;
    key += '|';
    key += view.args;
    return key;
}

void Orchestrator::feed_working_set(Messages& messages,
                                    const TurnContext& turn_ctx) {
    WorkingSet ws = context_.render_working_set(turn_ctx);
    if (ws.render.empty()) {
        return;
    }
    messages.push_back(Message{"system", std::move(ws.render), std::nullopt});
}

Result<RunResult> Orchestrator::run(const std::string& task,
                                    const Budget& budget) {
    RunResult result;
    auto wall_start = std::chrono::steady_clock::now();
    replay_.record(EventKind::TurnStarted, {{"task", task}});

    if (!options_.side_effect_root.empty()) {
        side_effects_.snapshot(options_.side_effect_root);
    }

    Messages messages;
    messages.push_back(Message{"user", task, std::nullopt});

    std::uint32_t max_turns = budget.active ? budget.max_turns : 64u;
    std::uint64_t prompt_tokens = 0;
    std::uint64_t completion_tokens = 0;

    for (std::uint32_t turn = 0; turn < max_turns; ++turn) {
        result.turns = turn + 1;
        replay_.record(EventKind::ModelRequested, {{"turn", turn}});

        auto response = complete_for(Role::Chore, messages);
        Tier chosen_tier = cascade_.route_for(Role::Chore);
        observe_outcome(chosen_tier, Role::Chore, response);

        if (!response.ok()) {
            replay_.record(EventKind::Degraded,
                           {{"error", response.error().message}});
            telemetry_.set_wall_clock_ms(
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - wall_start).count());
            return Result<RunResult>::fail(response.error());
        }
        auto& resp = response.value();
        replay_.record(EventKind::ModelResponded,
                       {{"plan", trim_for_log(resp.outcome.plan)}});

        std::uint32_t turn_prompt = 0;
        std::uint32_t turn_completion = 0;
        if (resp.raw.contains("usage")) {
            const auto& usage = resp.raw["usage"];
            turn_prompt = usage.value("prompt_tokens", 0u);
            turn_completion = usage.value("completion_tokens", 0u);
            telemetry_.record_token(turn_prompt, turn_completion);
            prompt_tokens += turn_prompt;
            completion_tokens += turn_completion;
            const double turn_cost =
                (turn_prompt / 1000.0) * options_.pricing.prompt_per_1k +
                (turn_completion / 1000.0) * options_.pricing.completion_per_1k;
            result.cost += turn_cost;
        }

        if (std::string reason; budget_exhausted(budget, result.cost, reason)) {
            result.bounded = true;
            result.bounded_reason = reason;
            replay_.record(EventKind::BudgetHit,
                           {{"reason", reason}, {"cost", result.cost}});
            break;
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

        // The Decision-tier model is invoked independently of the
        // Chore-tier model so that the cascade routing is real, not
        // cosmetic.  We ask the decision model to review the chore
        // response and either confirm or override the plan.  The
        // decision model is expected to be larger and more accurate
        // (e.g. GPT-4 vs GPT-4-mini) and is what gets recorded under
        // the Decision role in the cascade statistics.  If the
        // decision model rejects the chore plan, the override is
        // surfaced back into the model stream for the next turn.
        Messages decision_messages = messages;
        decision_messages.push_back(Message{
            "user",
            "Decision review: the chore-tier model proposed the plan '"
                + trim_for_log(resp.outcome.plan)
                + "' with "
                + std::to_string(calls.size())
                + " tool calls. Reply with either 'CONFIRM' to keep the "
                  "plan, or 'OVERRIDE: <new plan>' to change it. Empty plan "
                  "is treated as an override to halt tool use.",
            std::nullopt});
        Tier decision_tier = cascade_.route_for(Role::Decision);
        auto decision_response = complete_for(Role::Decision, decision_messages);
        observe_outcome(decision_tier, Role::Decision, decision_response);
        if (decision_response.ok()) {
            const auto& dplan = decision_response.value().outcome.plan;
            // Only count the decision model as a divergence when it
            // actively *rejects* the chore plan.  "CONFIRM" and an
            // empty plan are both treated as agreement.  An override
            // is recorded as a divergence and stored on the cascade.
            if (!dplan.empty() && dplan != "CONFIRM" &&
                dplan.rfind("OVERRIDE:", 0) == 0) {
                std::string override_plan = dplan.substr(std::string("OVERRIDE:").size());
                while (!override_plan.empty() &&
                       (override_plan.front() == ' ' ||
                        override_plan.front() == '\t')) {
                    override_plan.erase(override_plan.begin());
                }
                replay_.record(EventKind::Degraded,
                               {{"decision_override", override_plan}});
                cascade_.record_outcome(decision_tier, Role::Decision, true);
                // The override is fed back to the model: the chore's
                // proposed tools are dropped, and the next turn
                // starts with the override plan as the model input.
                if (calls.empty()) {
                    // No tool calls to drop, but the override is the
                    // final answer.  End the run.
                    result.completed = true;
                    result.final_output = override_plan;
                    replay_.record(EventKind::TaskEnded, {{"turn", turn}});
                    telemetry_.set_wall_clock_ms(
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - wall_start).count());
                    return Result<RunResult>::ok(std::move(result));
                }
                // Otherwise drop the tool calls and let the chore
                // model re-plan on the next turn.
                calls.clear();
                resp.outcome.has_tool_use = false;
                resp.outcome.plan = override_plan;
            }
        } else {
            // The decision model itself failed: the chore plan
            // stands, but the failure is recorded so the cascade
            // can escalate to a different tier next time.
            cascade_.record_failure(decision_tier, Role::Decision);
        }

        TurnContext turn_ctx;
        turn_ctx.goal = task;
        auto working = context_.render_working_set(turn_ctx);
        replay_.record(EventKind::WorkingSetRendered,
                       {{"tokens", working.token_count}});

        // Cache lookup: reuse prior outputs when args+deps are unchanged.
        std::vector<ToolCall> pending;
        std::map<std::size_t, nlohmann::json> hits;
        auto views = build_call_views(calls);
        for (std::size_t i = 0; i < views.size(); ++i) {
            const auto& v = views[i];
            auto key = cache_key(v);
            auto cached = cache_.get(key, v.dependencies);
            if (cached.has_value() && options_.cache_tools) {
                hits[i] = *cached;
                telemetry_.record_cache_hit(true);
                replay_.record(EventKind::CacheHit,
                               {{"tool", v.name}, {"key", key}});
            } else {
                pending.push_back(calls[i]);
                telemetry_.record_cache_hit(false);
            }
        }

        // Execute only the cache misses; later we splice the cached
        // results back into the same order as the model emitted them.
        std::vector<ToolExecutionRecord> records(calls.size());
        if (!pending.empty()) {
            auto executed = executor_.execute(pending, tool_ctx_);
            for (std::size_t k = 0; k < pending.size(); ++k) {
                std::size_t original_index = 0;
                for (std::size_t i = 0; i < calls.size(); ++i) {
                    if (calls[i].ordinal == pending[k].ordinal) {
                        original_index = i;
                        break;
                    }
                }
                records[original_index] = std::move(executed[k]);
            }
        }
        for (auto& [i, cached] : hits) {
            ToolExecutionRecord rec;
            rec.ordinal = calls[i].ordinal;
            rec.tool_name = calls[i].name;
            rec.arguments = calls[i].arguments;
            rec.from_cache = true;
            rec.result = ToolResult{true, cached, "", {}};
            records[i] = std::move(rec);
        }

        for (const auto& rec : records) {
            telemetry_.record_tool_call(rec.result.ok);
            replay_.record(EventKind::ToolFinished,
                           {{"tool", rec.tool_name}, {"ok", rec.result.ok}});
            if (!rec.result.ok) {
                cascade_.record_failure(chosen_tier, Role::Chore);
            }

            // Populate the cache so the next turn can hit it.
            if (rec.result.ok && options_.cache_tools) {
                auto view = std::find_if(views.begin(), views.end(),
                                          [&](const CallView& v) {
                                              return v.name == rec.tool_name &&
                                                     v.args == rec.arguments;
                                          });
                if (view != views.end()) {
                    cache_.put(cache_key(*view), rec.result.output,
                               view->dependencies, options_.cache_tools);
                }
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
                nlohmann::json{{"name", rec.tool_name},
                               {"ok", rec.result.ok},
                               {"from_cache", rec.from_cache}}
            });
        }

        // Surface the compressed working set to the model for the next
        // turn. Without this step the model would just see the raw tool
        // outputs and the digest would never be exercised.
        feed_working_set(messages, turn_ctx);

        // Detect files that changed during the turn and record them.
        apply_side_effects();
    }
    if (!result.bounded) {
        result.bounded = true;
        result.bounded_reason = "max_turns";
        replay_.record(EventKind::BudgetHit, {{"turns", result.turns}});
    }
    telemetry_.set_wall_clock_ms(
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - wall_start).count());
    return Result<RunResult>::ok(std::move(result));
}

} // namespace praxis
