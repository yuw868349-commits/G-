#pragma once

#include <cstddef>
#include <map>
#include <memory>

#include "core/model_cascade.hpp"
#include "llm/provider.hpp"

namespace swiftagent {

// Maps a `Tier` (small / large) to a concrete `Provider`. The router is
// mutable so callers can hot-swap underlying models at runtime (e.g. when
// the cascade pins a tier to a fallback).
class ProviderRouter {
public:
    void set(Tier tier, std::shared_ptr<Provider> provider);
    [[nodiscard]] Provider& for_tier(Tier tier) const;
    [[nodiscard]] bool has_tier(Tier tier) const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::map<Tier, std::shared_ptr<Provider>> providers_;
};

} // namespace swiftagent
