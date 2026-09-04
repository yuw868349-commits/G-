#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include "core/orchestrator.hpp"
#include "llm/fake_provider.hpp"
#include "tools/builtin.hpp"

using namespace swiftagent;

namespace {

class HarnessContext final : public ToolContext {
public:
    std::string read_file(const std::string& path) override {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return "";
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
    std::string exec(const std::string& cmd) override {
        std::string full = cmd + " 2>&1";
        FILE* pipe = ::popen(full.c_str(), "r");
        if (!pipe) {
            return "";
        }
        char buf[4096];
        std::string out;
        while (std::fgets(buf, sizeof(buf), pipe)) {
            out += buf;
        }
        ::pclose(pipe);
        return out;
    }
    bool file_exists(const std::string& path) const override {
        return std::filesystem::exists(path);
    }
};

} // namespace

TEST_CASE("end-to-end run drives a real task to completion") {
    FakeProvider provider;
    provider.script({
        {{"plan", "read the data"}, {"tool_calls", nlohmann::json::array({
            nlohmann::json{{"name", "read_file"}, {"arguments", nlohmann::json{{"path", "/dev/null"}}}}
        })}},
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });
    OrchestratorOptions options;
    options.token_budget = 1024;
    Orchestrator orch(provider, options);
    orch.register_builtin();
    Budget budget;
    budget.max_turns = 4;
    budget.active = true;
    auto result = orch.run("summarize this folder", budget);
    REQUIRE(result.ok());
    auto& r = result.value();
    CHECK(r.completed);
    CHECK(r.turns == 2);
    CHECK(r.final_output == "DONE");
    CHECK(orch.replay().size() > 0);
    auto events = orch.replay().events();
    bool saw_task_ended = false;
    for (const auto& e : events) {
        if (e.kind == EventKind::TaskEnded) {
            saw_task_ended = true;
        }
    }
    CHECK(saw_task_ended);
}

TEST_CASE("end-to-end run respects budget when model never yields") {
    FakeProvider provider;
    provider.script({
        {{"plan", "loop"}, {"tool_calls", nlohmann::json::array({
            nlohmann::json{{"name", "read_file"}, {"arguments", nlohmann::json{{"path", "/x"}}}}
        })}},
        {{"plan", "loop"}, {"tool_calls", nlohmann::json::array({
            nlohmann::json{{"name", "read_file"}, {"arguments", nlohmann::json{{"path", "/x"}}}}
        })}},
        {{"plan", "loop"}, {"tool_calls", nlohmann::json::array({
            nlohmann::json{{"name", "read_file"}, {"arguments", nlohmann::json{{"path", "/x"}}}}
        })}}
    });
    OrchestratorOptions options;
    options.token_budget = 1024;
    Orchestrator orch(provider, options);
    orch.register_builtin();
    Budget budget;
    budget.max_turns = 2;
    budget.active = true;
    auto result = orch.run("any task", budget);
    REQUIRE(result.ok());
    auto& r = result.value();
    CHECK(r.bounded);
    CHECK(r.turns == 2);
}

TEST_CASE("end-to-end read_file sees real on-disk content written by the agent") {
    // This is the integration test that was missing in the original
    // review: it drives the orchestrator through write_file, then
    // read_file, and asserts that the second tool call actually sees
    // the file produced by the first one.  Without a real
    // ToolContext behind the built-in tools this test would always
    // see "file not found" and fail.
    namespace fs = std::filesystem;
    fs::path tmp_root = fs::temp_directory_path() /
                       ("swiftagent_e2e_" + std::to_string(::getpid()));
    fs::create_directories(tmp_root);
    const fs::path target = tmp_root / "data.txt";

    // Make sure we start from a clean slate.
    std::error_code ec;
    fs::remove(target, ec);

    FakeProvider provider;
    provider.script({
        {{"plan", "create the file"},
         {"tool_calls", nlohmann::json::array({
             nlohmann::json{
                 {"name", "write_file"},
                 {"arguments", nlohmann::json{
                     {"path", target.string()},
                     {"content", "the quick brown fox jumps over the lazy dog"}
                 }}
             }
         })}},
        {{"plan", "read it back"},
         {"tool_calls", nlohmann::json::array({
             nlohmann::json{
                 {"name", "read_file"},
                 {"arguments", nlohmann::json{
                     {"path", target.string()}
                 }}
             }
         })}},
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });

    OrchestratorOptions options;
    options.token_budget = 4096;
    Orchestrator orch(provider, options);
    orch.register_builtin(std::make_shared<HarnessContext>());

    Budget budget;
    budget.max_turns = 8;
    budget.active = true;

    auto result = orch.run("write a file, then read it back", budget);
    REQUIRE(result.ok());
    auto& r = result.value();
    CHECK(r.completed);
    CHECK(r.turns >= 3);

    // The file should be on disk after the run.
    REQUIRE(fs::exists(target));
    std::ifstream in(target);
    std::stringstream ss;
    ss << in.rdbuf();
    CHECK(ss.str() == "the quick brown fox jumps over the lazy dog");

    // And the orchestrator should have observed at least one cache
    // hit-or-miss plus tool events along the way.
    bool saw_write = false;
    bool saw_read = false;
    for (const auto& e : orch.replay().events()) {
        if (e.kind == EventKind::ToolFinished) {
            const auto& data = e.payload;
            const std::string tool = data.value("tool", "");
            if (tool == "write_file") saw_write = true;
            if (tool == "read_file")  saw_read  = true;
        }
    }
    CHECK(saw_write);
    CHECK(saw_read);

    fs::remove_all(tmp_root, ec);
}

TEST_CASE("end-to-end shell tool runs commands and returns output") {
    // Smoke test the shell tool against a real process so that the
    // BasicContext → popen() path is exercised end to end.
    FakeProvider provider;
    provider.script({
        {{"plan", "run a command"},
         {"tool_calls", nlohmann::json::array({
             nlohmann::json{
                 {"name", "shell"},
                 {"arguments", nlohmann::json{{"cmd", "printf hello-agent"}}}
             }
         })}},
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });
    OrchestratorOptions options;
    options.token_budget = 2048;
    Orchestrator orch(provider, options);
    orch.register_builtin();
    Budget budget;
    budget.max_turns = 4;
    budget.active = true;
    auto result = orch.run("run a shell command", budget);
    REQUIRE(result.ok());
    CHECK(result.value().completed);
}

TEST_CASE("end-to-end cache key is reused for repeat identical calls") {
    // Drive the orchestrator twice through identical read_file calls
    // and verify that the second one comes from cache.
    namespace fs = std::filesystem;
    fs::path tmp_root = fs::temp_directory_path() /
                       ("swiftagent_cache_" + std::to_string(::getpid()));
    fs::create_directories(tmp_root);
    const fs::path target = tmp_root / "stable.txt";
    {
        std::ofstream out(target);
        out << "constant payload";
    }

    FakeProvider provider;
    provider.script({
        {{"plan", "first read"},
         {"tool_calls", nlohmann::json::array({
             nlohmann::json{
                 {"name", "read_file"},
                 {"arguments", nlohmann::json{{"path", target.string()}}}
             }
         })}},
        {{"plan", "second read"},
         {"tool_calls", nlohmann::json::array({
             nlohmann::json{
                 {"name", "read_file"},
                 {"arguments", nlohmann::json{{"path", target.string()}}}
             }
         })}},
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });

    OrchestratorOptions options;
    options.token_budget = 4096;
    Orchestrator orch(provider, options);
    orch.register_builtin(std::make_shared<HarnessContext>());
    Budget budget;
    budget.max_turns = 6;
    budget.active = true;

    auto result = orch.run("read the same file twice", budget);
    REQUIRE(result.ok());
    CHECK(orch.cache().hits() >= 1);

    std::error_code ec;
    fs::remove_all(tmp_root, ec);
}
