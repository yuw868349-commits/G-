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
        std::vector<std::string> resources;
        if (auto* tool = registry_.find(c.name)) {
            // Pull the per-call resource list (file paths, URLs, ...)
            // from the tool itself.  Two calls of the same tool are
            // only considered to conflict if they actually touch the
            // same address.
            resources = tool->resources_for(c);
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
        // fork() the supplied context once per worker so parallel
        // tool calls never share mutable state.  This addresses the
        // thread-safety contract of ToolContext: a context that has
        // been forked() is the sole owner of its state and may run
        // concurrently with sibling forks; the original instance is
        // not touched by the parallel workers.  Subclasses that
        // don't override fork() get a no-op StatelessContext.
        std::vector<std::future<ToolExecutionRecord>> futures;
        std::vector<std::unique_ptr<ToolContext>> forks;
        forks.reserve(group.size());
        for (auto idx : group) {
            auto forked = ctx ? ctx->fork() : nullptr;
            ToolContext* forked_raw = forked.get();
            forks.push_back(std::move(forked));
            futures.push_back(std::async(std::launch::async,
                [this, &calls, forked_raw, idx]() {
                    std::shared_ptr<ToolContext> local;
                    if (forked_raw) {
                        // Take a non-owning reference inside the
                        // worker; the `forks` vector keeps the
                        // instance alive for the lifetime of all
                        // futures in this group.
                        local = std::shared_ptr<ToolContext>(
                            forked_raw, [](ToolContext*) {});
                    }
                    return invoke_one(calls[idx], local);
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
        // forks is destroyed here, after all workers have finished,
        // so the underlying context instances outlive the workers.
    }
    return out;
}

} // namespace swiftagent
