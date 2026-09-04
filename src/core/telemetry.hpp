#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace swiftagent {

class Telemetry {
public:
    struct ModuleMetrics {
        std::string name;
        std::uint64_t invocations{0};
        double total_ms{0.0};
        double last_ms{0.0};
    };

    void record_module(const std::string& name, double ms);
    void record_token(std::uint32_t prompt, std::uint32_t completion);
    void record_cache_hit(bool hit);
    void record_tool_call(bool success);
    void set_baseline_ms(double ms);
    void set_wall_clock_ms(double ms);

    [[nodiscard]] double speedup_x() const noexcept;
    [[nodiscard]] std::size_t prompt_tokens() const noexcept { return prompt_tokens_; }
    [[nodiscard]] std::size_t completion_tokens() const noexcept { return completion_tokens_; }
    [[nodiscard]] std::size_t cache_hits() const noexcept { return cache_hits_; }
    [[nodiscard]] std::size_t cache_misses() const noexcept { return cache_misses_; }
    [[nodiscard]] double total_module_ms() const noexcept;

    [[nodiscard]] nlohmann::json report() const;

private:
    std::vector<ModuleMetrics> modules_;
    std::atomic<std::uint64_t> prompt_tokens_{0};
    std::atomic<std::uint64_t> completion_tokens_{0};
    std::atomic<std::uint64_t> cache_hits_{0};
    std::atomic<std::uint64_t> cache_misses_{0};
    std::atomic<std::uint64_t> tool_success_{0};
    std::atomic<std::uint64_t> tool_failures_{0};
    double baseline_ms_{0.0};
    double wall_ms_{0.0};
};

class ScopedTimer {
public:
    explicit ScopedTimer(Telemetry& t, std::string module)
        : t_(t), module_(std::move(module)),
          start_(std::chrono::steady_clock::now()) {}
    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start_).count();
        t_.record_module(module_, ms);
    }

private:
    Telemetry& t_;
    std::string module_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace swiftagent
