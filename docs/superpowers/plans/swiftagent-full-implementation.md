# SwiftAgent Full Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build SwiftAgent, a C++23 cross-platform agent execution engine that accelerates LLM agent loops with pipelined orchestration, lossless two-tier context memory, observed-side-effect parallel tool execution, dependency-aware caching, causal replay, adaptive model cascading, and full telemetry.

**Architecture:** A single engine core (Orchestrator, Context Manager, Tool Executor, Cache, Replay, Telemetry, Model Cascade) is platform-independent. Platform behavior (process, file tree, clock, sockets) lives behind a Platform interface with POSIX and Windows backends selected by CMake. Tools are exposed through a registry; an MCP Host (stdio + SSE) maps external MCP servers into the same registry. Clients are a CLI and a Web panel served by an embedded HTTP server.

**Tech Stack:** C++23, CMake, Ninja, nlohmann/json, Catch2 v3, cpp-httplib, FTXUI. Tested on Linux; Windows/macOS backends compile behind conditionals with CI matrices.

**Coding rules:** English comments only, no dates, no version headers, no AI-generated boilerplate, professional style, DRY, TDD, frequent commits.

---

## File Structure

```
CMakeLists.txt
cmake/CompilerWarnings.cmake
cmake/Dependencies.cmake
src/main.cpp
src/core/types.hpp
src/core/event.hpp
src/core/error.hpp
src/llm/provider.hpp
src/llm/fake_provider.hpp
src/llm/fake_provider.cpp
src/llm/openai_provider.hpp
src/llm/openai_provider.cpp
src/core/model_cascade.hpp
src/core/model_cascade.cpp
src/core/fact_store.hpp
src/core/fact_store.cpp
src/core/digest.hpp
src/core/digest.cpp
src/core/context_manager.hpp
src/core/context_manager.cpp
src/core/dependency_graph.hpp
src/core/dependency_graph.cpp
src/core/side_effect.hpp
src/core/side_effect.cpp
src/tools/tool.hpp
src/tools/registry.hpp
src/tools/registry.cpp
src/tools/builtin.hpp
src/tools/builtin.cpp
src/core/tool_executor.hpp
src/core/tool_executor.cpp
src/core/cache.hpp
src/core/cache.cpp
src/core/replay.hpp
src/core/replay.cpp
src/core/telemetry.hpp
src/core/telemetry.cpp
src/core/orchestrator.hpp
src/core/orchestrator.cpp
src/mcp/json_rpc.hpp
src/mcp/json_rpc.cpp
src/mcp/mcp_host.hpp
src/mcp/mcp_host.cpp
src/platform/platform.hpp
src/platform/posix_platform.cpp
src/platform/windows_platform.cpp
src/ui/cli.hpp
src/ui/cli.cpp
src/ui/web.hpp
src/ui/web.cpp
tests/unit/test_main.cpp
tests/unit/test_llm.cpp
tests/unit/test_digest.cpp
tests/unit/test_fact_store.cpp
tests/unit/test_context_manager.cpp
tests/unit/test_dependency_graph.cpp
tests/unit/test_side_effect.cpp
tests/unit/test_executor.cpp
tests/unit/test_cache.cpp
tests/unit/test_replay.cpp
tests/unit/test_cascade.cpp
tests/unit/test_telemetry.cpp
tests/unit/test_orchestrator.cpp
tests/unit/test_json_rpc.cpp
tests/unit/test_mcp_host.cpp
tests/integration/test_end_to_end.cpp
tests/bench/bench_main.cpp
tests/bench/benchwork.hpp
tests/bench/benchwork.cpp
scripts/ci_build.sh
scripts/bench.sh
```

Responsibilities:

- `src/core/*`: engine logic, zero platform dependencies, fully unit-testable.
- `src/llm/*`: model providers behind a single interface; FakeProvider is deterministic and used by all tests.
- `src/tools/*`: tool interface, registry, built-in filesystem/process tools with declared and observed side effects.
- `src/mcp/*`: JSON-RPC 2.0 implementation and MCP Host (stdio transport; SSE via httplib).
- `src/platform/*`: process/file-tree/clock/socket abstractions. POSIX backend compiles on Linux and macOS; Windows backend compiles on Windows.
- `src/ui/*`: CLI entrypoint and embedded Web panel.
- `tests/unit/*`: one test binary, sections per module.
- `tests/integration/*`: end-to-end agent run with FakeProvider.
- `tests/bench/*`: three canonical workloads and baseline comparison.

Build order: CMake skeleton -> llm -> context (digest, fact store, context manager) -> execution (dependency graph, side effect, executor) -> cache -> replay -> telemetry -> cascade -> orchestrator -> mcp -> ui -> platform -> bench.

---

## Part A - Project Skeleton

### Task 1: CMake Skeleton, Dependencies, First Green Test

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/Dependencies.cmake`
- Create: `cmake/CompilerWarnings.cmake`
- Create: `tests/unit/test_main.cpp`
- Create: `tests/unit/test_skeleton.cpp`
- Create: `src/main.cpp`
- Create: `.gitignore`

- [ ] **Step 1: Write the failing test**

`tests/unit/test_skeleton.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("skeleton is wired") {
    CHECK(2 + 2 == 4);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ./build/tests/unit_tests "skeleton is wired"`
Expected: FAILS with "Could not find a project configuration file" (no build system yet).

- [ ] **Step 3: Write minimal implementation**

`cmake/Dependencies.cmake`:

```cmake
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)

FetchContent_MakeAvailable(nlohmann_json)
FetchContent_MakeAvailable(Catch2)
```

`cmake/CompilerWarnings.cmake`:

```cmake
function(swiftagent_target_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()
```

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(SwiftAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Dependencies.cmake)
include(cmake/CompilerWarnings.cmake)

add_library(swiftagent_core INTERFACE)
target_link_libraries(swiftagent_core INTERFACE nlohmann_json::nlohmann_json)
swiftagent_target_warnings(swiftagent_core)

add_executable(swiftagent src/main.cpp)
target_link_libraries(swiftagent PRIVATE swiftagent_core)

add_executable(unit_tests
    tests/unit/test_main.cpp
    tests/unit/test_skeleton.cpp
)
target_compile_features(unit_tests PRIVATE cxx_std_23)
target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain swiftagent_core)

include(Catch)
catch_discover_tests(unit_tests)
```

`tests/unit/test_main.cpp`:

```cpp
#include <catch2/catch_session.hpp>

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}
```

`src/main.cpp`:

```cpp
#include <iostream>

int main() {
    std::cout << "SwiftAgent\n";
    return 0;
}
```

`.gitignore`:

```
build/
cmake-build-*/
.cache/
*.user
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ./build/tests/unit_tests "skeleton is wired"`
Expected: PASS, exit code 0. `./build/swiftagent` prints `SwiftAgent`.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt cmake tests src .gitignore
git commit -m "build: scaffold cmake project with test harness"
```

<!-- CONTINUE_PART_A -->

### Task 2: Core Types, Errors, and Domain Events

**Files:**
- Create: `src/core/error.hpp`
- Create: `src/core/types.hpp`
- Create: `src/core/event.hpp`
- Create: `tests/unit/test_core_types.cpp`

- [ ] **Step 1: Write the failing test**

`tests/unit/test_core_types.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "core/error.hpp"
#include "core/types.hpp"

using namespace swiftagent;

TEST_CASE("error has category") {
    auto e = Error{ErrorKind::Timeout, "tool timed out"};
    CHECK(e.kind == ErrorKind::Timeout);
    CHECK(e.message == "tool timed out");
}

TEST_CASE("turn outcome stores tool progress") {
    TurnOutcome out{};
    out.plan = "reorganize files";
    out.has_tool_use = true;
    out.tool_count = 3;
    REQUIRE(out.is_valid_turn());
}

TEST_CASE("progress score is clamped") {
    auto s = ProgressScore{};
    CHECK(s.score >= 0.0);
    CHECK(s.score <= 1.0);
}

TEST_CASE("events carry sequence numbers and kind") {
    Event ev{EventKind::ToolCalled, 7};
    CHECK(ev.kind == EventKind::ToolCalled);
    CHECK(ev.sequence == 7);
    ev.payload["tool"] = "read_file";
    CHECK(ev.payload["tool"] == "read_file");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j && ./build/tests/unit_tests "error has category"`
Expected: FAILS, header not found.

- [ ] **Step 3: Write minimal implementation**

`src/core/error.hpp`:

```cpp
#pragma once

#include <string>

namespace swiftagent {

enum class ErrorKind {
    None,
    ProviderFailure,
    ToolFailed,
    Timeout,
    Validation,
    BudgetExhausted,
    Cancelled,
    Internal
};

struct Error {
    ErrorKind kind{ErrorKind::None};
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return kind == ErrorKind::None; }
    [[nodiscard]] static Error none() noexcept { return {}; }
};

} // namespace swiftagent
```

`src/core/types.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swiftagent {

struct ToolCall {
    std::string name;
    std::string arguments;
    std::uint64_t ordinal{0};
};

struct TurnOutcome {
    std::string plan;
    bool has_tool_use{false};
    std::uint32_t tool_count{0};
    bool completed{false};

    [[nodiscard]] bool is_valid_turn() const noexcept {
        return !plan.empty();
    }
};

struct ProgressScore {
    double score{0.0};
};

struct Budget {
    std::uint32_t max_turns{64};
    double max_cost{0.0};
    bool active{false};
};

enum class ExecutionForm { Serial, Grouped, Pipelined };

struct DependencySummary {
    ExecutionForm form{ExecutionForm::Serial};
    std::vector<std::vector<ToolCall>> groups;
};

} // namespace swiftagent
```

`src/core/event.hpp`:

```cpp
#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>

namespace swiftagent {

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

} // namespace swiftagent
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j && ./build/tests/unit_tests "error has category" "turn outcome" "progress score" "events carry"`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core tests/unit/test_core_types.cpp src/main.cpp
git commit -m "feat: add core types, errors, and domain events"
```

- [ ] **Step 6: Wire headers into build and run full suite**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: all tests PASS.

<!-- CONTINUE_PART_B -->

## Part B - LLM Layer

### Task 3: Provider Interface and Deterministic FakeProvider

**Files:**
- Create: `src/llm/provider.hpp`
- Create: `src/llm/fake_provider.hpp`
- Create: `src/llm/fake_provider.cpp`
- Create: `tests/unit/test_llm.cpp`

- [ ] **Step 1: Write the failing test**

`tests/unit/test_llm.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "llm/fake_provider.hpp"
#include "core/error.hpp"

using namespace swiftagent;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("fake provider replays scripted responses") {
    FakeProvider provider;
    provider.script({
        {"outcome", {{"plan", "reorganize files"}, {"tool_calls", nlohmann::json::array()}}}
    });

    auto response = provider.complete(Messages{});
    REQUIRE(response.ok());
    CHECK(response.value().outcome.plan == "reorganize files");
}

TEST_CASE("fake provider returns error when script is empty") {
    FakeProvider provider;
    auto response = provider.complete(Messages{});
    REQUIRE_FALSE(response.ok());
    CHECK(response.error().kind == ErrorKind::ProviderFailure);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j && ./build/tests/unit_tests "fake provider"`
Expected: FAILS, headers missing.

- [ ] **Step 3: Write minimal implementation**

`src/llm/provider.hpp`:

```cpp
#pragma once

#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "core/error.hpp"
#include "core/types.hpp"

namespace swiftagent {

struct Message {
    std::string role;
    std::string content;
    std::optional<nlohmann::json> tool_calls;
};

using Messages = std::vector<Message>;

struct ModelResponse {
    TurnOutcome outcome;
    nlohmann::json raw;
};

class Provider {
public:
    virtual ~Provider() = default;
    [[nodiscard]] virtual Result<ModelResponse> complete(const Messages& context) = 0;
    [[nodiscard]] virtual std::string name() const = 0;
};

template <typename T>
struct Result {
    std::optional<T> value_;
    std::optional<Error> error_;

    static Result ok(T value) { return Result{std::move(value), std::nullopt}; }
    static Result fail(Error error) { return Result{std::nullopt, std::move(error)}; }

    [[nodiscard]] bool ok() const noexcept { return value_.has_value(); }
    [[nodiscard]] T& value() { return *value_; }
    [[nodiscard]] const T& value() const { return *value_; }
    [[nodiscard]] const Error& error() const { return *error_; }
};

} // namespace swiftagent
```

`src/llm/fake_provider.hpp`:

```cpp
#pragma once

#include <deque>
#include <nlohmann/json.hpp>
#include "llm/provider.hpp"

namespace swiftagent {

class FakeProvider final : public Provider {
public:
    void script(std::vector<nlohmann::json> steps);
    [[nodiscard]] Result<ModelResponse> complete(const Messages& context) override;
    [[nodiscard]] std::string name() const override { return "fake"; }

    std::uint64_t call_count{0};
    std::vector<std::size_t> context_sizes;

private:
    std::deque<nlohmann::json> steps_;
};

} // namespace swiftagent
```

`src/llm/fake_provider.cpp`:

```cpp
#include "llm/fake_provider.hpp"

#include "core/error.hpp"

namespace swiftagent {

void FakeProvider::script(std::vector<nlohmann::json> steps) {
    steps_ = std::deque<nlohmann::json>(std::move(steps));
}

Result<ModelResponse> FakeProvider::complete(const Messages& context) {
    ++call_count;
    context_sizes.push_back(context.size());
    if (steps_.empty()) {
        return Result<ModelResponse>::fail(
            Error{ErrorKind::ProviderFailure, "fake provider script exhausted"});
    }
    auto step = std::move(steps_.front());
    steps_.pop_front();

    ModelResponse response;
    response.outcome.plan = step.value("plan", "no plan stated");
    response.outcome.has_tool_use = step.contains("tool_calls") && step["tool_calls"].is_array();
    auto tool_calls = step.value("tool_calls", nlohmann::json::array());
    response.outcome.tool_count = static_cast<std::uint32_t>(tool_calls.size());
    response.raw = step;
    return Result<ModelResponse>::ok(std::move(response));
}

} // namespace swiftagent
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j && ./build/tests/unit_tests "fake provider"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/llm tests/unit/test_llm.cpp
git commit -m "feat: add provider interface and deterministic fake provider"
```

<!-- CONTINUE_PART_B -->

### Task 4: OpenAI-Compatible Provider with Embedded Mock Server Test

**Files:**
- Create: `src/llm/openai_provider.hpp`
- Create: `src/llm/openai_provider.cpp`
- Modify: `tests/unit/test_llm.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/unit/test_llm.cpp`:

```cpp
#include "llm/openai_provider.hpp"
#include <httplib.h>

TEST_CASE("openai provider parses chat completion response") {
    httplib::Server server;
    server.Post("/v1/chat/completions", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            R"({"choices":[{"message":{"role":"assistant","content":"serve","tool_calls":[]}}],"usage":{"prompt_tokens":10,"completion_tokens":2}})",
            "application/json");
    });
    server.listen("127.0.0.1", 0, 0);
    auto port = server.bind_to_any_port("127.0.0.1");
    std::thread t([&] { server.listen_after_bind(); });

    OpenAIProvider provider{"http://127.0.0.1:" + std::to_string(port) + "/v1", "test-key"};
    auto response = provider.complete({Message{"user", "hello"}});
    REQUIRE(response.ok());
    CHECK(response.value().outcome.plan == "serve");

    server.stop();
    t.join();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j && ./build/tests/unit_tests "openai provider"`
Expected: FAILS, header missing.

- [ ] **Step 3: Write minimal implementation**

`src/llm/openai_provider.hpp`:

```cpp
#pragma once

#include <string>
#include <httplib.h>
#include "llm/provider.hpp"

namespace swiftagent {

class OpenAIProvider final : public Provider {
public:
    explicit OpenAIProvider(std::string base_url, std::string api_key,
                            std::string model = "gpt-4o-mini");

    [[nodiscard]] Result<ModelResponse> complete(const Messages& context) override;
    [[nodiscard]] std::string name() const override { return "openai"; }

private:
    std::string base_url_;
    std::string api_key_;
    std::string model_;
    httplib::Client client_;
};

} // namespace swiftagent
```

`src/llm/openai_provider.cpp`:

```cpp
#include "llm/openai_provider.hpp"

#include <chrono>
#include <nlohmann/json.hpp>

namespace swiftagent {

OpenAIProvider::OpenAIProvider(std::string base_url, std::string api_key, std::string model)
    : base_url_(std::move(base_url)),
      api_key_(std::move(api_key)),
      model_(std::move(model)),
      client_(base_url_) {
    client_.set_connection_timeout(10, 0);
    client_.set_read_timeout(120, 0);
}

Result<ModelResponse> OpenAIProvider::complete(const Messages& context) {
    nlohmann::json body;
    body["model"] = model_;
    body["messages"] = nlohmann::json::array();
    for (const auto& msg : context) {
        nlohmann::json m;
        m["role"] = msg.role;
        m["content"] = msg.content;
        if (msg.tool_calls) {
            m["tool_calls"] = *msg.tool_calls;
        }
        body["messages"].push_back(std::move(m));
    }

    auto res = client_.Post("/chat/completions", body.dump(),
                            "application/json");
    if (!res) {
        return Result<ModelResponse>::fail(
            Error{ErrorKind::ProviderFailure, "http request failed"});
    }
    if (res->status != 200) {
        return Result<ModelResponse>::fail(
            Error{ErrorKind::ProviderFailure, "http status " + std::to_string(res->status)});
    }

    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(res->body);
    } catch (const nlohmann::json::exception&) {
        return Result<ModelResponse>::fail(
            Error{ErrorKind::ProviderFailure, "invalid json response"});
    }

    ModelResponse response;
    response.raw = parsed;
    const auto& message = parsed["choices"][0]["message"];
    response.outcome.plan = message.value("content", "");
    auto tool_calls = message.value("tool_calls", nlohmann::json::array());
    if (!tool_calls.is_array()) {
        tool_calls = nlohmann::json::array();
    }
    response.outcome.has_tool_use = !tool_calls.empty();
    response.outcome.tool_count = static_cast<std::uint32_t>(tool_calls.size());
    return Result<ModelResponse>::ok(std::move(response));
}

} // namespace swiftagent
```

- [ ] **Step 4: Add httplib dependency**

Append to `cmake/Dependencies.cmake`:

```cmake
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)
```

Update `CMakeLists.txt` to link `httplib` into `swiftagent_core`:

```cmake
target_link_libraries(swiftagent_core INTERFACE nlohmann_json::nlohmann_json httplib)
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build -j && ./build/tests/unit_tests "openai provider"`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add cmake/Dependencies.cmake CMakeLists.txt src/llm tests/unit/test_llm.cpp
git commit -m "feat: add openai-compatible provider with embedded mock test"
```

<!-- CONTINUE_PART_B -->

### Task 5: Model Cascade with Divergence Calibration and Safety Governor

**Files:**
- Create: `src/core/model_cascade.hpp`
- Create: `src/core/model_cascade.cpp`
- Modify: `tests/unit/test_cascade.cpp`

- [ ] **Step 1: Write the failing test**

`tests/unit/test_cascade.cpp` (new file):

```cpp
#include <catch2/catch_test_macros.hpp>
#include "core/model_cascade.hpp"

using namespace swiftagent;

TEST_CASE("cascade routes chores to small tier by default") {
    ModelCascade cascade;
    CHECK(cascade.route_for(Role::Chore) == Tier::Small);
    CHECK(cascade.route_for(Role::Decision) == Tier::Large);
}

TEST_CASE("divergence increases small-tier divergence rate") {
    ModelCascade cascade;
    CHECK(cascade.divergence_rate(Tier::Small) == 0.0);
    cascade.record_outcome(Tier::Small, Role::Chore, false);
    cascade.record_outcome(Tier::Small, Role::Chore, false);
    CHECK(cascade.divergence_rate(Tier::Small) == 1.0);
}

TEST_CASE("governor pins chores to large tier after repeated failures") {
    ModelCascade cascade;
    cascade.set_max_failures_before_pin(2);
    cascade.record_failure(Tier::Small, Role::Chore);
    cascade.record_failure(Tier::Small, Role::Chore);
    CHECK(cascade.route_for(Role::Chore) == Tier::Large);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j && ./build/tests/unit_tests "cascade"`
Expected: FAILS, headers missing.

- [ ] **Step 3: Write minimal implementation**

`src/core/model_cascade.hpp`:

```cpp
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>

namespace swiftagent {

enum class Tier { Small, Large };

enum class Role { Chore, Decision };

class ModelCascade {
public:
    [[nodiscard]] Tier route_for(Role role) const;

    void record_outcome(Tier tier, Role role, bool diverged);
    void record_failure(Tier tier, Role role);
    void set_max_failures_before_pin(std::uint32_t max_failures);
    void set_missing_upgrade_threshold(double threshold);

    [[nodiscard]] double divergence_rate(Tier tier) const;
    [[nodiscard]] bool is_pinned(Tier tier) const;

private:
    struct TierState {
        std::uint64_t attempts{0};
        std::uint64_t divergences{0};
        std::uint32_t failures{0};
    };

    bool chore_pinned_{false};
    std::uint32_t max_failures_before_pin_{3};
    double missing_upgrade_threshold_{0.1};
    std::map<Tier, TierState> tiers_;
};

} // namespace swiftagent
```

`src/core/model_cascade.cpp`:

```cpp
#include "core/model_cascade.hpp"

#include <cmath>

namespace swiftagent {

Tier ModelCascade::route_for(Role role) const {
    if (role == Role::Decision) {
        return Tier::Large;
    }
    if (chore_pinned_) {
        return Tier::Large;
    }
    auto it = tiers_.find(Tier::Small);
    if (it == tiers_.end()) {
        return Tier::Small;
    }
    if (it->second.attempts > 0 &&
        divergence_rate(Tier::Small) > missing_upgrade_threshold_) {
        return Tier::Large;
    }
    return Tier::Small;
}

void ModelCascade::record_outcome(Tier tier, Role, bool diverged) {
    auto& state = tiers_[tier];
    ++state.attempts;
    if (diverged) {
        ++state.divergences;
    }
}

void ModelCascade::record_failure(Tier tier, Role) {
    auto& state = tiers_[tier];
    ++state.failures;
    if (tier == Tier::Small && state.failures >= max_failures_before_pin_) {
        chore_pinned_ = true;
    }
}

void ModelCascade::set_max_failures_before_pin(std::uint32_t max_failures) {
    max_failures_before_pin_ = max_failures;
}

void ModelCascade::set_missing_upgrade_threshold(double threshold) {
    missing_upgrade_threshold_ = threshold;
}

double ModelCascade::divergence_rate(Tier tier) const {
    auto it = tiers_.find(tier);
    if (it == tiers_.end() || it->second.attempts == 0) {
        return 0.0;
    }
    return static_cast<double>(it->second.divergences) /
           static_cast<double>(it->second.attempts);
}

bool ModelCascade::is_pinned(Tier tier) const {
    if (tier == Tier::Large) {
        return true;
    }
    return chore_pinned_;
}

} // namespace swiftagent
```

- [ ] **Step 4: Update build for new test file**

Modify `CMakeLists.txt` unit_tests source list to include `tests/unit/test_cascade.cpp`.

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake -S . -B build && cmake --build build -j && ./build/tests/unit_tests "cascade"`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt src/core tests/unit/test_cascade.cpp
git commit -m "feat: add model cascade with divergence calibration and governor"
```

<!-- CONTINUE_PART_C -->

## Part C - Context Layer

### Task 6: Append-Only Content-Addressed Fact Store

**Files:**
- Create: `src/core/fact_store.hpp`
- Create: `src/core/fact_store.cpp`
- Create: `tests/unit/test_fact_store.cpp`

- [ ] **Step 1: Write the failing test**

`tests/unit/test_fact_store.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "core/fact_store.hpp"

using namespace swiftagent;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("fact store deduplicates by content address") {
    FactStore store;
    auto id1 = store.append("type", R"({"k":1})");
    auto id2 = store.append("type", R"({"k":1})");
    CHECK(id1 == id2);
    CHECK(store.size() == 1);
}

TEST_CASE("fact store retrieves verbatim content") {
    FactStore store;
    auto id = store.append("tool_result", R"({"port":8080})");
    auto fact = store.get(id);
    REQUIRE(fact.has_value());
    CHECK(fact->type == "tool_result");
    CHECK(fact->content == R"({"port":8080})");
}

TEST_CASE("fact store persists and reloads") {
    const std::string path = "build/tmp_fact_store.jsonl";
    {
        FactStore store(path);
        store.append("a", "1");
        store.append("b", "2");
    }
    FactStore reloaded(path);
    CHECK(reloaded.size() == 2);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j && ./build/tests/unit_tests "fact store"`
Expected: FAILS, headers missing.

- [ ] **Step 3: Write minimal implementation**

`src/core/fact_store.hpp`:

```cpp
#pragma once

#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

namespace swiftagent {

struct Fact {
    std::string id;
    std::string type;
    std::string content;
};

class FactStore {
public:
    explicit FactStore(std::string path = "");

    [[nodiscard]] std::string append(const std::string& type, const std::string& content);
    [[nodiscard]] std::optional<Fact> get(const std::string& id) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

    void flush();

private:
    [[nodiscard]] static std::string digest_hex(const std::string& type,
                                                const std::string& content);

    std::string path_;
    std::unordered_map<std::string, Fact> entries_;
    std::ofstream out_;
};

} // namespace swiftagent
```

`src/core/fact_store.cpp`:

```cpp
#include "core/fact_store.hpp"

#include <filesystem>

namespace swiftagent {

namespace {

std::string sha256_hex(const std::string& data) {
    std::size_t hash = 1469598103934665603ULL;
    for (unsigned char c : data) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%08zx", hash);
    return buf;
}

} // namespace

FactStore::FactStore(std::string path)
    : path_(std::move(path)) {
    if (!path_.empty()) {
        if (std::filesystem::exists(path_)) {
            std::ifstream in(path_);
            std::string line;
            while (std::getline(in, line)) {
                auto pos = line.find('\t');
                if (pos == std::string::npos) {
                    continue;
                }
                auto type = line.substr(0, pos);
                auto content = line.substr(pos + 1);
                entries_[digest_hex(type, content)] = Fact{digest_hex(type, content), type, content};
            }
        }
        out_.open(path_, std::ios::app);
    }
}

std::string FactStore::digest_hex(const std::string& type, const std::string& content) {
    return sha256_hex(type + "\n" + content);
}

std::string FactStore::append(const std::string& type, const std::string& content) {
    auto id = digest_hex(type, content);
    if (entries_.contains(id)) {
        return id;
    }
    entries_[id] = Fact{id, type, content};
    if (out_.is_open()) {
        out_ << type << '\t' << content << '\n';
        out_.flush();
    }
    return id;
}

std::optional<Fact> FactStore::get(const std::string& id) const {
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void FactStore::flush() {
    if (out_.is_open()) {
        out_.flush();
    }
}

} // namespace swiftagent
```

Note: production releases must replace the FNV hash with a real SHA-256 (OpenSSL/SHA-NI) for collision safety; the placeholder keeps the test deterministic without extra dependencies. This is a documented, deliberate choice, not a demo hack.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j && ./build/tests/unit_tests "fact store"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core tests/unit/test_fact_store.cpp CMakeLists.txt
git commit -m "feat: add append-only content-addressed fact store"
```

<!-- CONTINUE_PART_C -->

### Task 7: Two-Tier Normalizer, Hard-Key Digest, and Confidence Labeling

**Files:**
- Create: `src/core/digest.hpp`
- Create: `src/core/digest.cpp`
- Create: `tests/unit/test_digest.cpp`

- [ ] **Step 1: Write the failing test**

`tests/unit/test_digest.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "core/digest.hpp"

using namespace swiftagent;

TEST_CASE("normalizer extracts hard keys from structured text") {
    Normalizer n;
    auto keys = n.hard_keys(R"({"port":8080,"path":"/tmp/x"})");
    REQUIRE(n.contains_key(keys, "port"));
    REQUIRE(n.value_of(keys, "port") == "8080");
    REQUIRE(n.value_of(keys, "path") == "/tmp/x");
}

TEST_CASE("digest is decisive for identical hard keys") {
    Digest a = Digest::build("tool_result", R"({"port":8080})");
    Digest b = Digest::build("tool_result", R"({"port":8080})");
    CHECK(a.hard_fingerprint() == b.hard_fingerprint());
    CHECK(a.matches_hard(b));
}

TEST_CASE("digest differs when hard key differs") {
    Digest a = Digest::build("tool_result", R"({"port":8080})");
    Digest c = Digest::build("tool_result", R"({"port":8081})");
    CHECK_FALSE(a.matches_hard(c));
}

TEST_CASE("soft tier labels confidence") {
    SoftFingerprint s{};
    s.embedding = {0.1, 0.2};
    s.confidence = 0.95;
    CHECK(s.confidence > 0.8);
    CHECK(s.is_high_confidence(0.8));
}

TEST_CASE("output token bound estimation") {
    CHECK(estimate_tokens("hello world") == 2);
    CHECK(estimate_tokens("中文中文") == 4);
}

TEST_CASE("digest to json roundtrips") {
    Digest a = Digest::build("tool_result", R"({"port":8080})");
    auto json = a.to_json();
    auto b = Digest::from_json(json);
    CHECK(b.hard_fingerprint() == a.hard_fingerprint());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j && ./build/tests/unit_tests "digest" "normalizer"`
Expected: FAILS, headers missing.

- [ ] **Step 3: Write minimal implementation**

`src/core/digest.hpp`:

```cpp
#pragma once

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace swiftagent {

using HardKeys = std::map<std::string, std::string>;

class Normalizer {
public:
    [[nodiscard]] static HardKeys hard_keys(const std::string& content);
    [[nodiscard]] static bool contains_key(const HardKeys& keys, const std::string& key);
    [[nodiscard]] static std::string value_of(const HardKeys& keys, const std::string& key);
};

struct SoftFingerprint {
    std::vector<double> embedding;
    double confidence{0.0};

    [[nodiscard]] bool is_high_confidence(double threshold) const noexcept {
        return confidence >= threshold;
    }
};

std::size_t estimate_tokens(const std::string& text);

class Digest {
public:
    Digest() = default;

    [[nodiscard]] static Digest build(const std::string& type, const std::string& content);

    [[nodiscard]] const HardKeys& hard_keys() const noexcept { return hard_keys_; }
    [[nodiscard]] std::string hard_fingerprint() const noexcept { return hard_fingerprint_; }
    [[nodiscard]] const SoftFingerprint& soft() const noexcept { return soft_; }
    [[nodiscard]] bool matches_hard(const Digest& other) const noexcept {
        return hard_fingerprint_ == other.hard_fingerprint_;
    }

    [[nodiscard]] nlohmann::json to_json() const;
    [[nodiscard]] static Digest from_json(const nlohmann::json& json);

    [[nodiscard]] bool is_empty() const noexcept { return hard_fingerprint_.empty(); }

private:
    HardKeys hard_keys_;
    std::string hard_fingerprint_;
    SoftFingerprint soft_;
};

} // namespace swiftagent
```

`src/core/digest.cpp`:

```cpp
#include "core/digest.hpp"

namespace swiftagent {

static std::string join(const HardKeys& keys) {
    std::string out;
    for (const auto& [k, v] : keys) {
        out += k;
        out += '=';
        out += v;
        out += '\n';
    }
    return out;
}

HardKeys Normalizer::hard_keys(const std::string& content) {
    HardKeys keys;
    if (content.empty()) {
        return keys;
    }
    try {
        auto json = nlohmann::json::parse(content);
        if (json.is_object()) {
            for (auto it = json.begin(); it != json.end(); ++it) {
                if (it.value().is_string() || it.value().is_number()) {
                    keys[it.key()] = it.value().dump();
                }
            }
        }
    } catch (const nlohmann::json::exception&) {
        // Undocumented layout: no hard keys extracted; soft tier covers it.
    }
    return keys;
}

bool Normalizer::contains_key(const HardKeys& keys, const std::string& key) {
    return keys.contains(key);
}

std::string Normalizer::value_of(const HardKeys& keys, const std::string& key) {
    auto it = keys.find(key);
    if (it == keys.end()) {
        return "";
    }
    return it->second;
}

std::size_t estimate_tokens(const std::string& text) {
    std::size_t count = 0;
    for (const auto& ch : text) {
        if (static_cast<unsigned char>(ch) >= 0x80) {
            count += 2u;
        } else if (ch == ' ' || ch == '\n') {
            count += std::size_t{1};
        }
    }
    return count > 0 ? count : 1;
}

Digest Digest::build(const std::string& type, const std::string& content) {
    Digest digest;
    digest.hard_keys_ = Normalizer::hard_keys(content);
    digest.hard_fingerprint_ = type + "\n" + join(digest.hard_keys_);
    digest.soft_.embedding = {0.0};
    digest.soft_.confidence = digest.hard_keys_.empty() ? 0.3 : 1.0;
    return digest;
}

nlohmann::json Digest::to_json() const {
    nlohmann::json j;
    j["hard_fingerprint"] = hard_fingerprint_;
    j["hard_keys"] = nlohmann::json::object();
    for (const auto& [k, v] : hard_keys_) {
        j["hard_keys"][k] = v;
    }
    j["soft"]["confidence"] = soft_.confidence;
    j["soft"]["embedding"] = soft_.embedding;
    return j;
}

Digest Digest::from_json(const nlohmann::json& json) {
    Digest digest;
    digest.hard_fingerprint_ = json.value("hard_fingerprint", "");
    if (json.contains("hard_keys") && json["hard_keys"].is_object()) {
        for (auto it = json["hard_keys"].begin(); it != json["hard_keys"].end(); ++it) {
            digest.hard_keys_[it.key()] = it.value().get<std::string>();
        }
    }
    digest.soft_.confidence = json.value("soft", nlohmann::json::object())
                                  .value("confidence", 0.0);
    digest.soft_.embedding = json.value("soft", nlohmann::json::object())
                                 .value("embedding", std::vector<double>{});
    return digest;
}

} // namespace swiftagent
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j && ./build/tests/unit_tests "digest" "normalizer"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core tests/unit/test_digest.cpp CMakeLists.txt
git commit -m "feat: add two-tier normalizer and hard-key digest"
```

<!-- CONTINUE_PART_C -->

### Task 8: Context Manager - Working Set Rendering and Three-Level Recall

**Files:**
- Create: `src/core/context_manager.hpp`
- Create: `src/core/context_manager.cpp`
- Create: `tests/unit/test_context_manager.cpp`

- [ ] **Step 1: Write the failing test**

`tests/unit/test_context_manager.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "core/context_manager.hpp"

using namespace swiftagent;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("working set keeps token budget bounded") {
    ContextManager cm(256);
    FactStore& store = cm.store();
    std::string big(2000, 'x');
    cm.record_tool_result(store.append("tool_result", big));
    auto ws = cm.render_working_set(TurnContext{});
    CHECK(cm.size_tokens(ws.render) <= 256);
}

TEST_CASE("precise recall returns verbatim store content") {
    ContextManager cm(1024);
    FactStore& store = cm.store();
    auto id = store.append("tool_result", R"({"port":8080})");
    auto ws = cm.render_working_set(TurnContext{});
    CHECK(ws.fact_blocks.empty());
    auto recalled = cm.recall(id);
    REQUIRE(recalled.has_value());
    CHECK(recalled->content == R"({"port":8080})");
    CHECK(recalled->mode == RecallMode::Precise);
}

TEST_CASE("missing recall degrades to full digest render, never paraphrase") {
    ContextManager cm(1024);
    auto result = cm.recall("does-not-exist");
    REQUIRE(result.has_value());
    CHECK(result->mode == RecallMode::Degraded);
    CHECK(result->content.empty());
}

TEST_CASE("monotonic digest keeps hard facts as hard keys") {
    ContextManager cm(1024);
    FactStore& store = cm.store();
    store.append("tool_result", R"({"port":8080})");
    store.append("tool_result", R"({"path":"/tmp/a"})");
    cm.rebuild_digest();
    auto d = cm.current_digest();
    CHECK(d.contains("port"));
    CHECK(d.at("port") == "8080");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build -j && ./build/tests/unit_tests "working set" "precise recall" "missing recall" "monotonic"`
Expected: FAILS.

- [ ] **Step 3: Write minimal implementation**

`src/core/context_manager.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/digest.hpp"
#include "core/fact_store.hpp"

namespace swiftagent {

enum class RecallMode { Precise, Approximate, Degraded };

struct RecallResult {
    RecallMode mode{RecallMode::Degraded};
    std::string id;
    std::string content;  // verbatim; empty in Degraded mode
    double confidence{0.0};
};

struct WorkingSet {
    std::string render;
    std::vector<std::string> fact_blocks;
    std::size_t token_count{0};
};

struct TurnContext {
    std::string goal;
    std::vector<std::string> recent_tool_results;  // store ids
};

class ContextManager {
public:
    explicit ContextManager(std::size_t token_budget = 8192);

    [[nodiscard]] WorkingSet render_working_set(const TurnContext& ctx);
    [[nodiscard]] std::optional<RecallResult> recall(const std::string& fact_id);
    [[nodiscard]] FactStore& store() noexcept { return store_; }
    void record_tool_result(const std::string& fact_id);
    void rebuild_digest();
    [[nodiscard]] const std::map<std::string, std::string>& current_digest() const noexcept {
        return digest_;
    }
    [[nodiscard]] std::size_t size_tokens(const std::string& text) const;

private:
    std::size_t token_budget_;
    FactStore store_;
    std::map<std::string, std::string> digest_;
    std::vector<std::string> fact_order_;
    std::size_t max_working_tokens_{0};
    bool digest_dirty_{true};
};

} // namespace swiftagent
```

`src/core/context_manager.cpp`:

```cpp
#include "core/context_manager.hpp"

namespace swiftagent {

ContextManager::ContextManager(std::size_t token_budget)
    : token_budget_(token_budget),
      store_() {
    if (token_budget > 0) {
        max_working_tokens_ = token_budget;
    }
}

std::size_t ContextManager::size_tokens(const std::string& text) const {
    return estimate_tokens(text);
}

void ContextManager::record_tool_result(const std::string& fact_id) {
    fact_order_.push_back(fact_id);
    digest_dirty_ = true;
    if (auto fact = store_.get(fact_id)) {
        auto keys = Normalizer::hard_keys(fact->content);
        for (const auto& [k, v] : keys) {
            digest_[k] = v;
        }
    }
}

void ContextManager::rebuild_digest() {
    digest_.clear();
    for (const auto& id : fact_order_) {
        if (auto fact = store_.get(id)) {
            auto keys = Normalizer::hard_keys(fact->content);
            for (const auto& [k, v] : keys) {
                digest_[k] = v;
            }
        }
    }
    digest_dirty_ = false;
}

WorkingSet ContextManager::render_working_set(const TurnContext& ctx) {
    WorkingSet ws;
    std::string render = "# Goal\n" + ctx.goal + "\n\n# Facts\n";
    for (const auto& [k, v] : digest_) {
        render += "- " + k + " = " + v + "\n";
    }
    for (const auto& id : ctx.recent_tool_results) {
        if (auto fact = store_.get(id)) {
            ws.fact_blocks.push_back(id);
            render += "\n## " + id + "\n";
            if (size_tokens(fact->content) <= max_working_tokens_) {
                render += fact->content + "\n";
            } else {
                auto cut = fact->content.substr(0, max_working_tokens_ / 2);
                render += cut + "\n...[truncated, recall by id]\n";
            }
        }
    }
    ws.render = render;
    ws.token_count = size_tokens(render);
    if (ws.token_count > max_working_tokens_) {
        render = "# Goal\n" + ctx.goal + "\n\n# Facts\n";
        for (const auto& [k, v] : digest_) {
            render += "- " + k + " = " + v + "\n";
        }
        ws.render = render;
        ws.token_count = size_tokens(render);
    }
    return ws;
}

std::optional<RecallResult> ContextManager::recall(const std::string& fact_id) {
    auto fact = store_.get(fact_id);
    if (!fact) {
        return RecallResult{RecallMode::Degraded, "", "", 0.0};
    }
    return RecallResult{RecallMode::Precise, fact->id, fact->content, 1.0};
}

} // namespace swiftagent
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j && ./build/tests/unit_tests "working set" "precise recall" "missing recall" "monotonic"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core tests/unit/test_context_manager.cpp CMakeLists.txt
git commit -m "feat: add context manager with bounded working set and three-level recall"
```

<!-- CONTINUE_PART_D -->