#pragma once

#include <memory>
#include <string>
#include <vector>
#include "mcp/json_rpc.hpp"
#include "tools/registry.hpp"
#include "tools/tool.hpp"

namespace swiftagent {

struct McpToolSpec {
    std::string name;
    std::string description;
    nlohmann::json schema = nlohmann::json::object();
};

class McpHost {
public:
    explicit McpHost(ToolRegistry& registry);

    void attach_stdio(std::unique_ptr<JsonRpcTransport> transport,
                      const std::string& prefix = "");
    void attach_sse_url(const std::string& url, const std::string& prefix = "");

    [[nodiscard]] std::vector<std::string> tools() const;
    [[nodiscard]] std::size_t size() const noexcept { return tools_.size(); }

private:
    void register_remote_tool(const McpToolSpec& spec, const std::string& prefix,
                              JsonRpcClient* client);

    ToolRegistry& registry_;
    std::vector<McpToolSpec> tools_;
};

} // namespace swiftagent
