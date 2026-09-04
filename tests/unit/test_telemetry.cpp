#include <catch2/catch_test_macros.hpp>
#include "core/telemetry.hpp"

using namespace swiftagent;

TEST_CASE("telemetry aggregates module timings") {
    Telemetry t;
    t.record_module("cascade", 5.0);
    t.record_module("cascade", 7.0);
    CHECK(t.total_module_ms() == 12.0);
    t.record_token(10, 20);
    CHECK(t.prompt_tokens() == 10);
    CHECK(t.completion_tokens() == 20);
    t.record_cache_hit(true);
    t.record_cache_hit(false);
    CHECK(t.cache_hits() == 1);
    CHECK(t.cache_misses() == 1);
}

TEST_CASE("telemetry computes speedup from baseline") {
    Telemetry t;
    t.set_baseline_ms(200.0);
    t.set_wall_clock_ms(50.0);
    CHECK(t.speedup_x() == 4.0);
}

TEST_CASE("telemetry report contains expected fields") {
    Telemetry t;
    t.record_module("test", 1.0);
    t.set_baseline_ms(100.0);
    t.set_wall_clock_ms(50.0);
    auto rep = t.report();
    CHECK(rep["speedup_x"] == 2.0);
    CHECK(rep["modules"].is_array());
    CHECK(rep["modules"].size() == 1);
}
