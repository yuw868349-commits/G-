# SwiftAgent Design

## Overview

SwiftAgent is a high-performance agent execution engine written in C++23. It accelerates arbitrary LLM agent loops by layering pipelined orchestration, lossless context management, conflict-free parallel tool execution, dependency-aware caching, deterministic replay, adaptive model cascading, and cost telemetry into a single cross-platform runtime.

The engine is framework-agnostic. It integrates with any OpenAI-compatible LLM endpoint and any tool exposed over the Model Context Protocol (MCP).

The core is C++ for performance. A Python SDK wraps the C++ engine through pybind11 so Python developers can run agents in a few lines with identical behavior and zero IPC overhead.

## Goals

- Reduce end-to-end agent latency through pipelined execution and dependency-aware parallelism.
- Reduce token and monetary cost through context compression with exact recall and model cascading.
- Preserve correctness: no information is lost by compression, no cache returns stale results, no parallel execution corrupts shared state.
- Provide deterministic replay of any run for debugging and evaluation.
- Ship as one codebase on Windows, Linux, and macOS.
- Expose the full engine capability to Python developers through a first-class Python SDK.

## Non-Goals

- Not an agent framework. SwiftAgent does not define agent personas, memory formats, or task templates.
- Not a model training system.
- No GUI shell beyond the provided TUI and Web panel.

## Architecture

```
+--------------------------------------------------------------+
| Access Layer: CLI | TUI | Web Panel                          |
+---------------------------+----------------------------------+
                            |
+---------------------------v----------------------------------+
| Orchestrator - pipelined event loop                          |
| . plan -> act -> reflect cycle                               |
| . progress scoring and strategy switch                       |
| . stall / duplicate / timeout detection                      |
| . graceful degradation chain                                 |
| . budget enforcement with convergence mode                   |
| +------------------------+  +-----------------------------+  |
| | Context Manager        |  | Model Cascade               |  |
| | - fact store (full)    |  | - small model for chores    |  |
| | - fingerprint working  |  | - large model for decisions |  |
| |   set (compressed)     |  | - closed-loop quality       |  |
| | - exact recall lookup  |  |   calibration               |  |
| +------------------------+  +-----------------------------+  |
| +----------------------------------------------------------+ |
| | Tool Executor - preflight scheduler                        | |
| | - resource preplay graph                                  | |
| | - parallel dispatch with ordered conflicts                | |
| | - snapshot rollback on failure                            | |
| | - result validation, retry, blacklist                     | |
| +----------------------------------------------------------+ |
| +----------------------------------------------------------+ |
| | Cache - data lineage tracker                              | |
| | - fingerprints of inputs                                  | |
| | - recorded resource dependencies                          | |
| | - invalidation on dependency change                       | |
| +----------------------------------------------------------+ |
| +----------------------------------------------------------+ |
| | Replay - causal-chain recorder                            | |
| | - event log with causal edges                             | |
| | - deterministic replay without model calls                | |
| | - branch exploration                                      | |
| +----------------------------------------------------------+ |
| +----------------------------------------------------------+ |
| | Telemetry - cost and latency accounting                   | |
| | - per-module metrics                                      | |
| | - baseline comparison                                     | |
| | - exportable report                                       | |
| +----------------------------------------------------------+ |
+---------------------------+----------------------------------+
                            |
        +-------------------+-------------------+
        |                                       |
+-------v--------+                    +---------v---------+
| Platform Layer |                    | LLM Gateway + MCP |
| Win/Linux/Mac  |                    | Host              |
+----------------+                    +-------------------+
```

## Component Specifications

### Orchestrator

Responsibilities:

- Run the pipelined loop. While the model is reasoning, the executor consumes already-ready tool calls and caches their results for the next turn.
- Select the execution form per turn from the measured dependency graph, not from a fixed pipeline:
  - Strongly dependent turns (reasoning consumes the previous tool results) run serially; pipelining is not forced.
  - Independent tool groups run in parallel.
  - Fully independent groups overlap with model reasoning.
  - The form is downgraded and upgraded dynamically as the dependency graph is re-measured each turn.
- Enforce the plan-act-reflect cycle. A turn without reflection is rejected.
- Track progress per turn. If progress does not exceed a threshold for N turns, switch strategy (prompt form, tool set, then model).
- Detect stalls (empty or filler output), duplicates (high intent similarity with the previous turn), and timeouts (per-turn and per-tool hard limits).
- Degrade gracefully: primary model failure falls back to a smaller model, then to a persisted checkpoint handoff for human takeover.
- Enforce budget: when the remaining budget crosses a threshold, switch to convergence mode; at zero budget, stop and emit an interim report.

Interfaces:

- `push_task(TaskDescription)` -> TaskHandle
- Events emitted to Replay and Telemetry through a typed event bus.
- Calls ContextManager to render the working set for each turn.
- Calls ToolExecutor with the dependency group produced by the current turn.
- Calls ModelCascade to choose per-call model routing.

Failure handling: every degradation step is logged with the reason and is part of Replay causality.

### Context Manager

Storage model:

- Fact store: append-only storage of every raw tool result, model message, and derived fact. Content addressed for deduplication.
- Working set: the compact view handed to the model. Contains the current goal, structured digest of history, and the latest tool results. Its token size is kept bounded.
- Fingerprint: two-tier fingerprinting, with exactness carried by hard keys, not by similarity.
  - Hard-key tier: numbers, paths, ports, identifiers, status codes, and key-value pairs are extracted through a schema-driven normalizer. Matching on hard keys is decisive: identical keys hit, mismatched keys miss. No threshold, no similarity, no ambiguity.
  - Soft tier: only free-form descriptive text uses a reduced embedding. A soft hit is always returned with a confidence score.

Recall:

- The model may call `recall(fingerprint | query)`. Lookup returns the exact original fact block, not a paraphrase.
- Two-level recall protocol:
  - Precise mode: a hard-key match returns the verbatim block. This path has no uncertainty by construction.
  - Approximate mode: a soft match is returned only above a confidence threshold, always labeled with its confidence.
  - Degraded mode: below the threshold, recall never fabricates or paraphrases. It re-renders the full digest of the affected region and explicitly marks it as unverified.
- Digests are monotonic: hard facts (numbers, paths, identifiers) survive compression by construction and are always expressed as hard keys.

Error handling: every recall miss or low-confidence hit is logged and routed to the degraded mode; a paraphrase is never returned as a fact.

Invariants:

- Compression never drops a fact from the store.
- The model never sees a fact block that is not present verbatim in the store.
- Every fact returned to the model is either verbatim from the store or explicitly marked unverified. No third category exists.

### Tool Executor

Execution policy:

- Build a dependency graph for every tool group: nodes are tool calls, edges are shared resources (file paths, registry keys, environment variables, config scopes, ordering constraints).
- Side effects are measured, not trusted. Third-party MCP tools do not reliably declare their side effects, so the executor derives the dependency graph from observation:
  - File-tree diff before and after execution (mtime plus content hash).
  - Environment and registry diff before and after execution.
  - The graph uses the observed side-effect set; declared side effects, when present, seed the observation.
- Parallel dispatch all weakly-connected components. Intra-component calls execute sequentially in topological order.
- Conservative tiering for unobservable tools: if observation is incomplete or fails, the call is treated as if it may have mutated anything. It is serialized against all other execution and is excluded from caching. Optionally such tools run inside a sandbox so their real side effects stay inside the observable scope.
- Preflight resource play is deterministic: it runs the same graph analysis before execution, so conflicts are prevented, not repaired.

Failure handling:

- Snapshot-before-mutate for registered mutating tools; on failure, restore the snapshot and mark the attempt failed.
- Validation after every call: exit status, output well-formedness, expected side effects. Failed validations trigger one alternative-strategy retry.
- Persistently rejected strategies are blacklisted for the remainder of the task.

Concurrency: worker pool sized by configuration; all mutations to shared state happen inside the executor under a resource lock derived from the graph.

### Model Cascade

Routing:

- Chores: context digestion, dependency inference, validation, deduplication, progress scoring.
- Decisions: plan formation, complex tool selection, final output synthesis.
- Confidence escalation: no side-effect calls are escalated; only quality-sensitive decisions are.

Closed loop:

- Objective proxy signals replace subjective quality scores, because chores (digestion, deduplication, progress scoring) have no ground truth for direct scoring:
  - Divergence rate: a small-model result that disagrees with a large-model review on the same input counts as one divergence. Routing thresholds are tuned from divergence rate, never from a subjective score.
  - Verifiable features: chores with rule-checkable outcomes (all hard keys retained, duplicates recognized) are scored by automatic rules.
  - Failure coupling: consecutive tool failures caused by a small-model output force this task back to the large model and record the event.
- Safety governor on every small-model exit: any scoring noise or divergence causes routing to fall back to a more conservative tier. Calibration failure can only degrade to more conservative routing; it can never silently lower output quality.

### Cache

Keying:

- Query-type calls (no side effects) are cached by input fingerprint. Eligibility is decided by the executor's observed side-effect set; a call whose side effects were not fully observed is never cached.
- A cache entry records the resource set it depended on: file paths and their mtimes/hashes, config keys, environment variables.

Invalidation:

- Entries are validated at read against their recorded dependencies. Any change invalidates immediately.
- Side-effecting calls are never cached.

Scope: cache entries persist across tasks within a namespace.

### Replay

Recording:

- Every event (model request/response, tool dispatch, result, working-set digest, decision rationale) is appended with a monotonic sequence number and causal edges to the events it consumed.

Replay:

- Deterministic: replay consumes the recorded event stream; no model call, no tool re-execution.
- Supports speed control, breakpoints, and branch exploration: splice a synthetic event at sequence S and re-derive the downstream trace.

Search: queries over event type, tool name, and full-text content.

### Telemetry

- Per-module counters: latency, tokens, cost, and cache hit rate.
- Baseline estimator: simulates the same task on a plain sequential loop to compute speedup and savings.
- Export: task report with route, cost breakdown, and failure history.

### Platform Layer

- Unified interfaces for process spawn, file system, sockets, and clock.
- Implementations: POSIX (Linux/macOS) via fork/exec and kqueue or epoll per platform; Windows via CreateProcess and IOCP-compatible abstraction.
- Network I/O for the Web panel uses a cross-platform async library to avoid three handrolled event loops.

### LLM Gateway and MCP Host

- Gateway: OpenAI-compatible chat completions, multi-endpoint, key rotation, retry-on-failure.
- MCP Host: stdio and SSE transports; tools register into the executor dependency graph automatically.

### Python SDK

A first-class Python SDK exposes the full engine surface so Python developers can adopt SwiftAgent in a few lines, without paying the C++ build cost on day one.

- Built with pybind11; the public ABI is a single `import swiftagent` module.
- A prebuilt wheel is published for Linux, macOS, and Windows on every tagged release. Source distributions build the C++ extension through the same CMake project; users who cannot use the wheel can `pip install swiftagent --no-binary :swiftagent:` and get a working build with no manual steps.
- The SDK mirrors the C++ 1:1 for the parts that matter, and provides idiomatic Python on top:
  - `Engine`, `RunResult`, `Tool`, `ToolRegistry`, `McpHost`, `Replay`, `Telemetry`, `Budget`, `Provider` are the Python names.
  - Tools are plain Python callables annotated with `@tool`. The decorator inspects type hints, builds the JSON schema, registers the callable, and surfaces it to the executor. No boilerplate.
  - A `@mcp_client` decorator adapts an MCP server connection (stdio or SSE) into the same registry, so external MCP tools run through the same engine path.
  - A `StreamingObserver` API lets Python code subscribe to live Replay events for in-process TUI / Web integration.
- Concurrency model: pybind11 bindings are GIL-released on every potentially blocking C++ call (provider I/O, tool execution, file diff, cache read, replay import). Python callbacks that handle events run on a dedicated thread with the GIL reacquired.
- Versioning: the Python module version is kept equal to the engine version, and the wheel ABI is locked per major version. Deprecations go through `DeprecationWarning` for at least one minor cycle.

Example usage:

```python
import swiftagent

engine = swiftagent.Engine(provider="openai", model="gpt-4o-mini", budget_turns=32)

@engine.tool
def read_file(path: str) -> str:
    return open(path).read()

result = engine.run("summarize the latest report in this folder")
print(result.completed, result.turns, result.telemetry.speedup_x)
```

The SDK is the recommended adoption path; the CLI and Web panel remain for shell users and demonstrations.

## Data Flow

1. Access layer submits a task to the Orchestrator.
2. Orchestrator requests a working-set render from Context Manager.
3. Model responds with reasoning and a tool group.
4. Tool Executor preflights, parallelizes, and executes; results are validated and stored.
5. Results write into the Fact Store; cache serves any repeated query immediately.
6. Every step is recorded by Replay with causality and accounted by Telemetry.
7. On stop, Telemetry exports the report; Replay is available for inspection.

## Cross-Platform Strategy

- Core modules are platform-independent; one translation unit set builds on all three targets.
- Platform-specific code is isolated behind the Platform Layer interface.
- CMake selects the backend. C++23 features are used where the standard library suffices; external dependencies are limited to a JSON library, an async network library, and the TUI/Web binding.

## Testing and Benchmark

- Unit tests per module: digest recall accuracy (hard-key precision, soft-confidence labeling, degraded-mode routing), side-effect observation diffs, dependency graph analysis, cache invalidation, replay determinism, cascade calibration and governor fallback.
- Determinism test: two replays of one recorded run produce identical output.
- Side-effect test: a tool with undocumented mutations changes files; the executor must detect them via observation and adjust parallelism and cache eligibility accordingly.
- Benchmarks: three canonical tasks (file reorganization, multi-step data gathering, dependency install). Measure against the baseline estimator; report speedup separately for serial-dominant and tool-intensive workloads rather than claiming a single number.
- Cross-platform CI: build and run core tests on Linux, Windows, and macOS.

## Milestones

Stage 1 - Core engine: Orchestrator, Context Manager, Tool Executor, Cache, Telemetry. Deliverable: working speedup benchmark on one platform.

Stage 2 - Quality and portability: semantic recall, cascade closed loop, Platform Layer backends for all three targets.

Stage 3 - Replay and integration: deterministic replay with branching, MCP Host, Web panel.

Stage 4 - Python SDK: pybind11 bindings, `Engine` / `RunResult` / `Telemetry` / `Replay` / `McpHost` surface, `@tool` and `@mcp_client` decorators, GIL-safe release on blocking paths, prebuilt wheels for the three target platforms, end-to-end Python test that runs a real task.

Each stage ends with a runnable demo and passing tests.

## Risks

- Compression accuracy: mitigated by two-tier fingerprinting (decisive hard keys, confidence-labeled soft tier) and a mandatory degraded mode that re-renders rather than paraphrases. Soft matching is never allowed to fabricate a fact.
- Parallel side effects: mitigated by measuring side effects via file/env diffs instead of trusting declarations, conservative serialization and cache exclusion for unobservable tools, and snapshot rollback. Residual risk is limited to tools whose mutations escape observation entirely; correctness degrades to serial execution, never to corruption.
- Cache staleness: mitigated by dependency-set validation at read and cache eligibility gated on fully observed side effects; worst case is a recompute, never a wrong answer.
- Cascade calibration noise: scoring noise can only move routing toward a more conservative tier; a safety governor reverts any chore to the large model on divergence or repeated failure.
- Pipelining benefit is workload-dependent: speedup is only claimed and measured for tool-intensive, weakly dependent workloads; serial-dominant workloads are measured and reported honestly.
- Python binding GIL: every blocking C++ call releases the GIL, and Python callbacks for events run on a dedicated thread. The wheel ABI is pinned per major version to keep breakage rare.
- Scope: the full feature set is large; stages are sequenced so that each stage is independently reviewable.