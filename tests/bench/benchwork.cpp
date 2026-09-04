#include "benchwork.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <sstream>
#include <thread>
#include <vector>

#include "core/orchestrator.hpp"
#include "core/telemetry.hpp"
#include "llm/fake_provider.hpp"
#include "tools/builtin.hpp"
#include "tools/registry.hpp"

namespace swiftagent {

namespace {

nlohmann::json script_reorganize(std::size_t n) {
    nlohmann::json calls = nlohmann::json::array();
    for (std::size_t i = 0; i < n; ++i) {
        calls.push_back({
            {"name", "write_file"},
            {"arguments", {{"path", "out/file_" + std::to_string(i) + ".txt"},
                            {"content", "content " + std::to_string(i)}}}
        });
    }
    return {
        {"plan", "reorganize"},
        {"tool_calls", calls}
    };
}

nlohmann::json script_gather(std::size_t n) {
    nlohmann::json calls = nlohmann::json::array();
    for (std::size_t i = 0; i < n; ++i) {
        calls.push_back({
            {"name", "read_file"},
            {"arguments", {{"path", "in/data_" + std::to_string(i) + ".txt"}}}
        });
    }
    return {
        {"plan", "gather"},
        {"tool_calls", calls}
    };
}

nlohmann::json script_install(std::size_t n) {
    nlohmann::json calls = nlohmann::json::array();
    for (std::size_t i = 0; i < n; ++i) {
        calls.push_back({
            {"name", "shell"},
            {"arguments", {{"program", "sleep"},
                            {"args", nlohmann::json::array({"0.005"})}}}
        });
    }
    return {
        {"plan", "install"},
        {"tool_calls", calls}
    };
}

class TimingToolContext final : public ToolContext {
public:
    std::string read_file(const std::string& path) override {
        std::ifstream in(path);
        if (!in) {
            return "";
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
    std::string exec(const std::string& /*cmd*/) override {
        // No shell fallback in tests; the bench shell tool uses argv.
        return "";
    }
    bool file_exists(const std::string& path) const override {
        return std::filesystem::exists(path);
    }
};

BenchResult run_bench(const std::string& name, const nlohmann::json& script,
                      bool parallel, double baseline_ms) {
    BenchResult result;
    result.name = name;
    result.baseline_ms = baseline_ms;
    auto start = std::chrono::steady_clock::now();
    FakeProvider provider;
    provider.script({script, {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}});
    OrchestratorOptions options;
    options.token_budget = 16384;
    Orchestrator orch(provider, options);
    orch.register_builtin();
    orch.set_executor_parallel(parallel);
    Budget budget;
    budget.max_turns = 4;
    budget.active = true;
    auto run_result = orch.run("bench", budget);
    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.tool_invocations = orch.executor().invocations();
    result.cache_hits = orch.cache().hits();
    (void)run_result;
    return result;
}

} // namespace

BenchResult BenchHarness::file_reorganization(std::size_t files, bool parallel) {
    std::filesystem::create_directories("out");
    auto script = script_reorganize(files);
    return run_bench("file_reorganization", script, parallel,
                     static_cast<double>(files) * 0.6);
}

BenchResult BenchHarness::data_gathering(std::size_t items, bool parallel) {
    std::filesystem::create_directories("in");
    for (std::size_t i = 0; i < items; ++i) {
        std::ofstream out("in/data_" + std::to_string(i) + ".txt");
        out << "data-" << i;
    }
    auto script = script_gather(items);
    return run_bench("data_gathering", script, parallel,
                     static_cast<double>(items) * 0.4);
}

BenchResult BenchHarness::dependency_install(std::size_t packages, bool parallel) {
    auto script = script_install(packages);
    return run_bench("dependency_install", script, parallel,
                     static_cast<double>(packages) * 5.0);
}

} // namespace swiftagent
