#include "mcp/mcp_host.hpp"

#include <httplib.h>
#include <condition_variable>
#include <memory>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace praxis {

namespace {

// Perform the MCP handshake: send `initialize` and the
// `notifications/initialized` notification. Throws on failure.
void perform_mcp_handshake(JsonRpcClient& client) {
    nlohmann::json init_params = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", nlohmann::json::object()},
        {"clientInfo", {{"name", "praxis"}, {"version", "0.1.0"}}},
    };
    auto init_result = client.call("initialize", init_params);
    (void)init_result;  // server returns serverInfo + capabilities
    client.notify("notifications/initialized", nlohmann::json::object());
}

struct ParsedUrl {
    std::string base;
    std::string host;
    int port{0};
    bool tls{false};
    std::string path;
};

// Split an http(s) URL into (scheme, host, port, path). Defaults to
// port 80 / 443 when not specified.
ParsedUrl parse_url(const std::string& url) {
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        throw std::runtime_error("invalid url (no scheme): " + url);
    }
    ParsedUrl out;
    std::string scheme = url.substr(0, scheme_end);
    out.tls = (scheme == "https");
    std::string rest = url.substr(scheme_end + 3);

    auto path_pos = rest.find('/');
    std::string host_port;
    if (path_pos == std::string::npos) {
        host_port = rest;
        out.path = "/";
    } else {
        host_port = rest.substr(0, path_pos);
        out.path = rest.substr(path_pos);
        if (out.path.empty()) {
            out.path = "/";
        }
    }
    out.base = url.substr(0, scheme_end + 3) + host_port;

    auto colon = host_port.find(':');
    if (colon == std::string::npos) {
        out.host = host_port;
        out.port = out.tls ? 443 : 80;
    } else {
        out.host = host_port.substr(0, colon);
        out.port = std::stoi(host_port.substr(colon + 1));
    }
    return out;
}

} // namespace

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
    // Strip the namespacing prefix (e.g. "file__" in "file__list_dir") to
    // recover the original MCP tool name.
    std::string remote_name = call.name;
    if (!prefix_.empty()) {
        const std::string sep = prefix_ + "__";
        if (remote_name.rfind(sep, 0) == 0) {
            remote_name = remote_name.substr(sep.size());
        }
    }
    nlohmann::json call_params = {
        {"name", remote_name},
        {"arguments", params},
    };
    try {
        auto result = client_->call("tools/call", call_params);
        // MCP responses include {content: [{type: "text", text: "..."}], isError, structuredContent}
        if (result.is_object() && result.value("isError", false)) {
            std::string msg = "remote tool error";
            if (result.contains("content") && result["content"].is_array() &&
                !result["content"].empty()) {
                const auto& first = result["content"][0];
                if (first.is_object() && first.contains("text")) {
                    msg = first["text"].get<std::string>();
                }
            }
            return ToolResult{false, nullptr, msg, {}};
        }
        if (result.is_object() && result.contains("structuredContent")) {
            return ToolResult{true, result["structuredContent"], "", {}};
        }
        if (result.is_object() && result.contains("content") &&
            result["content"].is_array() && !result["content"].empty()) {
            const auto& first = result["content"][0];
            if (first.is_object() && first.contains("text")) {
                return ToolResult{true, first["text"], "", {}};
            }
        }
        return ToolResult{true, result, "", {}};
    } catch (const std::exception& e) {
        return ToolResult{false, nullptr, e.what(), {}};
    }
}

namespace {

// SSE transport implementing the real MCP-over-HTTP+SSE protocol.
//
// MCP-over-HTTP+SSE uses one HTTP POST per client-to-server request and
// a long-lived GET for server-to-client messages.  We open the GET
// stream in a background thread that pumps parsed SSE frames into a
// thread-safe queue, which `receive()` then drains.
class SseTransport final : public JsonRpcTransport {
public:
    SseTransport(std::unique_ptr<httplib::Client> cli,
                 std::string post_endpoint,
                 std::string get_endpoint,
                 std::string session_id)
        : client_(std::move(cli)),
          post_endpoint_(std::move(post_endpoint)),
          get_endpoint_(std::move(get_endpoint)),
          session_id_(std::move(session_id)) {
        start_event_thread();
    }

    ~SseTransport() override {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            closing_ = true;
        }
        cv_.notify_all();
        if (event_thread_.joinable()) {
            event_thread_.join();
        }
        try {
            if (client_ && !session_id_.empty()) {
                client_->Delete((post_endpoint_ + "/" + session_id_).c_str());
            }
        } catch (...) {
            // no-op
        }
    }

    void send(const std::string& payload) override {
        if (!client_) {
            throw std::runtime_error("sse transport: no client");
        }
        std::string url = post_endpoint_;
        if (!session_id_.empty()) {
            url += "/" + session_id_;
        }
        auto res = client_->Post(url.c_str(), payload, "application/json");
        if (!res) {
            throw std::runtime_error("sse transport: post failed");
        }
        if (res->status < 200 || res->status >= 300) {
            throw std::runtime_error("sse transport: post status " +
                                     std::to_string(res->status));
        }
        if (session_id_.empty()) {
            auto it = res->headers.find("Mcp-Session-Id");
            if (it == res->headers.end()) {
                it = res->headers.find("mcp-session-id");
            }
            if (it != res->headers.end()) {
                session_id_ = it->second;
            }
        }
    }

    std::string receive() override {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] {
            return !queue_.empty() || closing_ || !stream_alive_;
        });
        if (queue_.empty()) {
            return "";
        }
        std::string out = std::move(queue_.front());
        queue_.pop();
        return out;
    }

    bool alive() const override {
        return client_ != nullptr;
    }

private:
    struct SseFrame {
        std::string data;
    };

    static SseFrame parse_frame(const std::string& raw) {
        SseFrame out;
        std::istringstream iss(raw);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.rfind("data:", 0) == 0) {
                std::string payload = line.substr(5);
                if (!payload.empty() && payload.front() == ' ') {
                    payload.erase(0, 1);
                }
                if (!out.data.empty()) {
                    out.data += "\n";
                }
                out.data += payload;
            }
        }
        return out;
    }

    void push_frame(SseFrame frame) {
        if (frame.data.empty()) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(frame.data));
        }
        cv_.notify_one();
    }

    void push_eof() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stream_alive_ = false;
        }
        cv_.notify_all();
    }

    void start_event_thread() {
        std::string url = get_endpoint_;
        if (!session_id_.empty()) {
            url += "/" + session_id_;
        }
        auto client = client_.get();  // raw pointer; the transport owns the client
        event_thread_ = std::thread([this, client, url]() {
            std::string buffer;
            auto on_chunk = [this, &buffer](const char* data, std::size_t len) {
                buffer.append(data, len);
                for (;;) {
                    auto pos = buffer.find("\n\n");
                    if (pos == std::string::npos) {
                        break;
                    }
                    std::string frame = buffer.substr(0, pos);
                    buffer.erase(0, pos + 2);
                    push_frame(parse_frame(frame));
                }
                return true;
            };
            auto res = client->Get(url.c_str(), on_chunk);
            (void)res;
            push_eof();
        });
        stream_alive_ = true;
    }

    std::unique_ptr<httplib::Client> client_;
    std::string post_endpoint_;
    std::string get_endpoint_;
    std::string session_id_;
    std::thread event_thread_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<std::string> queue_;
    bool closing_{false};
    bool stream_alive_{false};
};

} // namespace

McpHost::McpHost(ToolRegistry& registry) : registry_(registry) {}

void McpHost::attach(std::shared_ptr<JsonRpcTransport> transport,
                     const std::string& prefix) {
    if (!transport) {
        throw std::runtime_error("attach: transport is null");
    }
    auto client = std::make_shared<JsonRpcClient>(transport);
    clients_.push_back(client);
    perform_mcp_handshake(*client);
    auto payload = client->call("tools/list");
    register_tools_from_payload(payload, prefix, client);
}

void McpHost::attach_stdio(std::unique_ptr<JsonRpcTransport> transport,
                           const std::string& prefix) {
    auto shared = std::shared_ptr<JsonRpcTransport>(std::move(transport));
    attach(shared, prefix);
}

void McpHost::attach_sse_url(const std::string& url, const std::string& prefix) {
    auto parsed = parse_url(url);
    auto client = std::make_unique<httplib::Client>(parsed.host, parsed.port);
    client->set_connection_timeout(10, 0);
    client->set_read_timeout(0, 0);  // long-lived GET
    client->set_follow_location(true);
    auto transport = std::make_unique<SseTransport>(
        std::move(client), parsed.path, parsed.path, "");
    attach(std::shared_ptr<JsonRpcTransport>(std::move(transport)), prefix);
}

void McpHost::register_tools_from_payload(
    const nlohmann::json& payload,
    const std::string& prefix,
    std::shared_ptr<JsonRpcClient> client) {
    if (!payload.contains("tools") || !payload["tools"].is_array()) {
        return;
    }
    for (const auto& t : payload["tools"]) {
        McpToolSpec spec;
        spec.name = t.value("name", "");
        spec.description = t.value("description", "");
        spec.schema = t.value("inputSchema", nlohmann::json::object());
        register_remote_tool(spec, prefix, client);
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
    std::string full_name = prefix.empty() ? spec.name : prefix + "__" + spec.name;
    // Record the namespaced full name so `tools()` reports the same
    // identifier callers would use to look the tool up via the
    // registry. Without this, callers would see "read_file" while
    // `registry.find("file__read_file")` is the actual key.
    McpToolSpec namespaced = spec;
    namespaced.name = full_name;
    tools_.push_back(std::move(namespaced));
    registry_.register_tool(std::make_unique<McpTool>(
        full_name, spec.description, spec.schema, client, prefix));
}

} // namespace praxis
