#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace praxis {

struct ToolCall {
    std::string name;
    std::string arguments;
    std::uint64_t ordinal{0};
};

struct TurnOutcome {
    std::string plan;
    bool has_tool_use{false};
    std::uint32_t tool_count{0};
    bool completed{false};

    [[nodiscard]] bool is_valid_turn() const noexcept {
        return !plan.empty();
    }
};

struct ProgressScore {
    double score{0.0};
};

struct Budget {
    std::uint32_t max_turns{64};
    double max_cost{0.0};
    bool active{false};
};

struct PricingConfig {
    // USD per 1000 prompt tokens.
    double prompt_per_1k{0.0};
    // USD per 1000 completion tokens.
    double completion_per_1k{0.0};
};

enum class ExecutionForm { Serial, Grouped, Pipelined };

struct DependencySummary {
    ExecutionForm form{ExecutionForm::Serial};
    std::vector<std::vector<ToolCall>> groups;
};

} // namespace praxis
