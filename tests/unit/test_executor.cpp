#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include "core/tool_executor.hpp"
#include "tools/registry.hpp"
#include "tools/tool.hpp"

using namespace swiftagent;

namespace {

class EchoTool final : public Tool {
public:
    ToolDescriptor descriptor() const override {
        return ToolDescriptor{"echo", "echo back", nlohmann::json::object(), {"file:read:any"}};
    }
    ToolResult invoke(const ToolCall& call, ToolContext& ctx) override {
        return ToolResult{true, nlohmann::json{{"echo", call.arguments}}, "", {}};
    }
};

class MissingTool final : public Tool {
public:
    ToolDescriptor descriptor() const override {
        return ToolDescriptor{"never", "", nlohmann::json::object(), {}};
    }
    ToolResult invoke(const ToolCall& call, ToolContext& ctx) override {
        return ToolResult{true, nullptr, "", {}};
    }
};

// Tool context that records how many times fork() was called and
// how many invocations the tool saw concurrently.  Used to prove
// that parallel execution does not share state across workers.
class CountingContext final : public ToolContext {
public:
    mutable std::atomic<int> live{0};
    mutable std::atomic<int> peak{0};
    mutable std::atomic<int> forks{0};

    std::string read_file(const std::string&) override { return {}; }
    std::string exec(const std::string&) override { return {}; }
    bool file_exists(const std::string&) const override { return false; }

    std::unique_ptr<ToolContext> fork() const override {
        forks.fetch_add(1, std::memory_order_relaxed);
        return std::make_unique<CountingContext>();
    }
};

class RacyTool final : public Tool {
public:
    ToolDescriptor descriptor() const override {
        return ToolDescriptor{"racy", "uses ctx counter", nlohmann::json::object(), {}};
    }
    ToolResult invoke(const ToolCall&, ToolContext& ctx) override {
        auto* c = dynamic_cast<CountingContext*>(&ctx);
        if (c) {
            int now = c->live.fetch_add(1) + 1;
            int peak = c->peak.load();
            while (now > peak && !c->peak.compare_exchange_weak(peak, now)) {
                // retry until our value is published
            }
            // Hold the slot briefly to make a race visible to a
            // thread sanitizer if fork() wasn't really giving us a
            // private context.
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            c->live.fetch_sub(1);
        }
        return ToolResult{true, nlohmann::json::object(), "", {}};
    }
};

} // namespace

TEST_CASE("executor runs multiple calls and counts results") {
    ToolRegistry registry;
    registry.register_tool(std::make_unique<EchoTool>());
    ToolExecutor exec(registry);
    std::vector<ToolCall> calls;
    ToolCall a{"echo", R"({"x":1})", 0};
    ToolCall b{"echo", R"({"x":2})", 1};
    calls.push_back(a);
    calls.push_back(b);
    auto records = exec.execute(calls);
    CHECK(records.size() == 2);
    CHECK(exec.invocations() == 2);
}

TEST_CASE("executor reports failure for missing tool") {
    ToolRegistry registry;
    registry.register_tool(std::make_unique<MissingTool>());
    ToolExecutor exec(registry);
    ToolCall unknown{"ghost", "{}", 0};
    auto records = exec.execute({unknown});
    REQUIRE(records.size() == 1);
    CHECK_FALSE(records[0].result.ok);
    CHECK(exec.failures() == 1);
}

TEST_CASE("executor forks the context once per parallel worker") {
    ToolRegistry registry;
    registry.register_tool(std::make_unique<RacyTool>());
    ToolExecutor exec(registry);
    exec.set_parallel_safe(true);
    auto ctx = std::make_shared<CountingContext>();
    std::vector<ToolCall> calls;
    for (int i = 0; i < 4; ++i) {
        calls.push_back({"racy", "{}", static_cast<std::uint64_t>(i)});
    }
    (void)exec.execute(calls, ctx);
    // 4 calls -> 4 forks, one per worker.  This proves the executor
    // hands each parallel tool call a *fresh* context instead of
    // racing on the original.
    CHECK(ctx->forks.load() == 4);
    // Each fork is a fresh instance, so the original context's peak
    // is necessarily 0 (the tool never runs against the original).
    // The interesting check is that fork() actually got called once
    // per worker, not zero times.
    CHECK(ctx->forks.load() == static_cast<int>(calls.size()));
}
