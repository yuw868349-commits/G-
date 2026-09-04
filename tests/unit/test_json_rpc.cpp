#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include "mcp/json_rpc.hpp"

using namespace swiftagent;

namespace {

class StreamTransport final : public JsonRpcTransport {
public:
    void send(const std::string& payload) override {
        sent.push_back(payload);
    }
    std::string receive() override {
        if (cursor >= scripted.size()) {
            return "";
        }
        return scripted[cursor++];
    }
    bool alive() const override { return cursor < scripted.size(); }

    std::vector<std::string> sent;
    std::vector<std::string> scripted;
    std::size_t cursor{0};
};

} // namespace

TEST_CASE("json rpc client sends a request and parses the response") {
    StreamTransport transport;
    transport.scripted = {R"({"jsonrpc":"2.0","id":1,"result":{"ok":true}})"};
    JsonRpcClient client(transport);
    auto result = client.call("ping", {{"k", 1}});
    REQUIRE(transport.sent.size() == 1);
    auto sent = nlohmann::json::parse(transport.sent[0]);
    CHECK(sent["method"] == "ping");
    CHECK(sent["params"]["k"] == 1);
    CHECK(result["ok"] == true);
}

TEST_CASE("json rpc client surfaces remote errors") {
    StreamTransport transport;
    transport.scripted = {R"({"jsonrpc":"2.0","id":1,"error":{"code":-1,"message":"nope"}})"};
    JsonRpcClient client(transport);
    bool threw = false;
    try {
        (void)client.call("boom");
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

TEST_CASE("json rpc client invokes local handler and replies") {
    StreamTransport transport;
    transport.scripted = {R"({"jsonrpc":"2.0","id":7,"method":"echo","params":{"x":42}})"};
    JsonRpcClient client(transport);
    client.register_handler("echo", [](const nlohmann::json& params) {
        return params;
    });
    client.pump_once();
    REQUIRE(transport.sent.size() == 1);
    auto sent = nlohmann::json::parse(transport.sent[0]);
    CHECK(sent["id"] == 7);
    CHECK(sent["result"]["x"] == 42);
}
