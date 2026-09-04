#include "ui/cli.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include "llm/fake_provider.hpp"
#include "llm/openai_provider.hpp"

namespace swiftagent {

std::unique_ptr<Provider> make_provider_from_cli(const CliOptions& opts) {
    if (opts.provider_name == "openai") {
        std::string base = opts.api_base.empty() ? "https://api.openai.com/v1" : opts.api_base;
        return std::make_unique<OpenAIProvider>(base, opts.api_key, opts.model);
    }
    auto fake = std::make_unique<FakeProvider>();
    fake->script({
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });
    return fake;
}

CliOptions parse_cli(int argc, char** argv) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--provider" && i + 1 < argc) {
            opts.provider_name = argv[++i];
        } else if (arg == "--model" && i + 1 < argc) {
            opts.model = argv[++i];
        } else if (arg == "--api-key" && i + 1 < argc) {
            opts.api_key = argv[++i];
        } else if (arg == "--api-base" && i + 1 < argc) {
            opts.api_base = argv[++i];
        } else if (arg == "--budget" && i + 1 < argc) {
            opts.budget_turns = static_cast<std::uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--web") {
            opts.use_web = true;
        } else if (arg == "--port" && i + 1 < argc) {
            opts.web_port = std::stoi(argv[++i]);
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: swiftagent [task] [options]\n"
                      << "  --provider NAME   fake|openai (default fake)\n"
                      << "  --model NAME      model name (default gpt-4o-mini)\n"
                      << "  --api-key KEY     API key\n"
                      << "  --api-base URL    API base URL\n"
                      << "  --budget N        max turns (default 32)\n"
                      << "  --web             run web panel\n"
                      << "  --port N          web port (default 8080)\n";
            std::exit(0);
        } else if (arg[0] != '-') {
            if (!opts.task.empty()) {
                opts.task += " ";
            }
            opts.task += arg;
        }
    }
    return opts;
}

int run_cli(const CliOptions& opts) {
    auto provider = make_provider_from_cli(opts);
    Orchestrator orch(*provider);
    orch.register_builtin();
    Budget budget;
    budget.max_turns = opts.budget_turns;
    budget.active = true;
    auto result = orch.run(opts.task, budget);
    if (!result.ok()) {
        std::cerr << "error: " << result.error().message << "\n";
        return 1;
    }
    auto& r = result.value();
    std::cout << "turns: " << r.turns << " completed: " << (r.completed ? "yes" : "no");
    if (r.bounded) {
        std::cout << " bounded: yes";
    }
    std::cout << "\n";
    if (!r.final_output.empty()) {
        std::cout << "output: " << r.final_output << "\n";
    }
    if (opts.verbose) {
        std::cout << orch.telemetry().report().dump(2) << "\n";
    }
    return 0;
}

} // namespace swiftagent
