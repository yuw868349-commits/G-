#include "core/context_manager.hpp"

namespace praxis {

ContextManager::ContextManager(std::size_t token_budget)
    : token_budget_(token_budget),
      store_() {
    if (token_budget > 0) {
        max_working_tokens_ = token_budget;
    }
}

std::size_t ContextManager::size_tokens(const std::string& text) const {
    return estimate_tokens(text);
}

void ContextManager::record_tool_result(const std::string& fact_id) {
    fact_order_.push_back(fact_id);
    digest_dirty_ = true;
    if (auto fact = store_.get(fact_id)) {
        auto keys = Normalizer::hard_keys(fact->content);
        for (const auto& [k, v] : keys) {
            digest_[k] = v;
        }
    }
}

void ContextManager::rebuild_digest() {
    digest_.clear();
    for (const auto& id : fact_order_) {
        if (auto fact = store_.get(id)) {
            auto keys = Normalizer::hard_keys(fact->content);
            for (const auto& [k, v] : keys) {
                digest_[k] = v;
            }
        }
    }
    digest_dirty_ = false;
}

WorkingSet ContextManager::render_working_set(const TurnContext& ctx) {
    WorkingSet ws;
    std::string render = "# Goal\n" + ctx.goal + "\n\n# Facts\n";
    for (const auto& [k, v] : digest_) {
        render += "- " + k + " = " + v + "\n";
    }
    for (const auto& id : ctx.recent_tool_results) {
        if (auto fact = store_.get(id)) {
            ws.fact_blocks.push_back(id);
            render += "\n## " + id + "\n";
            if (size_tokens(fact->content) <= max_working_tokens_) {
                render += fact->content + "\n";
            } else {
                auto cut = fact->content.substr(0, max_working_tokens_ / 2);
                render += cut + "\n...[truncated, recall by id]\n";
            }
        }
    }
    ws.render = render;
    ws.token_count = size_tokens(render);
    if (ws.token_count > max_working_tokens_) {
        render = "# Goal\n" + ctx.goal + "\n\n# Facts\n";
        for (const auto& [k, v] : digest_) {
            render += "- " + k + " = " + v + "\n";
        }
        ws.render = render;
        ws.token_count = size_tokens(render);
    }
    return ws;
}

std::optional<RecallResult> ContextManager::recall(const std::string& fact_id) {
    auto fact = store_.get(fact_id);
    if (!fact) {
        return RecallResult{RecallMode::Degraded, "", "", 0.0};
    }
    return RecallResult{RecallMode::Precise, fact->id, fact->content, 1.0};
}

} // namespace praxis
