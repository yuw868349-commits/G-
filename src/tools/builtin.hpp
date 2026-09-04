#pragma once

#include <memory>
#include "tools/registry.hpp"
#include "tools/tool.hpp"

namespace praxis {

class ReadFileTool;
class WriteFileTool;
class ShellTool;

void register_builtin_tools(ToolRegistry& registry,
                            std::shared_ptr<ToolContext> ctx = nullptr);

class ReadFileTool final : public Tool {
public:
    explicit ReadFileTool(std::shared_ptr<ToolContext> ctx);
    ToolDescriptor descriptor() const override;
    ToolResult invoke(const ToolCall& call, ToolContext& ctx) override;
    [[nodiscard]] std::vector<std::string>
    resources_for(const ToolCall& call) const override;

private:
    std::shared_ptr<ToolContext> ctx_;
};

class WriteFileTool final : public Tool {
public:
    explicit WriteFileTool(std::shared_ptr<ToolContext> ctx);
    ToolDescriptor descriptor() const override;
    ToolResult invoke(const ToolCall& call, ToolContext& ctx) override;
    [[nodiscard]] std::vector<std::string>
    resources_for(const ToolCall& call) const override;

private:
    std::shared_ptr<ToolContext> ctx_;
};

class ShellTool final : public Tool {
public:
    explicit ShellTool(std::shared_ptr<ToolContext> ctx);
    ToolDescriptor descriptor() const override;
    ToolResult invoke(const ToolCall& call, ToolContext& ctx) override;

private:
    std::shared_ptr<ToolContext> ctx_;
};

} // namespace praxis
