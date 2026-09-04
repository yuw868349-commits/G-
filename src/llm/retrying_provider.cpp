#include "llm/retrying_provider.hpp"

#include <thread>
#include <utility>

namespace praxis {

RetryingProvider::RetryingProvider(std::shared_ptr<Provider> inner,
                                   RetryPolicy policy)
    : inner_(std::move(inner)), policy_(policy) {}

Result<ModelResponse>
RetryingProvider::complete(const Messages& context) {
    if (!inner_) {
        return Result<ModelResponse>::fail(
            Error{ErrorKind::Internal, "retrying provider has no inner"});
    }
    auto backoff = policy_.initial_backoff;
    Result<ModelResponse> last{};
    for (std::uint32_t attempt = 0; attempt < policy_.max_attempts; ++attempt) {
        ++attempts_;
        last = inner_->complete(context);
        if (last.ok()) {
            return last;
        }
        // Validation / cancellation errors should not be retried.
        if (last.error().kind == ErrorKind::Validation ||
            last.error().kind == ErrorKind::Cancelled) {
            return last;
        }
        if (attempt + 1 >= policy_.max_attempts) {
            break;
        }
        ++retries_;
        std::this_thread::sleep_for(backoff);
        auto next_ms = std::chrono::milliseconds(
            static_cast<long long>(static_cast<double>(backoff.count()) *
                                   policy_.backoff_multiplier));
        backoff = std::min(next_ms, policy_.max_backoff);
    }
    return last;
}

std::string RetryingProvider::name() const {
    return inner_ ? "retry(" + inner_->name() + ")" : "retry()";
}

} // namespace praxis
