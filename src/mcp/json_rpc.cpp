#include "mcp/json_rpc.hpp"

#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace praxis {

namespace {

std::string serialize(const nlohmann::json& json) {
    return json.dump();
}

nlohmann::json parse(const std::string& data) {
    return nlohmann::json::parse(data);
}

} // namespace

JsonRpcClient::JsonRpcClient(JsonRpcTransport& transport)
    : transport_(transport) {}

JsonRpcClient::JsonRpcClient(std::shared_ptr<JsonRpcTransport> transport)
    : owned_(std::move(transport)), transport_(*owned_) {}

nlohmann::json JsonRpcClient::call(const std::string& method, const nlohmann::json& params) {
    auto id = next_id_++;
    nlohmann::json req;
    req["jsonrpc"] = "2.0";
    req["id"] = id;
    req["method"] = method;
    req["params"] = params;
    transport_.send(serialize(req));
    auto raw = transport_.receive();
    if (raw.empty()) {
        throw std::runtime_error("transport closed during call");
    }
    auto resp = parse(raw);
    if (resp.contains("error") && !resp["error"].is_null()) {
        throw std::runtime_error(resp["error"].dump());
    }
    return resp.value("result", nlohmann::json::object());
}

void JsonRpcClient::notify(const std::string& method, const nlohmann::json& params) {
    nlohmann::json req;
    req["jsonrpc"] = "2.0";
    req["method"] = method;
    req["params"] = params;
    transport_.send(serialize(req));
}

void JsonRpcClient::register_handler(const std::string& method, Handler handler) {
    handlers_[method] = std::move(handler);
}

void JsonRpcClient::pump_once() {
    auto raw = transport_.receive();
    if (raw.empty()) {
        return;
    }
    auto req = parse(raw);
    auto it = handlers_.find(req.value("method", ""));
    if (it == handlers_.end()) {
        return;
    }
    auto result = it->second(req.value("params", nlohmann::json::object()));
    if (req.contains("id") && !req["id"].is_null()) {
        nlohmann::json resp;
        resp["jsonrpc"] = "2.0";
        resp["id"] = req["id"];
        resp["result"] = result;
        transport_.send(serialize(resp));
    }
}

void JsonRpcClient::close() {
    // No-op for now; transports may override to flush.
}

} // namespace praxis
