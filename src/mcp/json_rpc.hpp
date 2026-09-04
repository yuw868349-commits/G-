#pragma once

#include <cstdint>
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>

namespace swiftagent {

struct JsonRpcRequest {
    std::string method;
    nlohmann::json params = nlohmann::json::object();
    std::optional<std::int64_t> id;
};

struct JsonRpcResponse {
    nlohmann::json result = nlohmann::json::object();
    std::optional<nlohmann::json> error;
    std::optional<std::int64_t> id;
    bool is_notification{false};
};

class JsonRpcTransport {
public:
    virtual ~JsonRpcTransport() = default;
    virtual void send(const std::string& payload) = 0;
    virtual std::string receive() = 0;
    virtual bool alive() const = 0;
};

class JsonRpcClient {
public:
    using Handler = std::function<nlohmann::json(const nlohmann::json& params)>;

    explicit JsonRpcClient(JsonRpcTransport& transport);
    // Owning constructor: the client takes shared ownership of the
    // transport and is responsible for keeping it alive as long as
    // pending requests might still need it.
    explicit JsonRpcClient(std::shared_ptr<JsonRpcTransport> transport);

    [[nodiscard]] nlohmann::json call(const std::string& method,
                                       const nlohmann::json& params = nlohmann::json::object());
    void notify(const std::string& method, const nlohmann::json& params = nlohmann::json::object());
    void register_handler(const std::string& method, Handler handler);
    void pump_once();
    void close();

private:
    std::shared_ptr<JsonRpcTransport> owned_;
    JsonRpcTransport& transport_;
    std::int64_t next_id_{1};
    std::unordered_map<std::string, Handler> handlers_;
};

} // namespace swiftagent
