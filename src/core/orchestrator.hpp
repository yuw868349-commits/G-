#pragma once

#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include "core/cache.hpp"
#include "core/context_manager.hpp"
#include "core/error.hpp"
#include "core/event.hpp"
#include "core/model_cascade.hpp"
#include "core/replay.hpp"
#include "core/telemetry.hpp"
#include "core/tool_executor.hpp"
#include "core/types.hpp"
#include "llm/provider.hpp"
#include "tools/registry.hpp"

namespace swiftagent {

struct RunResult {
    bool completed{false};
    bool bounded{false};
    std::uint32_t turns{0};
    std::string final_output;
};

class Orchestrator {
public:
    explicit Orchestrator(Provider& provider, std::size_t token_budget = 8192);

    [[nodiscard]] Result<RunResult> run(const std::string& task, const Budget& budget);

    [[nodiscard]] ToolRegistry& registry() noexcept { return registry_; }
    [[nodiscard]] ContextManager& context() noexcept { return context_; }
    [[nodiscard]] ToolExecutor& executor() noexcept { return executor_; }
    [[nodiscard]] Cache& cache() noexcept { return cache_; }
    [[nodiscard]] ModelCascade& cascade() noexcept { return cascade_; }
    [[nodiscard]] Replay& replay() noexcept { return replay_; }
    [[nodiscard]] Telemetry& telemetry() noexcept { return telemetry_; }

    void register_builtin();
    void attach_observer(std::shared_ptr<EventSink> sink);

private:
    Provider& provider_;
    ToolRegistry registry_;
    ToolExecutor executor_;
    ContextManager context_;
    Cache cache_;
    ModelCascade cascade_;
    Replay replay_;
    Telemetry telemetry_;
};

} // namespace swiftagent
