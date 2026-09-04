// Intentionally minimal: this file lives in the `tools` module and
// depends on nothing except the public `core` types. `ToolRegistry`
// is declared in `core/registry.hpp` so the `core` library can be used
// without pulling in the `tools` library. The `tools` library, in turn,
// depends on `core` and provides the built-in tools.
#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include "core/types.hpp"

namespace praxis {

class ToolCall;
class ToolResult;
class ToolContext;

using ToolCall = praxis::ToolCall;

struct ToolDescriptor {
    std::string name;
    std::string description;
    nlohmann::json schema;
    std::vector<std::string> declared_resources;
};

class Tool {
public:
    virtual ~Tool() = default;
    [[nodiscard]] virtual ToolDescriptor descriptor() const = 0;
    [[nodiscard]] virtual ToolResult invoke(const ToolCall& call, ToolContext& ctx) = 0;

    // Extract the *concrete* resource keys touched by a call so the
    // dependency graph can decide whether two calls in the same turn
    // may run in parallel.  The default implementation returns the
    // tool's statically-declared resources, which means two calls of
    // the same tool always conflict.  Tools that touch addressable
    // resources (file paths, URLs, etc.) should override this to
    // return keys that include those addresses.
    [[nodiscard]] virtual std::vector<std::string>
    resources_for(const ToolCall& call) const;
};

struct ToolResult {
    bool ok{true};
    nlohmann::json output;
    std::string error_message;
    std::vector<std::string> observed_resources;
};

class ToolContext {
public:
    virtual ~ToolContext() = default;
    [[nodiscard]] virtual std::string read_file(const std::string& path) = 0;
    [[nodiscard]] virtual std::string exec(const std::string& cmd) = 0;
    [[nodiscard]] virtual bool file_exists(const std::string& path) const = 0;

    // Preferred exec entry point: run `program` with a fixed argv
    // vector.  Implementations should pass the argv directly to
    // execvp() (POSIX) or CreateProcess() (Windows) so that arguments
    // containing whitespace, single quotes, or shell metacharacters
    // are never re-parsed by a shell.  The default implementation
    // falls back to `exec()` joined with single-quote wrapping, which
    // is *unsafe*; concrete contexts that want shell-like composition
    // should override this.  Callers that care about safety should
    // pass arguments through this method rather than building a
    // command-line string.
    [[nodiscard]] virtual std::string
    exec_argv(const std::string& program,
              const std::vector<std::string>& args) {
        // Last-resort fallback: single-quote each argument and
        // re-join.  Not safe in general; the implementation is here
        // only to keep the abstract class usable from contexts that
        // haven't been updated yet.
        std::string joined;
        joined.reserve(program.size() + 16);
        joined += '\'';
        for (char c : program) {
            joined += (c == '\'') ? "'\"'\"'" : std::string(1, c);
        }
        joined += '\'';
        for (const auto& a : args) {
            joined += " '";
            for (char c : a) {
                joined += (c == '\'') ? "'\"'\"'" : std::string(1, c);
            }
            joined += '\'';
        }
        return exec(joined);
    }

    // The orchestrator's parallel executor calls this once per worker
    // before invoking a tool.  Returning a fresh, independent copy
    // ensures each parallel tool call observes its own state instead
    // of racing on a shared instance.  The default implementation
    // returns a new empty context; concrete subclasses that own
    // per-call state (working directory, environment, accumulated
    // output, ...) MUST override this to return a real copy of
    // *this*.
    //
    // After this method is called the returned instance is the sole
    // owner of the state visible to that tool call; the original
    // instance continues to be usable for serial tool calls and is
    // *not* accessed by the parallel worker.
    [[nodiscard]] virtual std::unique_ptr<ToolContext> fork() const;
};

// Minimal context that satisfies the interface but has no shared
// state.  Used as the default for fork() so that the abstract
// class remains usable.
class StatelessContext final : public ToolContext {
public:
    [[nodiscard]] std::string read_file(const std::string& path) override;
    [[nodiscard]] std::string exec(const std::string& cmd) override;
    [[nodiscard]] bool file_exists(const std::string& path) const override;
};

} // namespace praxis
