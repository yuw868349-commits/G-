#include "llm/provider_router.hpp"

#include <stdexcept>
#include <utility>

#include "core/model_cascade.hpp"

namespace swiftagent {

void ProviderRouter::set(Tier tier, std::shared_ptr<Provider> provider) {
    if (!provider) {
        throw std::invalid_argument("provider must be non-null");
    }
    providers_[tier] = std::move(provider);
}

Provider& ProviderRouter::for_tier(Tier tier) const {
    auto it = providers_.find(tier);
    if (it == providers_.end() || !it->second) {
        throw std::runtime_error("no provider registered for tier");
    }
    return *it->second;
}

bool ProviderRouter::has_tier(Tier tier) const noexcept {
    auto it = providers_.find(tier);
    return it != providers_.end() && static_cast<bool>(it->second);
}

std::size_t ProviderRouter::size() const noexcept {
    std::size_t n = 0;
    for (const auto& [_, p] : providers_) {
        if (p) {
            ++n;
        }
    }
    return n;
}

} // namespace swiftagent
