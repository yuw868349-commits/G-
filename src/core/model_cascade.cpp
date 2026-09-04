#include "core/model_cascade.hpp"

#include <stdexcept>

namespace praxis {

Tier ModelCascade::route_for(Role role) const {
    if (role == Role::Decision) {
        return Tier::Large;
    }
    if (chore_pinned_) {
        return Tier::Large;
    }
    auto it = tiers_.find(Tier::Small);
    if (it == tiers_.end()) {
        return Tier::Small;
    }
    if (it->second.attempts > 0 &&
        divergence_rate(Tier::Small) > missing_upgrade_threshold_) {
        return Tier::Large;
    }
    return Tier::Small;
}

void ModelCascade::record_outcome(Tier tier, Role, bool diverged) {
    auto& state = tiers_[tier];
    ++state.attempts;
    if (diverged) {
        ++state.divergences;
    }
}

void ModelCascade::record_failure(Tier tier, Role) {
    auto& state = tiers_[tier];
    ++state.failures;
    if (tier == Tier::Small && state.failures >= max_failures_before_pin_) {
        chore_pinned_ = true;
    }
}

void ModelCascade::set_max_failures_before_pin(std::uint32_t max_failures) {
    max_failures_before_pin_ = max_failures;
}

void ModelCascade::set_missing_upgrade_threshold(double threshold) {
    if (threshold < 0.0 || threshold > 1.0) {
        throw std::out_of_range("threshold must be in [0, 1]");
    }
    missing_upgrade_threshold_ = threshold;
}

void ModelCascade::clear_pin() noexcept {
    chore_pinned_ = false;
    tiers_.clear();
}

double ModelCascade::divergence_rate(Tier tier) const {
    auto it = tiers_.find(tier);
    if (it == tiers_.end() || it->second.attempts == 0) {
        return 0.0;
    }
    return static_cast<double>(it->second.divergences) /
           static_cast<double>(it->second.attempts);
}

bool ModelCascade::is_pinned(Tier tier) const {
    if (tier == Tier::Small) {
        return chore_pinned_;
    }
    // The large tier is only "pinned" once we have actual evidence that
    // small-tier escalations are too frequent. Until then, callers may
    // still use the small model for non-decision roles.
    if (!chore_pinned_) {
        return false;
    }
    auto it = tiers_.find(Tier::Small);
    if (it == tiers_.end() || it->second.attempts == 0) {
        return false;
    }
    return divergence_rate(Tier::Small) > missing_upgrade_threshold_;
}

} // namespace praxis
