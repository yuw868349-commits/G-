#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "core/types.hpp"

namespace swiftagent {

struct ResourceClaim {
    std::string resource;
    std::size_t call_index{0};
};

class DependencyGraph {
public:
    void add_call(const ToolCall& call, const std::vector<std::string>& resources);
    [[nodiscard]] bool has_conflict(std::size_t a, std::size_t b) const;
    [[nodiscard]] std::vector<std::vector<std::size_t>> weakly_connected_components() const;
    [[nodiscard]] std::vector<std::vector<std::size_t>> schedule() const;
    [[nodiscard]] std::size_t size() const noexcept { return calls_.size(); }
    [[nodiscard]] const ToolCall& call_at(std::size_t index) const { return calls_[index]; }
    [[nodiscard]] const std::vector<std::string>& resources_at(std::size_t index) const {
        return resources_[index];
    }

private:
    std::vector<ToolCall> calls_;
    std::vector<std::vector<std::string>> resources_;
    std::unordered_map<std::string, std::vector<std::size_t>> resource_owners_;
};

} // namespace swiftagent
