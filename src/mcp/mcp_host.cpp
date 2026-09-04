#include "mcp/mcp_host.hpp"

#include <stdexcept>
#include <utility>

namespace swiftagent {

namespace {

class McpTool final : public Tool {
public:
    McpTool(std::string name, std::string description, nlohmann::json schema,
            std::shared_ptr<JsonRpcClient> client, std::string prefix)
        : name_(std::move(name)),
          description_(std::move(description)),
          schema_(std::move(schema)),
          client_(std::move(client)),
          prefix_(std::move(prefix)) {}

    ToolDescriptor descriptor() const override {
        return ToolDescriptor{name_, description_, schema_, {"mcp"}};
    }

    ToolResult invoke(const ToolCall& call, ToolContext&) override {
        if (!client_) {
            return ToolResult{false, nullptr, "mcp client disconnected", {}};
        }
        nlohmann::json params;
        try {
            params = nlohmann::json::parse(call.arguments);
        } catch (const nlohmann::json::exception&) {
            return ToolResult{false, nullptr, "invalid arguments", {}};
        }
        try {
            auto result = client_->call(prefix_ + "/" + call.name, params);
            return ToolResult{true, result, "", {}};
        } catch (const std::exception& e) {
            return ToolResult{false, nullptr, e.what(), {}};
        }
    }

private:
    std::string name_;
    std::string description_;
    nlohmann::json schema_;
    std::shared_ptr<JsonRpcClient> client_;
    std::string prefix_;
};

} // namespace

McpHost::McpHost(ToolRegistry& registry) : registry_(registry) {}

void McpHost::attach_stdio(std::unique_ptr<JsonRpcTransport> transport,
                           const std::string& prefix) {
    auto* raw = transport.release();
    auto client = std::make_shared<JsonRpcClient>(*raw);
    auto payload = client->call("tools/list");
    if (payload.contains("tools") && payload["tools"].is_array()) {
        for (const auto& t : payload["tools"]) {
            McpToolSpec spec;
            spec.name = t.value("name", "");
            spec.description = t.value("description", "");
            spec.schema = t.value("inputSchema", nlohmann::json::object());
            register_remote_tool(spec, prefix, client.get());
        }
    }
}

void McpHost::attach_sse_url(const std::string& url, const std::string& prefix) {
    (void)url;
    (void)prefix;
    // Real implementation would open an SSE stream; placeholder for now.
}

std::vector<std::string> McpHost::tools() const {
    std::vector<std::string> out;
    out.reserve(tools_.size());
    for (const auto& t : tools_) {
        out.push_back(t.name);
    }
    return out;
}

void McpHost::register_remote_tool(const McpToolSpec& spec, const std::string& prefix,
                                   JsonRpcClient* client) {
    if (spec.name.empty()) {
        return;
    }
    tools_.push_back(spec);
    std::string full_name = prefix.empty() ? spec.name : prefix + "__" + spec.name;
    auto shared_client = static_cast<std::shared_ptr<JsonRpcClient>*>(nullptr);
    (void)shared_client;
    auto stashed = std::shared_ptr<JsonRpcClient>(client, [](JsonRpcClient*) {});
    registry_.register_tool(std::make_unique<McpTool>(
        full_name, spec.description, spec.schema, stashed, prefix));
}

} // namespace swiftagent
