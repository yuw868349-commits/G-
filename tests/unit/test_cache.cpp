#include <catch2/catch_test_macros.hpp>
#include "core/cache.hpp"

using namespace praxis;

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

TEST_CASE("cache tracks per-key hit counts without mutating the entry") {
    Cache cache;
    cache.put("k1", nlohmann::json{{"v", 1}}, {"a"});
    cache.put("k2", nlohmann::json{{"v", 2}}, {"b"});
    cache.get("k1", {"a"});
    cache.get("k1", {"a"});
    cache.get("k2", {"b"});
    CHECK(cache.hits_for("k1") == 2);
    CHECK(cache.hits_for("k2") == 1);
    CHECK(cache.hits_for("k3") == 0);
    // The entry itself should still be const-friendly; we test that
    // get() is callable on a const reference.
    const Cache& cref = cache;
    auto v = cref.get("k1", {"a"});
    REQUIRE(v.has_value());
    CHECK((*v)["v"] == 1);
}
