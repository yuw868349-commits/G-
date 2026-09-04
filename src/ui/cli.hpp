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
    // Bind address for the web panel.  Default to loopback so the
    // shell tool, which can execute arbitrary programs on the host,
    // is not exposed to the public network.  Operators can opt in
    // to a public binding explicitly via --web-host 0.0.0.0 (or any
    // other address) when they understand the implications.
    std::string web_host = "127.0.0.1";
    // Optional HTTP basic-auth credentials.  When both fields are
    // non-empty the web panel requires an `Authorization: Basic …`
    // header on every request.  Empty credentials disable auth but
    // the loopback default binding is the primary defence.
    std::string web_user;
    std::string web_pass;
    bool verbose = false;
};

CliOptions parse_cli(int argc, char** argv);

int run_cli(const CliOptions& opts);
int run_web(const CliOptions& opts);

std::unique_ptr<Provider> make_provider_from_cli(const CliOptions& opts);

} // namespace swiftagent
