#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/digest.hpp"
#include "core/fact_store.hpp"

namespace swiftagent {

enum class RecallMode { Precise, Approximate, Degraded };

struct RecallResult {
    RecallMode mode{RecallMode::Degraded};
    std::string id;
    std::string content;
    double confidence{0.0};
};

struct WorkingSet {
    std::string render;
    std::vector<std::string> fact_blocks;
    std::size_t token_count{0};
};

struct TurnContext {
    std::string goal;
    std::vector<std::string> recent_tool_results;
};

class ContextManager {
public:
    explicit ContextManager(std::size_t token_budget = 8192);

    [[nodiscard]] WorkingSet render_working_set(const TurnContext& ctx);
    [[nodiscard]] std::optional<RecallResult> recall(const std::string& fact_id);
    [[nodiscard]] FactStore& store() noexcept { return store_; }
    void record_tool_result(const std::string& fact_id);
    void rebuild_digest();
    [[nodiscard]] const std::map<std::string, std::string>& current_digest() const noexcept {
        return digest_;
    }
    [[nodiscard]] std::size_t size_tokens(const std::string& text) const;

private:
    std::size_t token_budget_;
    FactStore store_;
    std::map<std::string, std::string> digest_;
    std::vector<std::string> fact_order_;
    std::size_t max_working_tokens_{0};
    bool digest_dirty_{true};
};

} // namespace swiftagent
