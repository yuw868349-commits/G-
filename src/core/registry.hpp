#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "tools/tool.hpp"

namespace praxis {

// `ToolRegistry` lives in the `core` library so that `core/orchestrator`,
// `core/tool_executor`, and `mcp/mcp_host` can use it without depending
// on the `tools` library (which would in turn depend on `core`).
// The `tools` library depends on `core` and contributes built-in tools
// that get registered into this same registry.
class ToolRegistry {
public:
    void register_tool(std::unique_ptr<Tool> tool);
    [[nodiscard]] Tool* find(const std::string& name) const;
    [[nodiscard]] std::vector<ToolDescriptor> list() const;
    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] std::size_t size() const noexcept { return tools_.size(); }

private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
};

} // namespace praxis
