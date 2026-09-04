#include "core/telemetry.hpp"

#include <algorithm>

namespace praxis {

void Telemetry::record_module(const std::string& name, double ms) {
    auto it = std::find_if(modules_.begin(), modules_.end(),
                           [&](const ModuleMetrics& m) { return m.name == name; });
    if (it == modules_.end()) {
        modules_.push_back(ModuleMetrics{name, 1, ms, ms});
        return;
    }
    ++it->invocations;
    it->total_ms += ms;
    it->last_ms = ms;
}

void Telemetry::record_token(std::uint32_t prompt, std::uint32_t completion) {
    prompt_tokens_ += prompt;
    completion_tokens_ += completion;
}

void Telemetry::record_cache_hit(bool hit) {
    if (hit) {
        ++cache_hits_;
    } else {
        ++cache_misses_;
    }
}

void Telemetry::record_tool_call(bool success) {
    if (success) {
        ++tool_success_;
    } else {
        ++tool_failures_;
    }
}

void Telemetry::set_baseline_ms(double ms) {
    baseline_ms_ = ms;
}

void Telemetry::set_wall_clock_ms(double ms) {
    wall_ms_ = ms;
}

double Telemetry::speedup_x() const noexcept {
    if (wall_ms_ <= 0.0) {
        return 1.0;
    }
    return baseline_ms_ / wall_ms_;
}

double Telemetry::total_module_ms() const noexcept {
    double total = 0.0;
    for (const auto& m : modules_) {
        total += m.total_ms;
    }
    return total;
}

nlohmann::json Telemetry::report() const {
    nlohmann::json j;
    j["prompt_tokens"] = static_cast<std::uint64_t>(prompt_tokens_);
    j["completion_tokens"] = static_cast<std::uint64_t>(completion_tokens_);
    j["cache_hits"] = static_cast<std::uint64_t>(cache_hits_);
    j["cache_misses"] = static_cast<std::uint64_t>(cache_misses_);
    j["tool_success"] = static_cast<std::uint64_t>(tool_success_);
    j["tool_failures"] = static_cast<std::uint64_t>(tool_failures_);
    j["baseline_ms"] = baseline_ms_;
    j["wall_clock_ms"] = wall_ms_;
    j["speedup_x"] = speedup_x();
    j["modules"] = nlohmann::json::array();
    for (const auto& m : modules_) {
        nlohmann::json mod;
        mod["name"] = m.name;
        mod["invocations"] = m.invocations;
        mod["total_ms"] = m.total_ms;
        mod["last_ms"] = m.last_ms;
        j["modules"].push_back(std::move(mod));
    }
    return j;
}

} // namespace praxis
