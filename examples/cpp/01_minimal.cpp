// Minimal example: run a single-turn task against the fake provider.
//
// Build: cmake --build build --target example_minimal
// Run:   ./build/example_minimal

#include <cstdlib>
#include <iostream>

#include "core/orchestrator.hpp"
#include "core/types.hpp"
#include "llm/fake_provider.hpp"

int main()
{
    using namespace swiftagent;

    FakeProvider provider;
    provider.script({
        {{"plan", "task is done"}, {"completed", true}},
    });

    Orchestrator orchestrator(provider);
    orchestrator.register_builtin();

    Budget budget;
    budget.active = true;
    budget.max_turns = 4;

    auto result = orchestrator.run("say hello", budget);
    if (!result.ok())
    {
        std::cerr << "error: " << result.error().message << "\n";
        return 1;
    }

    auto& out = result.value();
    std::cout << "turns=" << out.turns << " completed=" << (out.completed ? "yes" : "no")
              << " final=" << out.final_output << "\n";
    return out.completed ? 0 : 1;
}
