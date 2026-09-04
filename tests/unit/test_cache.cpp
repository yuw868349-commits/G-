#include <catch2/catch_test_macros.hpp>
#include "core/cache.hpp"

using namespace swiftagent;

TEST_CASE("cache stores and returns values") {
    Cache cache;
    cache.put("k1", nlohmann::json{{"v", 1}}, {"file:read:a"});
    auto hit = cache.get("k1", {"file:read:a"});
    REQUIRE(hit.has_value());
    CHECK((*hit)["v"] == 1);
    CHECK(cache.hits() == 1);
}

TEST_CASE("cache invalidates when dependencies change") {
    Cache cache;
    cache.put("k1", nlohmann::json{{"v", 1}}, {"file:read:a"});
    cache.invalidate_dependency("file:read:a");
    auto miss = cache.get("k1", {"file:read:a"});
    CHECK_FALSE(miss.has_value());
    CHECK(cache.misses() == 1);
}

TEST_CASE("cache refuses to return non-cacheable entries") {
    Cache cache;
    cache.put("k1", nlohmann::json{{"v", 1}}, {}, false);
    auto miss = cache.get("k1", {});
    CHECK_FALSE(miss.has_value());
}
