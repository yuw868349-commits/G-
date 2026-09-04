#include "core/dependency_graph.hpp"

#include <algorithm>
#include <climits>
#include <queue>
#include <set>

namespace swiftagent {

void DependencyGraph::add_call(const ToolCall& call,
                               const std::vector<std::string>& resources) {
    auto index = calls_.size();
    calls_.push_back(call);
    resources_.push_back(resources);
    for (const auto& r : resources) {
        resource_owners_[r].push_back(index);
    }
}

bool DependencyGraph::has_conflict(std::size_t a, std::size_t b) const {
    if (a == b || a >= calls_.size() || b >= calls_.size()) {
        return false;
    }
    for (const auto& r : resources_[a]) {
        for (const auto& r2 : resources_[b]) {
            if (r == r2) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::vector<std::size_t>>
DependencyGraph::weakly_connected_components() const {
    std::vector<std::vector<std::size_t>> components;
    std::vector<bool> visited(calls_.size(), false);
    for (std::size_t seed = 0; seed < calls_.size(); ++seed) {
        if (visited[seed]) {
            continue;
        }
        std::vector<std::size_t> comp;
        std::queue<std::size_t> q;
        q.push(seed);
        visited[seed] = true;
        while (!q.empty()) {
            auto v = q.front();
            q.pop();
            comp.push_back(v);
            for (std::size_t i = 0; i < calls_.size(); ++i) {
                if (!visited[i] && has_conflict(v, i)) {
                    visited[i] = true;
                    q.push(i);
                }
            }
        }
        std::sort(comp.begin(), comp.end());
        components.push_back(std::move(comp));
    }
    std::sort(components.begin(), components.end());
    return components;
}

std::vector<std::vector<std::size_t>> DependencyGraph::schedule() const {
    std::vector<std::vector<std::size_t>> groups;
    if (calls_.empty()) {
        return groups;
    }
    std::vector<std::size_t> assignment(calls_.size(), SIZE_MAX);
    std::size_t next_group = 0;
    bool progress = true;
    while (progress) {
        progress = false;
        for (std::size_t i = 0; i < calls_.size(); ++i) {
            if (assignment[i] != SIZE_MAX) {
                continue;
            }
            bool placed = false;
            for (std::size_t g = 0; g < next_group; ++g) {
                bool ok = true;
                for (auto j : groups[g]) {
                    if (has_conflict(i, j)) {
                        ok = false;
                        break;
                    }
                }
                if (ok) {
                    assignment[i] = g;
                    groups[g].push_back(i);
                    placed = true;
                    break;
                }
            }
            if (!placed) {
                assignment[i] = next_group;
                groups.push_back({i});
                ++next_group;
                progress = true;
            }
        }
    }
    for (auto& g : groups) {
        std::sort(g.begin(), g.end());
    }
    return groups;
}

} // namespace swiftagent
