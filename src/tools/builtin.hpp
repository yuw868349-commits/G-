#pragma once

#include <memory>
#include "tools/registry.hpp"
#include "tools/tool.hpp"

namespace swiftagent {

class ReadFileTool;
class WriteFileTool;
class ShellTool;

void register_builtin_tools(ToolRegistry& registry);

class ReadFileTool final : public Tool {
public:
    explicit ReadFileTool(std::shared_ptr<ToolContext> ctx);
    ToolDescriptor descriptor() const override;
    ToolResult invoke(const ToolCall& call, ToolContext& ctx) override;
};

class WriteFileTool final : public Tool {
public:
    explicit WriteFileTool(std::shared_ptr<ToolContext> ctx);
    ToolDescriptor descriptor() const override;
    ToolResult invoke(const ToolCall& call, ToolContext& ctx) override;
};

class ShellTool final : public Tool {
public:
    explicit ShellTool(std::shared_ptr<ToolContext> ctx);
    ToolDescriptor descriptor() const override;
    ToolResult invoke(const ToolCall& call, ToolContext& ctx) override;
};

} // namespace swiftagent
