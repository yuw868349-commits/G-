#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "core/cache.hpp"
#include "core/context_manager.hpp"
#include "core/error.hpp"
#include "core/event.hpp"
#include "core/model_cascade.hpp"
#include "core/registry.hpp"
#include "core/replay.hpp"
#include "core/side_effect.hpp"
#include "core/telemetry.hpp"
#include "core/tool_executor.hpp"
#include "core/types.hpp"
#include "llm/provider.hpp"
#include "llm/provider_router.hpp"
#include "llm/retrying_provider.hpp"
#include "tools/tool.hpp"

namespace swiftagent {

struct RunResult {
    bool completed{false};
    bool bounded{false};
    std::string bounded_reason;
    std::uint32_t turns{0};
    double cost{0.0};
    std::string final_output;
};

struct OrchestratorOptions {
    std::size_t token_budget{8192};
    // USD per 1k tokens; split into prompt/completion via PricingConfig.
    PricingConfig pricing{};
    // Root directory to snapshot for side-effect tracking. When empty,
    // side-effect tracking is disabled.
    std::filesystem::path side_effect_root{};
    // When true, treat every tool call as cacheable.
    bool cache_tools{true};
    // Retry policy applied to every Provider that the orchestrator
    // routes a request through. Leave the default to get finite retries
    // with exponential backoff; pass `RetryPolicy{1, std::chrono::milliseconds{0}}`
    // to disable.
    RetryPolicy retry_policy{};
};

// Main entry point. Owns all collaborators and exposes a narrow public
// API. The internal collaborators are reachable only through dedicated
// read-only accessors and writer methods, never through mutable
// references that callers could use to bypass invariants.
class Orchestrator {
public:
    explicit Orchestrator(Provider& provider,
                          OrchestratorOptions options = {});
    Orchestrator(ProviderRouter& router,
                 OrchestratorOptions options = {});

    [[nodiscard]] Result<RunResult> run(const std::string& task,
                                        const Budget& budget);

    void register_builtin(std::shared_ptr<ToolContext> ctx = nullptr);
    void attach_observer(std::shared_ptr<EventSink> sink);
    void register_tool(std::unique_ptr<Tool> tool);
    void set_executor_parallel(bool parallel) noexcept {
        executor_.set_parallel_safe(parallel);
    }

    // Forwarding methods for telemetry. The accessor returns a const
    // reference to prevent callers from bypassing the orchestrator's
    // own internal writes; these wrappers expose the mutating surface
    // that the example/test code needs.
    void set_baseline_ms(double ms) { telemetry_.set_baseline_ms(ms); }
    void record_module(const std::string& name, double ms) {
        telemetry_.record_module(name, ms);
    }
    void record_token(std::uint32_t prompt, std::uint32_t completion) {
        telemetry_.record_token(prompt, completion);
    }
    void record_cache_hit(bool hit) { telemetry_.record_cache_hit(hit); }
    void record_tool_call(bool success) { telemetry_.record_tool_call(success); }

    // Read-only accessors. Callers must not mutate the underlying
    // collaborators through these views; if they need to mutate, they
    // should go through the dedicated method on the orchestrator.
    [[nodiscard]] const ToolRegistry& registry() const noexcept { return registry_; }
    [[nodiscard]] const ContextManager& context() const noexcept { return context_; }
    [[nodiscard]] const ToolExecutor& executor() const noexcept { return executor_; }
    [[nodiscard]] const Cache& cache() const noexcept { return cache_; }
    [[nodiscard]] const ModelCascade& cascade() const noexcept { return cascade_; }
    [[nodiscard]] const Replay& replay() const noexcept { return replay_; }
    [[nodiscard]] const Telemetry& telemetry() const noexcept { return telemetry_; }
    [[nodiscard]] const SideEffectObserver& side_effects() const noexcept { return side_effects_; }

    // Cache control: callers may invalidate a dependency when they know
    // the underlying file/resource changed.
    void invalidate_cache_dependency(const std::string& dep);

    // Switch the underlying provider for a tier at runtime (e.g. to
    // roll out a model upgrade or load-failover to a backup).
    void set_provider(Tier tier, std::shared_ptr<Provider> provider);
    [[nodiscard]] std::size_t provider_count() const noexcept {
        return router_.size();
    }

private:
    struct CallView {
        std::string name;
        std::string args;
        std::vector<std::string> dependencies;
    };

    Result<ModelResponse> complete_for(Role role, const Messages& messages);
    void observe_outcome(Tier tier, Role role, const Result<ModelResponse>& result);
    [[nodiscard]] std::vector<CallView> build_call_views(
        const std::vector<ToolCall>& calls) const;
    void apply_side_effects();
    [[nodiscard]] bool budget_exhausted(const Budget& budget,
                                         double cost,
                                         std::string& reason) const;
    [[nodiscard]] std::string cache_key(const CallView& view) const;
    void feed_working_set(Messages& messages, const TurnContext& turn_ctx);

    OrchestratorOptions options_;
    ProviderRouter router_;
    ToolRegistry registry_;
    ToolExecutor executor_;
    ContextManager context_;
    Cache cache_;
    ModelCascade cascade_;
    Replay replay_;
    Telemetry telemetry_;
    SideEffectObserver side_effects_;
    std::shared_ptr<ToolContext> tool_ctx_;
};

} // namespace swiftagent
