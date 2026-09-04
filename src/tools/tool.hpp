// Intentionally minimal: this file lives in the `tools` module and
// depends on nothing except the public `core` types. `ToolRegistry`
// is declared in `core/registry.hpp` so the `core` library can be used
// without pulling in the `tools` library. The `tools` library, in turn,
// depends on `core` and provides the built-in tools.
#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include "core/types.hpp"

namespace swiftagent {

class ToolCall;
class ToolResult;
class ToolContext;

using ToolCall = swiftagent::ToolCall;

struct ToolDescriptor {
    std::string name;
    std::string description;
    nlohmann::json schema;
    std::vector<std::string> declared_resources;
};

class Tool {
public:
    virtual ~Tool() = default;
    [[nodiscard]] virtual ToolDescriptor descriptor() const = 0;
    [[nodiscard]] virtual ToolResult invoke(const ToolCall& call, ToolContext& ctx) = 0;
};

struct ToolResult {
    bool ok{true};
    nlohmann::json output;
    std::string error_message;
    std::vector<std::string> observed_resources;
};

class ToolContext {
public:
    virtual ~ToolContext() = default;
    [[nodiscard]] virtual std::string read_file(const std::string& path) = 0;
    [[nodiscard]] virtual std::string exec(const std::string& cmd) = 0;
    [[nodiscard]] virtual bool file_exists(const std::string& path) const = 0;
};

} // namespace swiftagent
