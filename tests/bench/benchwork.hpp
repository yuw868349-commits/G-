#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace praxis {

struct BenchResult {
    std::string name;
    double elapsed_ms{0.0};
    std::uint64_t tool_invocations{0};
    std::uint64_t cache_hits{0};
    double baseline_ms{0.0};
    [[nodiscard]] double speedup() const noexcept {
        return baseline_ms > 0.0 ? baseline_ms / elapsed_ms : 1.0;
    }
};

class BenchHarness {
public:
    static BenchResult file_reorganization(std::size_t files, bool parallel);
    static BenchResult data_gathering(std::size_t items, bool parallel);
    static BenchResult dependency_install(std::size_t packages, bool parallel);
};

} // namespace praxis
