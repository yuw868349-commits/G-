#include "core/tool_executor.hpp"

#include "core/dependency_graph.hpp"

#include <future>
#include <thread>
#include <utility>

namespace swiftagent {

ToolExecutor::ToolExecutor(ToolRegistry& registry) : registry_(registry) {}

ToolExecutionRecord
ToolExecutor::invoke_one(const ToolCall& call,
                         const std::shared_ptr<ToolContext>& ctx) {
    ToolExecutionRecord rec;
    rec.ordinal = call.ordinal;
    rec.tool_name = call.name;
    rec.arguments = call.arguments;
    auto* tool = registry_.find(call.name);
    if (!tool) {
        rec.result = ToolResult{false, nullptr, "tool not found: " + call.name, {}};
        return rec;
    }
    if (!ctx) {
        rec.result = ToolResult{false, nullptr,
                                "no tool context available for: " + call.name, {}};
        return rec;
    }
    rec.result = tool->invoke(call, *ctx);
    rec.observed_resources = rec.result.observed_resources;
    return rec;
}

std::vector<ToolExecutionRecord>
ToolExecutor::execute(const std::vector<ToolCall>& calls,
                      std::shared_ptr<ToolContext> ctx) {
    std::vector<ToolExecutionRecord> out;
    if (calls.empty()) {
        return out;
    }
    DependencyGraph graph;
    for (const auto& c : calls) {
        auto* tool = registry_.find(c.name);
        std::vector<std::string> resources;
        if (tool) {
            for (const auto& r : tool->descriptor().declared_resources) {
                resources.push_back(r + ":" + c.name);
            }
        }
        graph.add_call(c, resources);
    }
    std::vector<std::vector<std::size_t>> groups;
    if (parallel_safe_) {
        groups = graph.schedule();
    } else {
        groups.push_back({});
        for (std::size_t i = 0; i < calls.size(); ++i) {
            groups.back().push_back(i);
        }
    }
    if (groups.empty()) {
        groups.push_back({});
        for (std::size_t i = 0; i < calls.size(); ++i) {
            groups.back().push_back(i);
        }
    }
    for (const auto& group : groups) {
        std::vector<std::future<ToolExecutionRecord>> futures;
        for (auto idx : group) {
            futures.push_back(std::async(std::launch::async,
                [this, &calls, ctx, idx]() {
                    return invoke_one(calls[idx], ctx);
                }));
        }
        for (auto& fut : futures) {
            auto rec = fut.get();
            {
                std::lock_guard<std::mutex> lock(mtx_);
                ++invocations_;
                if (!rec.result.ok) {
                    ++failures_;
                }
            }
            out.push_back(std::move(rec));
        }
    }
    return out;
}

} // namespace swiftagent
