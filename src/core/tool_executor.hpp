#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "core/registry.hpp"
#include "core/types.hpp"
#include "tools/tool.hpp"

namespace swiftagent {

struct ToolExecutionRecord {
    std::size_t ordinal{0};
    std::string tool_name;
    std::string arguments;
    ToolResult result;
    std::vector<std::string> observed_resources;
    bool from_cache{false};
};

class ToolExecutor {
public:
    explicit ToolExecutor(ToolRegistry& registry);

    [[nodiscard]] std::vector<ToolExecutionRecord>
    execute(const std::vector<ToolCall>& calls,
            std::shared_ptr<ToolContext> ctx = nullptr);

    [[nodiscard]] bool parallel_safe() const noexcept { return parallel_safe_; }
    void set_parallel_safe(bool value) noexcept { parallel_safe_ = value; }

    [[nodiscard]] std::size_t invocations() const noexcept { return invocations_; }
    [[nodiscard]] std::size_t failures() const noexcept { return failures_; }

private:
    [[nodiscard]] ToolExecutionRecord
    invoke_one(const ToolCall& call,
               const std::shared_ptr<ToolContext>& ctx);

    ToolRegistry& registry_;
    mutable std::mutex mtx_;
    bool parallel_safe_{true};
    std::size_t invocations_{0};
    std::size_t failures_{0};
};

} // namespace swiftagent
