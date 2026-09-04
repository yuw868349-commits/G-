#include "llm/fake_provider.hpp"

#include "core/error.hpp"

namespace praxis {

namespace {
// The decision-tier review prompt is constructed by the
// orchestrator (see orchestrator.cpp).  When the FakeProvider
// detects a "Decision review:" user message it returns a CONFIRM
// so the cascade wiring is exercised end-to-end without requiring
// callers to script a separate decision response.
bool is_decision_review(const Messages& context) {
    if (context.empty()) return false;
    const auto& last = context.back();
    if (last.role != "user") return false;
    return last.content.rfind("Decision review:", 0) == 0;
}
} // namespace

void FakeProvider::script(std::vector<nlohmann::json> steps) {
    steps_ = std::deque<nlohmann::json>(steps.begin(), steps.end());
}

Result<ModelResponse> FakeProvider::complete(const Messages& context) {
    ++call_count;
    context_sizes.push_back(context.size());
    if (is_decision_review(context)) {
        // The decision tier always CONFIRMs the chore plan in tests
        // unless the script also queued an explicit override.  This
        // keeps the cascade observable in every fake run.
        ModelResponse response;
        response.outcome.plan = "CONFIRM";
        response.outcome.has_tool_use = false;
        response.outcome.tool_count = 0;
        response.raw = {{"plan", "CONFIRM"}, {"tool_calls", nlohmann::json::array()}};
        return Result<ModelResponse>::ok(std::move(response));
    }
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

} // namespace praxis
