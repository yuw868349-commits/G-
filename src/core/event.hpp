#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

namespace praxis {

enum class EventKind : std::uint8_t {
    TurnStarted,
    ModelRequested,
    ModelResponded,
    ToolCalled,
    ToolFinished,
    WorkingSetRendered,
    FactRecalled,
    CacheHit,
    CacheInvalidated,
    ProgressScored,
    StrategySwitched,
    Degraded,
    BudgetHit,
    TaskEnded
};

struct Event {
    EventKind kind{EventKind::TurnStarted};
    std::uint64_t sequence{0};
    nlohmann::json payload = nlohmann::json::object();
};

class EventSink {
public:
    virtual ~EventSink() = default;
    virtual void on_event(const Event& event) = 0;
};

} // namespace praxis
