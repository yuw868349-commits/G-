#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include "core/fact_store.hpp"

using namespace swiftagent;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("fact store deduplicates by content address") {
    FactStore store;
    auto id1 = store.append("type", R"({"k":1})");
    auto id2 = store.append("type", R"({"k":1})");
    CHECK(id1 == id2);
    CHECK(store.size() == 1);
}

TEST_CASE("fact store retrieves verbatim content") {
    FactStore store;
    auto id = store.append("tool_result", R"({"port":8080})");
    auto fact = store.get(id);
    REQUIRE(fact.has_value());
    CHECK(fact->type == "tool_result");
    CHECK(fact->content == R"({"port":8080})");
}

TEST_CASE("fact store persists and reloads") {
    const std::string path = "tmp_fact_store.jsonl";
    std::filesystem::remove(path);
    {
        FactStore store(path);
        store.append("a", "1");
        store.append("b", "2");
    }
    FactStore reloaded(path);
    CHECK(reloaded.size() == 2);
    std::filesystem::remove(path);
}
