#include <catch2/catch_test_macros.hpp>
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
