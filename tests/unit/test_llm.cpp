#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <iostream>
#include "core/error.hpp"
#include "llm/fake_provider.hpp"
#include "llm/openai_provider.hpp"
#include "llm/retrying_provider.hpp"
#include <httplib.h>

using namespace praxis;

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

namespace {

// A provider that fails the first N calls before succeeding.
class FlakyProvider final : public Provider {
public:
    explicit FlakyProvider(std::uint32_t failures_before_success)
        : failures_(failures_before_success) {}
    [[nodiscard]] Result<ModelResponse> complete(const Messages&) override {
        ++calls_;
        if (calls_ <= failures_) {
            return Result<ModelResponse>::fail(
                Error{ErrorKind::ProviderFailure, "flaky failure " +
                std::to_string(calls_)});
        }
        ModelResponse r;
        r.outcome.plan = "finally";
        return Result<ModelResponse>::ok(std::move(r));
    }
    [[nodiscard]] std::string name() const override { return "flaky"; }
    std::uint32_t calls_{0};
private:
    std::uint32_t failures_{0};
};

} // namespace

TEST_CASE("retrying provider retries transient failures and eventually succeeds") {
    FlakyProvider inner(2);  // fail twice, succeed on the 3rd
    RetryPolicy policy;
    policy.max_attempts = 4;
    policy.initial_backoff = std::chrono::milliseconds(1);
    policy.max_backoff = std::chrono::milliseconds(2);
    RetryingProvider retrying(std::make_shared<FlakyProvider>(inner), policy);

    auto response = retrying.complete(Messages{});
    REQUIRE(response.ok());
    CHECK(response.value().outcome.plan == "finally");
    CHECK(retrying.total_attempts() == 3);
    CHECK(retrying.total_retries() == 2);
}

TEST_CASE("retrying provider gives up after max_attempts") {
    FlakyProvider inner(10);  // never recovers within the budget
    RetryPolicy policy;
    policy.max_attempts = 3;
    policy.initial_backoff = std::chrono::milliseconds(1);
    policy.max_backoff = std::chrono::milliseconds(2);
    auto inner_ptr = std::make_shared<FlakyProvider>(inner);
    RetryingProvider retrying(inner_ptr, policy);

    auto response = retrying.complete(Messages{});
    REQUIRE_FALSE(response.ok());
    CHECK(response.error().kind == ErrorKind::ProviderFailure);
    CHECK(retrying.total_attempts() == 3);
    CHECK(retrying.total_retries() == 2);
    CHECK(inner_ptr->calls_ == 3);
}

TEST_CASE("openai provider normalizes trailing /v1 in base url") {
    // The canonical OpenAI base URL is "https://api.openai.com/v1".
    // The provider must strip the trailing "/v1" so that the default
    // path ("/chat/completions") is appended to the host and not to
    // the versioned prefix.
    OpenAIProvider p1{"https://api.openai.com/v1", "k", "gpt-4o-mini"};
    CHECK(p1.base_url() == "https://api.openai.com");
    CHECK(p1.path() == "/chat/completions");

    OpenAIProvider p2{"https://api.openai.com/v1/", "k", "gpt-4o-mini"};
    CHECK(p2.base_url() == "https://api.openai.com");

    OpenAIProvider p3{"https://api.openai.com", "k", "gpt-4o-mini"};
    CHECK(p3.base_url() == "https://api.openai.com");

    // Callers that pass a custom path keep it verbatim and don't get
    // a second "/v1" added behind their back.
    OpenAIProvider p4{"https://api.openai.com/v1", "k", "gpt-4o-mini",
                      "/v1/chat/completions"};
    CHECK(p4.base_url() == "https://api.openai.com");
    CHECK(p4.path() == "/v1/chat/completions");
}

TEST_CASE("openai provider parses chat completion response") {
    httplib::Server server;
    server.Post("/chat/completions", [](const httplib::Request&, httplib::Response& res) {
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
