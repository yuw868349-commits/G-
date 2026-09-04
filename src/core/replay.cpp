#include "core/replay.hpp"

#include <utility>

namespace swiftagent {

void Replay::record(const Event& event) {
    std::lock_guard<std::mutex> lock(mtx_);
    Event e = event;
    if (e.sequence == 0) {
        ++last_seq_;
        e.sequence = last_seq_;
    } else {
        last_seq_ = std::max(last_seq_, e.sequence);
    }
    events_.push_back(e);
    for (auto& sink : sinks_) {
        sink->on_event(e);
    }
}

void Replay::record(EventKind kind, nlohmann::json payload) {
    Event ev;
    ev.kind = kind;
    ev.payload = std::move(payload);
    record(ev);
}

void Replay::link(std::uint64_t from, std::uint64_t to) {
    std::lock_guard<std::mutex> lock(mtx_);
    edges_.push_back(CausalEdge{from, to});
}

void Replay::subscribe(std::shared_ptr<EventSink> sink) {
    std::lock_guard<std::mutex> lock(mtx_);
    sinks_.push_back(std::move(sink));
}

std::vector<Event> Replay::events() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return events_;
}

std::vector<CausalEdge> Replay::edges() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return edges_;
}

std::size_t Replay::size() const noexcept {
    return events_.size();
}

void Replay::clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    events_.clear();
    edges_.clear();
    sinks_.clear();
    last_seq_ = 0;
}

void Replay::replay_into(EventSink& sink) const {
    std::lock_guard<std::mutex> lock(mtx_);
    for (const auto& e : events_) {
        sink.on_event(e);
    }
}

} // namespace swiftagent
