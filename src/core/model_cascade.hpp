#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

namespace praxis {

enum class Tier { Small, Large };

enum class Role { Chore, Decision };

class ModelCascade {
public:
    [[nodiscard]] Tier route_for(Role role) const;

    void record_outcome(Tier tier, Role role, bool diverged);
    void record_failure(Tier tier, Role role);
    void set_max_failures_before_pin(std::uint32_t max_failures);
    void set_missing_upgrade_threshold(double threshold);
    void clear_pin() noexcept;

    [[nodiscard]] double divergence_rate(Tier tier) const;
    [[nodiscard]] bool is_pinned(Tier tier) const;

private:
    struct TierState {
        std::uint64_t attempts{0};
        std::uint64_t divergences{0};
        std::uint32_t failures{0};
    };

    bool chore_pinned_{false};
    std::uint32_t max_failures_before_pin_{3};
    double missing_upgrade_threshold_{0.1};
    std::map<Tier, TierState> tiers_;
};

} // namespace praxis
