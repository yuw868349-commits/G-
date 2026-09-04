#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <iostream>
#include "core/error.hpp"
#include "llm/fake_provider.hpp"
#include "llm/openai_provider.hpp"
#include <httplib.h>

using namespace swiftagent;

TEST_CASE("fake provider replays scripted responses") {
    FakeProvider provider;
    provider.script({
        {{"plan", "reorganize files"}, {"tool_calls", nlohmann::json::array()}}
    });

    auto response = provider.complete(Messages{});
    REQUIRE(response.ok());
    CHECK(response.value().outcome.plan == "reorganize files");
}

TEST_CASE("fake provider returns error when script is empty") {
    FakeProvider provider;
    auto response = provider.complete(Messages{});
    REQUIRE_FALSE(response.ok());
    CHECK(response.error().kind == ErrorKind::ProviderFailure);
}

TEST_CASE("openai provider parses chat completion response") {
    httplib::Server server;
    server.Post("/v1/chat/completions", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            R"({"choices":[{"message":{"role":"assistant","content":"serve","tool_calls":[]}}],"usage":{"prompt_tokens":10,"completion_tokens":2}})",
            "application/json");
    });
    int port = server.bind_to_any_port("127.0.0.1");
    REQUIRE(port > 0);
    std::thread t([&] { server.listen_after_bind(); });
    for (int i = 0; i < 50 && !server.is_running(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(server.is_running());

    OpenAIProvider provider{"http://127.0.0.1:" + std::to_string(port), "test-key"};
    auto response = provider.complete({Message{"user", "hello"}});
    REQUIRE(response.ok());
    CHECK(response.value().outcome.plan == "serve");

    server.stop();
    t.join();
}
