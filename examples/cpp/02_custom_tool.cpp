// Register a custom tool, then run a task that uses it.
//
// Build: cmake --build build --target example_custom_tool
// Run:   ./build/example_custom_tool

#include <iostream>
#include <memory>
#include <utility>

#include "core/orchestrator.hpp"
#include "core/types.hpp"
#include "llm/fake_provider.hpp"
#include "tools/registry.hpp"
#include "tools/tool.hpp"

namespace {

// A noop tool context: tools normally read files / run commands, but
// this stub returns empty results so the example stays self-contained.
class StubContext final : public praxis::ToolContext
{
public:
    std::string read_file(const std::string&) override { return {}; }
    std::string exec(const std::string&) override { return {}; }
    bool file_exists(const std::string&) const override { return false; }
};

class ReverseTool final : public praxis::Tool
{
public:
    explicit ReverseTool(std::shared_ptr<praxis::ToolContext> ctx) : ctx_(std::move(ctx)) {}

    praxis::ToolDescriptor descriptor() const override
    {
        return praxis::ToolDescriptor{
            "reverse",
            "Reverse a string.",
            {{"type", "object"}, {"properties", {{"text", {{"type", "string"}}}}}, {"required", {"text"}}},
            {},
        };
    }

    praxis::ToolResult invoke(const praxis::ToolCall& call, praxis::ToolContext&) override
    {
        nlohmann::json args;
        try
        {
            args = nlohmann::json::parse(call.arguments);
        }
        catch (const nlohmann::json::exception&)
        {
            return praxis::ToolResult{false, nullptr, "bad arguments", {}};
        }
        const std::string text = args.value("text", "");
        std::string out(text.rbegin(), text.rend());
        return praxis::ToolResult{true, out, "", {}};
    }

private:
    std::shared_ptr<praxis::ToolContext> ctx_;
};

} // namespace

int main()
{
    using namespace praxis;

    auto stub = std::make_shared<StubContext>();
    auto reverse = std::make_unique<ReverseTool>(stub);

    FakeProvider provider;
    provider.script({
        {{"plan", "call reverse(\"hello\")"},
         {"tool_calls", nlohmann::json::array({
                            {{"name", "reverse"}, {"arguments", {{"text", "hello"}}}},
                        })}},
        {{"plan", "tool returned output, done"}, {"completed", true}},
    });

    Orchestrator orchestrator(provider);
    orchestrator.register_builtin();
    orchestrator.register_tool(std::move(reverse));

    Budget budget;
    budget.active = true;
    budget.max_turns = 4;

    auto result = orchestrator.run("reverse the word 'hello'", budget);
    if (!result.ok())
    {
        std::cerr << "error: " << result.error().message << "\n";
        return 1;
    }

    auto& out = result.value();
    std::cout << "turns=" << out.turns << " completed=" << (out.completed ? "yes" : "no") << "\n";
    std::cout << "telemetry speedup_x=" << orchestrator.telemetry().speedup_x() << "\n";
    return 0;
}
