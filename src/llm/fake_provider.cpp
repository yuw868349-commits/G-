#include "llm/fake_provider.hpp"

#include "core/error.hpp"

namespace swiftagent {

void FakeProvider::script(std::vector<nlohmann::json> steps) {
    steps_ = std::deque<nlohmann::json>(steps.begin(), steps.end());
}

Result<ModelResponse> FakeProvider::complete(const Messages& context) {
    ++call_count;
    context_sizes.push_back(context.size());
    if (steps_.empty()) {
        return Result<ModelResponse>::fail(
            Error{ErrorKind::ProviderFailure, "fake provider script exhausted"});
    }
    auto step = std::move(steps_.front());
    steps_.pop_front();

    ModelResponse response;
    response.outcome.plan = step.value("plan", "no plan stated");
    response.outcome.has_tool_use = step.contains("tool_calls") && step["tool_calls"].is_array()
                                   && !step["tool_calls"].empty();
    auto tool_calls = step.value("tool_calls", nlohmann::json::array());
    response.outcome.tool_count = static_cast<std::uint32_t>(tool_calls.size());
    response.raw = step;
    return Result<ModelResponse>::ok(std::move(response));
}

} // namespace swiftagent
