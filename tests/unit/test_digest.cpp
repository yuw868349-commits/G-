#include <catch2/catch_test_macros.hpp>
#include "core/digest.hpp"

using namespace praxis;

TEST_CASE("normalizer extracts hard keys from structured text") {
    Normalizer n;
    auto keys = n.hard_keys(R"({"port":8080,"path":"/tmp/x"})");
    REQUIRE(n.contains_key(keys, "port"));
    REQUIRE(n.value_of(keys, "port") == "8080");
    REQUIRE(n.value_of(keys, "path") == "/tmp/x");
}

TEST_CASE("digest is decisive for identical hard keys") {
    Digest a = Digest::build("tool_result", R"({"port":8080})");
    Digest b = Digest::build("tool_result", R"({"port":8080})");
    CHECK(a.hard_fingerprint() == b.hard_fingerprint());
    CHECK(a.matches_hard(b));
}

TEST_CASE("digest differs when hard key differs") {
    Digest a = Digest::build("tool_result", R"({"port":8080})");
    Digest c = Digest::build("tool_result", R"({"port":8081})");
    CHECK_FALSE(a.matches_hard(c));
}

TEST_CASE("soft tier labels confidence") {
    SoftFingerprint s{};
    s.embedding = {0.1, 0.2};
    s.confidence = 0.95;
    CHECK(s.confidence > 0.8);
    CHECK(s.is_high_confidence(0.8));
}

TEST_CASE("output token bound estimation") {
    CHECK(estimate_tokens("hello world") == 2);
    CHECK(estimate_tokens("中文中文") == 4);
}

TEST_CASE("digest to json roundtrips") {
    Digest a = Digest::build("tool_result", R"({"port":8080})");
    auto json = a.to_json();
    auto b = Digest::from_json(json);
    CHECK(b.hard_fingerprint() == a.hard_fingerprint());
}
