#pragma once

#include <chrono>
#include <cstdint>

#include "llm/provider.hpp"

namespace swiftagent {

struct RetryPolicy {
    std::uint32_t max_attempts{3};
    std::chrono::milliseconds initial_backoff{200};
    double backoff_multiplier{2.0};
    std::chrono::milliseconds max_backoff{std::chrono::seconds(5)};
};

// Wraps any Provider with finite retries and exponential backoff. The
// wrapper records each attempt via a callback so callers can surface
// retries through telemetry / replay.
class RetryingProvider final : public Provider {
public:
    RetryingProvider(std::shared_ptr<Provider> inner, RetryPolicy policy = {});

    [[nodiscard]] Result<ModelResponse> complete(const Messages& context) override;
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] std::uint32_t total_attempts() const noexcept { return attempts_; }
    [[nodiscard]] std::uint32_t total_retries() const noexcept { return retries_; }

private:
    std::shared_ptr<Provider> inner_;
    RetryPolicy policy_;
    std::uint32_t attempts_{0};
    std::uint32_t retries_{0};
};

} // namespace swiftagent
