# SwiftAgent Design

## Overview

SwiftAgent is a high-performance agent execution engine written in C++23. It accelerates arbitrary LLM agent loops by layering pipelined orchestration, lossless context management, conflict-free parallel tool execution, dependency-aware caching, deterministic replay, adaptive model cascading, and cost telemetry into a single cross-platform runtime.

The engine is framework-agnostic. It integrates with any OpenAI-compatible LLM endpoint and any tool exposed over the Model Context Protocol (MCP).

## Goals

- Reduce end-to-end agent latency through pipelined execution and dependency-aware parallelism.
- Reduce token and monetary cost through context compression with exact recall and model cascading.
- Preserve correctness: no information is lost by compression, no cache returns stale results, no parallel execution corrupts shared state.
- Provide deterministic replay of any run for debugging and evaluation.
- Ship as one codebase on Windows, Linux, and macOS.

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
- Fingerprint: each fact carries a semantic fingerprint (reduced embedding plus extracted key-value pairs). The digest references fingerprints, not text.

Recall:

- The model may call `recall(fingerprint | query)`. Lookup returns the exact original fact block, not a paraphrase.
- Digests are monotonic: hard facts (numbers, paths, identifiers) survive compression by construction.

Error handling: recall misses are logged and never silently answered with a paraphrase.

Invariants:

- Compression never drops a fact from the store.
- The model never sees a fact block that is not present verbatim in the store.

### Tool Executor

Execution policy:

- Build a dependency graph for every tool group: nodes are tool calls, edges are shared resources (file paths, registry keys, environment variables, config scopes, ordering constraints).
- Parallel dispatch all weakly-connected components. Intra-component calls execute sequentially in topological order.
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

- Each small-model output is scored after use (outcome based). Scores feed a per-tier quality table.
- Routing thresholds are recalibrated periodically from the table; the system spends progressively less on chores without measurable quality loss.

### Cache

Keying:

- Query-type calls (no side effects) are cached by input fingerprint.
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

- Unit tests per module: digest recall accuracy, dependency graph analysis, cache invalidation, replay determinism, cascade calibration.
- Determinism test: two replays of one recorded run produce identical output.
- Benchmarks: three canonical tasks (file reorganization, multi-step data gathering, dependency install). Measure against the baseline estimator; assert speedup and savings.
- Cross-platform CI: build and run core tests on Linux, Windows, and macOS.

## Milestones

Stage 1 - Core engine: Orchestrator, Context Manager, Tool Executor, Cache, Telemetry. Deliverable: working speedup benchmark on one platform.

Stage 2 - Quality and portability: semantic recall, cascade closed loop, Platform Layer backends for all three targets.

Stage 3 - Replay and integration: deterministic replay with branching, MCP Host, Web panel.

Each stage ends with a runnable demo and passing tests.

## Risks

- Compression accuracy: mitigated by factual monotonicity and exact recall; recall failure must degrade to a full working-set render, never to paraphrase.
- Parallel side effects: mitigated by preflight graph analysis and snapshot rollback; residual risk confined to unregistered mutating tools.
- Cache staleness: mitigated by dependency-set validation at read; worst case is a recompute, never a wrong answer.
- Scope: the full feature set is large; stages are sequenced so that each stage is independently reviewable.