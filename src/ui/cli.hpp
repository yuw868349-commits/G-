#pragma once

#include <memory>
#include <string>
#include "core/orchestrator.hpp"
#include "llm/provider.hpp"

namespace swiftagent {

struct CliOptions {
    std::string task;
    std::string provider_name = "fake";
    std::string model = "gpt-4o-mini";
    std::string api_key;
    std::string api_base;
    std::uint32_t budget_turns = 32;
    bool use_web = false;
    int web_port = 8080;
    bool verbose = false;
};

CliOptions parse_cli(int argc, char** argv);

int run_cli(const CliOptions& opts);
int run_web(const CliOptions& opts);

std::unique_ptr<Provider> make_provider_from_cli(const CliOptions& opts);

} // namespace swiftagent
