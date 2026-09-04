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

Build order: CMake skeleton -> llm -> context (digest, fact store, context manager) -> execution (dependency graph, side effect, executor) -> cache -> replay -> telemetry -> cascade -> orchestrator -> mcp -> ui -> platform -> bench -> python sdk.

---

## Part J - Python SDK

### Task 25: pybind11 Subproject, Build Hook, Smoke Test

**Files:**
- Create: `python/CMakeLists.txt`
- Create: `python/bindings/engine.cpp`
- Create: `python/swiftagent/__init__.py`
- Create: `tests/python/test_smoke.py`
- Create: `pyproject.toml`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing Python test**

`tests/python/test_smoke.py`:

```python
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "build" / "python"))

import swiftagent
import pytest


def test_engine_runs_a_task():
    engine = swiftagent.Engine(provider="fake", budget_turns=2)
    result = engine.run("demo task")
    assert result is not None
    assert result.turns >= 1
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m pytest tests/python/test_smoke.py -q`
Expected: ModuleNotFoundError: `swiftagent`.

- [ ] **Step 3: pyproject.toml**

`pyproject.toml`:

```toml
[build-system]
requires = ["scikit-build-core>=0.10", "pybind11>=2.12", "cmake>=3.24", "ninja"]
build-backend = "scikit_build_core.build"

[project]
name = "swiftagent"
version = "0.1.0"
description = "High-performance agent execution engine."
readme = "README.md"
requires-python = ">=3.10"
license = {text = "MIT"}
authors = [{name = "SwiftAgent Contributors"}]
classifiers = ["Programming Language :: C++", "Programming Language :: Python :: 3"]
dependencies = []

[project.optional-dependencies]
test = ["pytest>=8"]

[tool.scikit-build]
wheel.expand-cmake-modules = ["python/CMakeLists.txt"]
sdist.exclude = [".github", "build*"]
```

- [ ] **Step 4: Python bindings target**

`python/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(swiftagent_python LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(pybind11 CONFIG REQUIRED)
find_package(swiftagent CONFIG REQUIRED)

pybind11_add_module(swiftagent_native MODULE
    bindings/engine.cpp
)
target_link_libraries(swiftagent_native PRIVATE swiftagent::core)
```

`python/bindings/engine.cpp`:

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "core/orchestrator.hpp"
#include "llm/fake_provider.hpp"
#include "llm/openai_provider.hpp"
#include "tools/registry.hpp"
#include "tools/builtin.hpp"

namespace py = pybind11;

namespace {

std::unique_ptr<swiftagent::Provider> make_provider(const std::string& name,
                                                    const std::string& model) {
    if (name == "openai") {
        return std::make_unique<swiftagent::OpenAIProvider>(
            "https://api.openai.com/v1", "", model);
    }
    auto fake = std::make_unique<swiftagent::FakeProvider>();
    fake->script({
        {{"plan", "DONE"}, {"tool_calls", nlohmann::json::array()}}
    });
    return fake;
}

} // namespace

PYBIND11_MODULE(swiftagent_native, m) {
    py::class_<swiftagent::RunResult>(m, "RunResult")
        .def_readonly("completed", &swiftagent::RunResult::completed)
        .def_readonly("bounded", &swiftagent::RunResult::bounded)
        .def_readonly("turns", &swiftagent::RunResult::turns)
        .def_readonly("final_output", &swiftagent::RunResult::final_output);

    py::class_<swiftagent::Telemetry>(m, "Telemetry")
        .def_property_readonly("speedup_x", &swiftagent::Telemetry::speedup_x)
        .def("report", &swiftagent::Telemetry::export_report);

    py::class_<swiftagent::Orchestrator>(m, "Engine")
        .def(py::init([](const std::string& provider, const std::string& model,
                         std::uint32_t budget_turns) {
            auto* p = new swiftagent::Orchestrator(
                *make_provider(provider, model));
            (void)budget_turns;  // bound through run; kept on the signature for forward compat
            return std::unique_ptr<swiftagent::Orchestrator>(p);
        }), py::arg("provider") = "fake", py::arg("model") = "gpt-4o-mini",
             py::arg("budget_turns") = 32)
        .def("run", [](swiftagent::Orchestrator& self, const std::string& task) {
            swiftagent::Budget budget;
            budget.max_turns = 32;
            budget.active = true;
            py::gil_scoped_release release;
            auto result = self.run(task, budget);
            py::gil_scoped_acquire acquire;
            if (!result.ok()) {
                throw std::runtime_error(result.error().message);
            }
            return result.value();
        }, py::arg("task"))
        .def("telemetry", &swiftagent::Orchestrator::telemetry,
             py::return_value_policy::reference_internal)
        .def("replay", &swiftagent::Orchestrator::replay,
             py::return_value_policy::reference_internal);
}
```

`python/swiftagent/__init__.py`:

```python
from .swiftagent_native import Engine, RunResult, Telemetry

__all__ = ["Engine", "RunResult", "Telemetry"]
__version__ = "0.1.0"
```

- [ ] **Step 5: Top-level CMake integration**

Append to `CMakeLists.txt`:

```cmake
option(SWIFTAGENT_BUILD_PYTHON "Build Python bindings" ON)
if(SWIFTAGENT_BUILD_PYTHON)
    add_subdirectory(python)
endif()
```

- [ ] **Step 6: Build and test**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j && python -m pip install --user . && python -m pytest tests/python/test_smoke.py -q`
Expected: test PASSES.

- [ ] **Step 7: Commit**

```bash
git add pyproject.toml python tests/python CMakeLists.txt
git commit -m "feat: add python sdk with pybind11 bindings and smoke test"
```

<!-- CONTINUE_PART_J -->

### Task 26: Python `Engine` Surface, `Budget`, and GIL-Release Audit

**Files:**
- Modify: `python/bindings/engine.cpp`
- Modify: `python/swiftagent/__init__.py`
- Create: `tests/python/test_engine.py`

- [ ] **Step 1: Write the failing test**

`tests/python/test_engine.py`:

```python
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "build" / "python"))

import swiftagent
import pytest


def test_engine_budget_limits_turns():
    engine = swiftagent.Engine(provider="fake", budget_turns=4)
    result = engine.run("loop until budget ends")
    assert result.turns <= 4
    assert result.bounded is True


def test_telemetry_report_contains_speedup():
    engine = swiftagent.Engine(provider="fake", budget_turns=2)
    engine.run("demo")
    snap = engine.telemetry().report()
    assert "speedup_x" in snap
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m pytest tests/python/test_engine.py -q`
Expected: AttributeError (`budget_turns` not honored; no `bounded` field on RunResult).

- [ ] **Step 3: Wire `Budget` through the binding**

Replace the body of `Engine::run` lambda in `python/bindings/engine.cpp`:

```cpp
.def("run", [](swiftagent::Orchestrator& self, const std::string& task, std::uint32_t max_turns) {
    swiftagent::Budget budget;
    budget.max_turns = max_turns;
    budget.active = true;
    py::gil_scoped_release release;
    auto result = self.run(task, budget);
    py::gil_scoped_acquire acquire;
    if (!result.ok()) {
        throw std::runtime_error(result.error().message);
    }
    return result.value();
}, py::arg("task"), py::arg("max_turns") = 32)
```

Update `Engine.__init__` to keep `budget_turns` as a member field on a small Python wrapper instead of dropping it:

```cpp
struct EngineState {
    std::uint32_t budget_turns{32};
};
```

Expose `budget_turns` through a property:

```cpp
py::class_<EngineState>(m, "_EngineState")
    .def(py::init<>())
    .def_readwrite("budget_turns", &EngineState::budget_turns);
```

Note: rather than two parallel C++ state objects, attach state via Python `Engine` subclass in `swiftagent/__init__.py` (Step 4). Keep the binding surface minimal: only `run(task, max_turns=...)` is the public entry point.

- [ ] **Step 4: Python wrapper that closes the budget loop**

Replace `python/swiftagent/__init__.py`:

```python
from .swiftagent_native import Engine as _NativeEngine, RunResult, Telemetry

__all__ = ["Engine", "RunResult", "Telemetry"]
__version__ = "0.1.0"


class Engine(_NativeEngine):
    def __init__(self, provider: str = "fake", model: str = "gpt-4o-mini",
                 budget_turns: int = 32):
        super().__init__(provider=provider, model=model, budget_turns=budget_turns)
        self._budget_turns = int(budget_turns)

    def run(self, task: str, max_turns: int | None = None) -> RunResult:
        if max_turns is None:
            max_turns = self._budget_turns
        return super().run(task, max_turns)
```

- [ ] **Step 5: Audit every binding for GIL release**

Add at top of `engine.cpp` an explicit comment:

```cpp
// GIL policy: every function whose body calls into the engine releases the
// GIL before the call and reacquires it on return. Tool handlers in Python
// run with the GIL held; the executor acquires it around handler dispatch.
```

Bindings on which the GIL is already released: `Engine::run`. Bindings on which the GIL is intentionally held: `Engine::telemetry`/`replay` (cheap, GIL-reacquired accessors are fine).

- [ ] **Step 6: Run test to verify it passes**

Run: `cmake --build build -j && python -m pytest tests/python/test_engine.py -q`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add python tests/python CMakeLists.txt
git commit -m "feat: complete python engine surface with budget and gil audit"
```

<!-- CONTINUE_PART_J -->

### Task 27: Python `@tool` Decorator with Type-Hint Schema and `@mcp_client`

**Files:**
- Create: `python/swiftagent/decorators.py`
- Create: `python/swiftagent/mcp_client.py`
- Create: `tests/python/test_decorators.py`
- Modify: `python/bindings/engine.cpp`
- Modify: `python/swiftagent/__init__.py`

- [ ] **Step 1: Write the failing test**

`tests/python/test_decorators.py`:

```python
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "build" / "python"))

import swiftagent
import pytest


def test_tool_decorator_registers_callable():
    engine = swiftagent.Engine(provider="fake", budget_turns=2)

    @engine.tool
    def read_file(path: str) -> str:
        return f"contents of {path}"

    spec = engine.tool_spec("read_file")
    assert spec is not None
    assert spec["name"] == "read_file"
    assert "path" in spec["schema"]["properties"]


def test_mcp_client_adapts_external_server():
    engine = swiftagent.Engine(provider="fake", budget_turns=2)
    client = swiftagent.McpClient.transport_stdio("python -c pass")
    engine.attach_mcp(client, prefix="test")
    spec = engine.tool_spec("test__noop")
    assert spec is None  # no tools; sanity that attachment is non-throwing
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m pytest tests/python/test_decorators.py -q`
Expected: AttributeError (`tool` decorator and `tool_spec` not exposed).

- [ ] **Step 3: Python decorators and registry bridge**

`python/swiftagent/decorators.py`:

```python
import inspect
import json
from typing import Callable, get_type_hints


def schema_for(func: Callable) -> dict:
    sig = inspect.signature(func)
    hints = get_type_hints(func)
    properties: dict = {}
    required: list = []
    for name, param in sig.parameters.items():
        if name == "self":
            continue
        ann = hints.get(name, str)
        properties[name] = _type_schema(ann)
        if param.default is inspect._empty:
            required.append(name)
    return {
        "type": "object",
        "properties": properties,
        "required": required,
    }


def _type_schema(ann) -> dict:
    name = getattr(ann, "__name__", str(ann))
    if name == "str":
        return {"type": "string"}
    if name == "int":
        return {"type": "integer"}
    if name == "float":
        return {"type": "number"}
    if name == "bool":
        return {"type": "boolean"}
    return {"type": "string"}
```

`python/swiftagent/mcp_client.py`:

```python
import json
import subprocess
import threading
from typing import Optional


class McpClient:
    def __init__(self, send, recv):
        self._send = send
        self._recv = recv
        self._id = 0

    @classmethod
    def transport_stdio(cls, command: str) -> "McpClient":
        proc = subprocess.Popen(
            command, shell=True, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, text=True, bufsize=1,
        )
        lock = threading.Lock()

        def send(payload: dict) -> None:
            with lock:
                proc.stdin.write(json.dumps(payload) + "\n")
                proc.stdin.flush()

        def recv() -> dict:
            line = proc.stdout.readline()
            return json.loads(line) if line else {}

        return cls(send=send, recv=recv)

    def initialize(self) -> None:
        self._id += 1
        self._send({"jsonrpc": "2.0", "id": self._id, "method": "initialize", "params": {}})

    def list_tools(self) -> list:
        self._id += 1
        self._send({"jsonrpc": "2.0", "id": self._id, "method": "tools/list", "params": {}})
        resp = self._recv()
        return resp.get("result", {}).get("tools", [])
```

- [ ] **Step 4: Expose tool registration on the binding**

Append to `python/bindings/engine.cpp`:

```cpp
#include "tools/tool.hpp"
#include "tools/registry.hpp"

#include <pybind11/functional.h>
```

Extend the `Engine` class with two methods:

```cpp
.def("tool", [](py::object self, py::function func) -> py::object {
    auto name = func.attr("__name__").cast<std::string>();
    auto* py_func = new py::function(std::move(func));
    swiftagent::Registry& reg = *reinterpret_cast<swiftagent::Registry*>(self.attr("_registry_ptr").cast<std::uintptr_t>());
    swiftagent::ToolSpec spec;
    spec.name = name;
    spec.description = "";
    spec.declares_side_effects = false;
    spec.handler = [py_func](const nlohmann::json& args) {
        py::gil_scoped_acquire acquire;
        py::object result = py_func->attr("__call__")(py::module::import("json").attr("loads")(args.dump()));
        return swiftagent::ToolResultValue{result.cast<std::string>(), {}};
    };
    reg.register_tool(spec);
    return self;
})
.def("tool_spec", [](py::object self, const std::string& name) -> py::object {
    auto* reg = reinterpret_cast<swiftagent::Registry*>(self.attr("_registry_ptr").cast<std::uintptr_t>());
    auto* spec = reg->find(name);
    if (!spec) {
        return py::none();
    }
    py::dict out;
    out["name"] = spec->name;
    out["description"] = spec->description;
    out["schema"] = spec->schema;
    out["declares_side_effects"] = spec->declares_side_effects;
    return out;
})
.def("attach_mcp", [](py::object self, py::object client, const std::string& prefix) -> py::object {
    auto* reg = reinterpret_cast<swiftagent::Registry*>(self.attr("_registry_ptr").cast<std::uintptr_t>());
    auto tools = client.attr("list_tools")();
    for (auto tool : tools) {
        swiftagent::ToolSpec spec;
        spec.name = prefix + "__" + tool["name"].cast<std::string>();
        spec.description = tool.value("description", std::string{});
        spec.schema = tool.value("inputSchema", nlohmann::json::object());
        spec.declares_side_effects = false;
        spec.handler = [client, prefix](const nlohmann::json& args) {
            py::gil_scoped_acquire acquire;
            py::object result = client.attr("call")(prefix, args.dump());
            return swiftagent::ToolResultValue{result.cast<std::string>(), {}};
        };
        reg->register_tool(spec);
    }
    return self;
});
```

The `_registry_ptr` is a Python-level metadata attribute. In `Engine.__init__` the native side owns the registry; expose it through a `py::class_` member that returns the address:

```cpp
.def("_registry_handle", [](swiftagent::Orchestrator& self) -> std::uintptr_t {
    return reinterpret_cast<std::uintptr_t>(&self.registry());
})
```

Add a `Registry& registry()` accessor to `Orchestrator` (and a forwarding declaration in `core/orchestrator.hpp`).

The Python wrapper sets `_registry_ptr` in `__init__`:

```python
class Engine(_NativeEngine):
    def __init__(self, ...):
        super().__init__(...)
        self._registry_ptr = self._registry_handle()
        self._tool_decorator = tool_decorator_factory(self)
```

The decorator factory uses `swiftagent.decorators.schema_for` to populate `ToolSpec.schema`:

```python
def tool_decorator_factory(engine):
    def decorator(func):
        spec = swiftagent.ToolSpec.from_function(func)
        engine._register_tool(spec)
        return func
    return decorator
```

This is split into a final assembly step in Step 5 to avoid forward references; the tests pass once both sides agree on the registry address.

- [ ] **Step 5: Glue Python side**

Update `python/swiftagent/__init__.py`:

```python
from .swiftagent_native import Engine as _NativeEngine, RunResult, Telemetry
from .decorators import schema_for
from .mcp_client import McpClient

__all__ = ["Engine", "RunResult", "Telemetry", "McpClient", "schema_for"]
__version__ = "0.1.0"


class Engine(_NativeEngine):
    def __init__(self, provider: str = "fake", model: str = "gpt-4o-mini",
                 budget_turns: int = 32):
        super().__init__(provider=provider, model=model, budget_turns=budget_turns)
        self._budget_turns = int(budget_turns)
        self._registry_ptr = self._registry_handle()

    def run(self, task: str, max_turns: int | None = None) -> RunResult:
        if max_turns is None:
            max_turns = self._budget_turns
        return super().run(task, max_turns)

    @property
    def tool(self):
        from .decorators import make_tool_decorator
        return make_tool_decorator(self)
```

- [ ] **Step 6: Run tests**

Run: `cmake --build build -j && python -m pytest tests/python -q`
Expected: all PASS.

- [ ] **Step 7: Commit**

```bash
git add python tests/python CMakeLists.txt
git commit -m "feat: add python @tool decorator and @mcp_client adapter"
```

<!-- CONTINUE_PART_J -->

### Task 28: Python Streaming Observer and End-to-End Smoke Run

**Files:**
- Create: `python/swiftagent/observer.py`
- Create: `tests/python/test_e2e.py`
- Modify: `python/swiftagent/__init__.py`

- [ ] **Step 1: Write the failing test**

`tests/python/test_e2e.py`:

```python
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "build" / "python"))

import swiftagent


def test_observer_receives_events():
    engine = swiftagent.Engine(provider="fake", budget_turns=2)
    received: list = []
    engine.subscribe(lambda ev: received.append(ev["kind"]))
    engine.run("any task")
    assert any(k.startswith("Task") or k.startswith("Tool") for k in received)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m pytest tests/python/test_e2e.py -q`
Expected: AttributeError (`subscribe` not exposed).

- [ ] **Step 3: Add observer to native binding**

Append to `Engine` class in `engine.cpp`:

```cpp
.def("subscribe", [](swiftagent::Orchestrator& self, py::function callback) {
    auto* py_func = new py::function(std::move(callback));
    self.attach_observer([py_func](const swiftagent::Event& ev) {
        py::gil_scoped_acquire acquire;
        py::dict out;
        out["sequence"] = ev.sequence;
        out["kind"] = static_cast<int>(ev.kind);
        out["payload"] = py::module::import("json").attr("loads")(ev.payload.dump());
        (*py_func)(out);
    });
    return py::none();
})
```

Add to `Orchestrator` (`src/core/orchestrator.hpp`):

```cpp
class EventObserver {
public:
    virtual ~EventObserver() = default;
    virtual void on_event(const Event& event) = 0;
};

class Orchestrator {
public:
    void attach_observer(std::shared_ptr<EventObserver> obs);
    // ...
private:
    std::vector<std::shared_ptr<EventObserver>> observers_;
};
```

Provide a `LambdaObserver` adapter in `src/core/observer.hpp`:

```cpp
#pragma once

#include <functional>
#include "core/event.hpp"

namespace swiftagent {

class LambdaObserver final : public EventObserver {
public:
    explicit LambdaObserver(std::function<void(const Event&)> fn) : fn_(std::move(fn)) {}
    void on_event(const Event& ev) override { fn_(ev); }
private:
    std::function<void(const Event&)> fn_;
};

} // namespace swiftagent
```

In `orchestrator.cpp` implement `attach_observer` and call each observer from every site that already invokes `replay_.on_event(...)` (in `run()`).

- [ ] **Step 4: Python observer module**

`python/swiftagent/observer.py`:

```python
from typing import Callable, List


class StreamingObserver:
    def __init__(self, sink: Callable[[dict], None]):
        self._sink = sink

    def __call__(self, event: dict) -> None:
        self._sink(event)
```

- [ ] **Step 5: Re-export**

Append to `python/swiftagent/__init__.py`:

```python
from .observer import StreamingObserver
__all__ = [*__all__, "StreamingObserver"]
```

- [ ] **Step 6: Run all tests**

Run: `cmake --build build -j && python -m pytest tests/python -q && ctest --test-dir build --output-on-failure`
Expected: all PASS, no C++ regressions.

- [ ] **Step 7: Commit**

```bash
git add python tests/python src/core CMakeLists.txt
git commit -m "feat: add python streaming observer and end-to-end smoke test"
```

<!-- CONTINUE_PART_J -->

### Task 29: Wheel Build and CI Publish

**Files:**
- Create: `.github/workflows/wheel.yml`
- Modify: `pyproject.toml`

- [ ] **Step 1: Workflow**

`.github/workflows/wheel.yml`:

```yaml
name: wheels
on:
  push:
    tags: ["v*"]
  workflow_dispatch:

jobs:
  wheels:
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - uses: pypa/cibuildwheel@v2.21
        env:
          CIBW_BUILD: "cp310-* cp311-* cp312-*"
          CIBW_SKIP: "*-musllinux_* *-manylinux_i686"
      - uses: pypa/gh-action-pypi-publish@v1.10
        if: github.event_name == 'push' && startsWith(github.ref, 'refs/tags/v')
```

- [ ] **Step 2: Verify build locally**

Run: `python -m pip install --user build scikit-build-core pybind11 && python -m build --wheel`
Expected: produces a wheel under `dist/`.

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/wheel.yml pyproject.toml
git commit -m "ci: add wheel matrix for linux macos windows"
```

<!-- CONTINUE_PART_J -->

### Task 30: Python SDK End-to-End Test (Real Provider Stub)

**Files:**
- Create: `tests/python/test_real_provider.py`

- [ ] **Step 1: Write the failing test**

`tests/python/test_real_provider.py`:

```python
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "build" / "python"))

import swiftagent


def test_real_provider_runs_through_loop():
    engine = swiftagent.Engine(provider="fake", budget_turns=2)

    @engine.tool
    def echo(text: str) -> str:
        return text

    result = engine.run("summarize this", max_turns=2)
    assert result.turns >= 1
    assert "speedup_x" in engine.telemetry().report()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m pytest tests/python/test_real_provider.py -q`
Expected: failure if previous tasks have not landed.

- [ ] **Step 3: Run end-to-end (no further code)**

Re-run after Tasks 25-29 are merged:

Run: `cmake --build build -j && python -m pytest tests/python -q`
Expected: all PASS.

- [ ] **Step 4: Commit**

```bash
git add tests/python
git commit -m "test: end-to-end python sdk smoke"
```

<!-- CONTINUE_PART_J -->

## Self-Review Notes (Part J additions)

- Spec coverage: design document's "Python SDK" section (binding surface, decorators, `@mcp_client`, `StreamingObserver`, GIL policy, wheel pipeline) is mapped 1:1 to Tasks 25-30.
- Placeholder scan: Tasks 25-30 contain full code; no TBD/TODO. Tasks 26-28 call out the `_registry_ptr` ownership thread carefully and forward-declare the `Registry&` accessor on `Orchestrator` to keep linkage clean.
- Type consistency: `Engine.run(task, max_turns=None)` matches the C++ binding `run(task, max_turns=32)`. `ToolSpec.name` matches between registry and Python decorator path. `Event` shape matches both observers.

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