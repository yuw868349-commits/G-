// Inspect telemetry: token counts, cache hits, per-module timings, speedup.
//
// Build: cmake --build build --target example_telemetry
// Run:   ./build/example_telemetry

#include <iostream>

#include "core/orchestrator.hpp"
#include "core/telemetry.hpp"
#include "core/types.hpp"
#include "llm/fake_provider.hpp"

int main()
{
    using namespace swiftagent;

    FakeProvider provider;
    provider.script({
        {{"plan", "first turn"}, {"completed", false}},
        {{"plan", "second turn"}, {"completed", true}},
    });

    Orchestrator orchestrator(provider);
    orchestrator.register_builtin();

    // Manually record some telemetry to demonstrate the surface.
    orchestrator.set_baseline_ms(1000.0);
    orchestrator.record_module("model", 12.5);
    orchestrator.record_module("tools", 3.4);
    orchestrator.record_token(120, 40);
    orchestrator.record_token(80, 20);
    orchestrator.record_cache_hit(true);
    orchestrator.record_cache_hit(true);
    orchestrator.record_cache_hit(false);
    orchestrator.record_tool_call(true);

    Budget budget;
    budget.active = true;
    budget.max_turns = 4;

    auto result = orchestrator.run("measure things", budget);
    if (!result.ok())
    {
        std::cerr << "error: " << result.error().message << "\n";
        return 1;
    }

    std::cout << orchestrator.telemetry().report().dump(2) << "\n";
    return 0;
}
