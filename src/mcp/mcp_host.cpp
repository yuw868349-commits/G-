#include "mcp/mcp_host.hpp"

#include <httplib.h>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace swiftagent {

McpTool::McpTool(std::string name, std::string description, nlohmann::json schema,
                 std::shared_ptr<JsonRpcClient> client, std::string prefix)
    : name_(std::move(name)),
      description_(std::move(description)),
      schema_(std::move(schema)),
      client_(std::move(client)),
      prefix_(std::move(prefix)) {}

ToolDescriptor McpTool::descriptor() const {
    return ToolDescriptor{name_, description_, schema_, {"mcp"}};
}

ToolResult McpTool::invoke(const ToolCall& call, ToolContext&) {
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

namespace {

class SseTransport final : public JsonRpcTransport {
public:
    SseTransport(std::unique_ptr<httplib::Client> cli, std::string endpoint)
        : client_(std::move(cli)), endpoint_(std::move(endpoint)) {}

    void send(const std::string& payload) override {
        auto res = client_->Post(endpoint_.c_str(), payload, "application/json");
        if (!res) {
            throw std::runtime_error("sse transport send failed");
        }
    }

    std::string receive() override {
        if (!pulled_) {
            auto res = client_->Get(endpoint_.c_str());
            if (!res) {
                return "";
            }
            last_event_ = parse_event(res->body);
            pulled_ = true;
        }
        return last_event_;
    }

    bool alive() const override { return true; }

private:
    static std::string parse_event(const std::string& body) {
        std::istringstream iss(body);
        std::string line;
        std::string data;
        while (std::getline(iss, line)) {
            if (line.rfind("data:", 0) == 0) {
                data += line.substr(5);
                data += "\n";
            }
        }
        while (!data.empty() && (data.back() == '\n' || data.back() == ' ')) {
            data.pop_back();
        }
        return data;
    }

    std::unique_ptr<httplib::Client> client_;
    std::string endpoint_;
    std::string last_event_;
    bool pulled_{false};
};

} // namespace

McpHost::McpHost(ToolRegistry& registry) : registry_(registry) {}

void McpHost::attach_stdio(std::unique_ptr<JsonRpcTransport> transport,
                           const std::string& prefix) {
    auto* raw = transport.release();
    auto client = std::make_shared<JsonRpcClient>(*raw);
    clients_.push_back(client);
    auto payload = client->call("tools/list");
    if (payload.contains("tools") && payload["tools"].is_array()) {
        for (const auto& t : payload["tools"]) {
            McpToolSpec spec;
            spec.name = t.value("name", "");
            spec.description = t.value("description", "");
            spec.schema = t.value("inputSchema", nlohmann::json::object());
            register_remote_tool(spec, prefix, client);
        }
    }
}

void McpHost::attach_sse_url(const std::string& url, const std::string& prefix) {
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        throw std::runtime_error("invalid sse url: " + url);
    }
    auto rest = url.substr(scheme_end + 3);
    auto path_pos = rest.find('/');
    std::string host_port;
    std::string path;
    if (path_pos == std::string::npos) {
        host_port = rest;
        path = "/";
    } else {
        host_port = rest.substr(0, path_pos);
        path = rest.substr(path_pos);
    }
    auto base = url.substr(0, scheme_end + 3) + host_port;
    auto client_ptr = std::make_unique<httplib::Client>(base);
    client_ptr->set_connection_timeout(10, 0);
    auto transport = std::make_unique<SseTransport>(std::move(client_ptr), path);
    auto* raw = transport.release();
    auto client_obj = std::make_shared<JsonRpcClient>(*raw);
    clients_.push_back(client_obj);
    auto payload = client_obj->call("tools/list");
    if (payload.contains("tools") && payload["tools"].is_array()) {
        for (const auto& t : payload["tools"]) {
            McpToolSpec spec;
            spec.name = t.value("name", "");
            spec.description = t.value("description", "");
            spec.schema = t.value("inputSchema", nlohmann::json::object());
            register_remote_tool(spec, prefix, client_obj);
        }
    }
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
                                   std::shared_ptr<JsonRpcClient> client) {
    if (spec.name.empty()) {
        return;
    }
    tools_.push_back(spec);
    std::string full_name = prefix.empty() ? spec.name : prefix + "__" + spec.name;
    registry_.register_tool(std::make_unique<McpTool>(
        full_name, spec.description, spec.schema, client, prefix));
}

} // namespace swiftagent
