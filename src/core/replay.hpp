#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
#include "core/event.hpp"

namespace praxis {

struct CausalEdge {
    std::uint64_t from{0};
    std::uint64_t to{0};
};

class Replay {
public:
    void record(const Event& event);
    void record(EventKind kind, nlohmann::json payload = nlohmann::json::object());
    void link(std::uint64_t from, std::uint64_t to);
    void subscribe(std::shared_ptr<EventSink> sink);

    [[nodiscard]] std::vector<Event> events() const;
    [[nodiscard]] std::vector<CausalEdge> edges() const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::uint64_t last_sequence() const noexcept { return last_seq_; }

    void clear();
    void replay_into(EventSink& sink) const;

private:
    mutable std::mutex mtx_;
    std::vector<Event> events_;
    std::vector<CausalEdge> edges_;
    std::vector<std::shared_ptr<EventSink>> sinks_;
    std::uint64_t last_seq_{0};
};

} // namespace praxis
