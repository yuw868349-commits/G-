#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "core/error.hpp"
#include "llm/fake_provider.hpp"

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
