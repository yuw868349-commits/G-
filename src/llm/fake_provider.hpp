#pragma once

#include <cstdint>
#include <deque>
#include <nlohmann/json.hpp>
#include <vector>
#include "llm/provider.hpp"

namespace praxis {

class FakeProvider final : public Provider {
public:
    void script(std::vector<nlohmann::json> steps);
    [[nodiscard]] Result<ModelResponse> complete(const Messages& context) override;
    [[nodiscard]] std::string name() const override { return "fake"; }

    std::uint64_t call_count{0};
    std::vector<std::size_t> context_sizes;

private:
    std::deque<nlohmann::json> steps_;
};

} // namespace praxis
