#pragma once

#include <memory>
#include <string>
#include <vector>
#include "core/registry.hpp"
#include "mcp/json_rpc.hpp"
#include "tools/tool.hpp"

namespace praxis {

struct McpToolSpec {
    std::string name;
    std::string description;
    nlohmann::json schema = nlohmann::json::object();
};

class McpTool final : public Tool {
public:
    McpTool(std::string name, std::string description, nlohmann::json schema,
            std::shared_ptr<JsonRpcClient> client, std::string prefix);

    ToolDescriptor descriptor() const override;
    ToolResult invoke(const ToolCall& call, ToolContext& ctx) override;

private:
    std::string name_;
    std::string description_;
    nlohmann::json schema_;
    std::shared_ptr<JsonRpcClient> client_;
    std::string prefix_;
};

class McpHost {
public:
    explicit McpHost(ToolRegistry& registry);

    // Attach a transport that the McpHost will take shared ownership
    // of. The transport must live at least until the McpHost itself
    // is destroyed.
    void attach(std::shared_ptr<JsonRpcTransport> transport,
                const std::string& prefix = "");
    void attach_stdio(std::unique_ptr<JsonRpcTransport> transport,
                      const std::string& prefix = "");
    void attach_sse_url(const std::string& url, const std::string& prefix = "");

    [[nodiscard]] std::vector<std::string> tools() const;
    [[nodiscard]] std::size_t size() const noexcept { return tools_.size(); }
    [[nodiscard]] std::size_t client_count() const noexcept { return clients_.size(); }

private:
    void register_tools_from_payload(const nlohmann::json& payload,
                                    const std::string& prefix,
                                    std::shared_ptr<JsonRpcClient> client);
    void register_remote_tool(const McpToolSpec& spec, const std::string& prefix,
                              std::shared_ptr<JsonRpcClient> client);

    ToolRegistry& registry_;
    std::vector<McpToolSpec> tools_;
    std::vector<std::shared_ptr<JsonRpcClient>> clients_;
};

} // namespace praxis
