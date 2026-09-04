#include "tools/builtin.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

namespace swiftagent {

// The default fork() returns a fresh StatelessContext.  This is
// only invoked from tool contexts that have not been updated to
// override fork() with a real copy of themselves; the abstract
// class cannot have a body for fork() inline because the helper
// type's overrides would not be visible until the class is closed.
std::unique_ptr<ToolContext> ToolContext::fork() const {
    return std::make_unique<StatelessContext>();
}

std::string StatelessContext::read_file(const std::string& path) {
    (void)path;
    return {};
}

std::string StatelessContext::exec(const std::string& cmd) {
    (void)cmd;
    return {};
}

bool StatelessContext::file_exists(const std::string& path) const {
    (void)path;
    return false;
}

namespace {

class BasicContext : public ToolContext {
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

    // The legacy `exec(cmd)` entry point remains for backward
    // compatibility but is intentionally unimplemented on POSIX: it
    // would require a shell and exposes injection.  Use
    // `exec_argv()` instead, which goes straight to execvp.
    std::string exec(const std::string& /*cmd*/) override {
        return "ToolContext::exec() is unsafe and disabled; "
               "use exec_argv() with an explicit argv vector instead.";
    }

    bool file_exists(const std::string& path) const override {
        return std::filesystem::exists(path);
    }

    // BasicContext is intentionally stateless on POSIX (the only
    // state is the OS file system), so a fork() returns another
    // stateless instance of the same type.  Parallel tool calls
    // therefore never race on shared state.
    [[nodiscard]] std::unique_ptr<ToolContext> fork() const override {
        return std::make_unique<BasicContext>();
    }

    std::string exec_argv(const std::string& program,
                          const std::vector<std::string>& args) override {
#if defined(__unix__) || defined(__APPLE__)
        // Tool execution must honour a hard timeout: a runaway
        // child cannot be allowed to wedge the agent.  Use poll()
        // over the read end of the pipe and SIGTERM/SIGKILL the
        // child if it overruns.  A blocking read() would never
        // observe the deadline when the child produces no output.
        constexpr std::chrono::milliseconds kDefaultTimeout{30'000};
        constexpr int kGraceTicks = 20;             // 20 * 50 ms = 1 s
        constexpr long kGraceNs = 50'000'000;        // 50 ms

        int pipefd[2];
        if (::pipe(pipefd) != 0) {
            return "";
        }
        pid_t pid = ::fork();
        if (pid < 0) {
            ::close(pipefd[0]);
            ::close(pipefd[1]);
            return "";
        }
        if (pid == 0) {
            ::dup2(pipefd[1], STDOUT_FILENO);
            ::dup2(pipefd[1], STDERR_FILENO);
            ::close(pipefd[0]);
            ::close(pipefd[1]);
            std::vector<std::string> storage;
            storage.reserve(args.size() + 1);
            storage.push_back(program);
            for (const auto& a : args) {
                storage.push_back(a);
            }
            std::vector<char*> argv;
            argv.reserve(storage.size() + 1);
            for (auto& s : storage) {
                argv.push_back(s.data());
            }
            argv.push_back(nullptr);
            ::execvp(program.c_str(), argv.data());
            ::_exit(127);
        }
        ::close(pipefd[1]);

        std::string out;
        char buf[4096];
        auto deadline = std::chrono::steady_clock::now() + kDefaultTimeout;
        struct pollfd pfd{};
        pfd.fd = pipefd[0];
        pfd.events = POLLIN;
        bool timed_out = false;

        for (;;) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                timed_out = true;
                break;
            }
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  deadline - now)
                                  .count();
            if (remaining < 1) remaining = 1;
            int rc = ::poll(&pfd, 1, static_cast<int>(remaining));
            if (rc < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (rc == 0) {
                continue;
            }
            if (pfd.revents & (POLLIN | POLLHUP)) {
                ssize_t n = ::read(pipefd[0], buf, sizeof(buf));
                if (n > 0) {
                    out.append(buf, static_cast<std::size_t>(n));
                    continue;
                }
                break;
            }
            if (pfd.revents & (POLLERR | POLLNVAL)) {
                break;
            }
        }

        if (timed_out) {
            out += "\n[swiftagent] tool execution exceeded the 30s deadline; terminating child\n";
            ::kill(pid, SIGTERM);
            for (int i = 0; i < kGraceTicks; ++i) {
                int status = 0;
                pid_t r = ::waitpid(pid, &status, WNOHANG);
                if (r == pid) {
                    pid = -1;
                    break;
                }
                struct timespec ts{0, kGraceNs};
                ::nanosleep(&ts, nullptr);
            }
            if (pid > 0) {
                ::kill(pid, SIGKILL);
                int status = 0;
                while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
                }
            }
        } else {
            int status = 0;
            while (::waitpid(pid, &status, 0) < 0) {
                if (errno != EINTR) {
                    break;
                }
            }
        }
        ::close(pipefd[0]);
        return out;
#else
        // On non-POSIX platforms fall back to the unsafe default
        // implementation in the base class.
        return ToolContext::exec_argv(program, args);
#endif
    }
};

} // namespace

void register_builtin_tools(ToolRegistry& registry,
                            std::shared_ptr<ToolContext> ctx) {
    if (!ctx) {
        ctx = std::make_shared<BasicContext>();
    }
    registry.register_tool(std::make_unique<ReadFileTool>(ctx));
    registry.register_tool(std::make_unique<WriteFileTool>(ctx));
    registry.register_tool(std::make_unique<ShellTool>(ctx));
}

namespace {

// Pull a single string field out of a JSON-encoded arguments blob.
// Returns an empty string if the blob is malformed or the field is
// absent, so the resulting resource key simply degrades to the
// static prefix when callers mis-issue a call.
std::string arg_string(const std::string& arguments, const std::string& key) {
    auto args = nlohmann::json::parse(arguments, nullptr, false);
    if (args.is_discarded() || !args.is_object() || !args.contains(key)) {
        return {};
    }
    if (!args[key].is_string()) {
        return {};
    }
    return args[key].get<std::string>();
}

}  // namespace

ReadFileTool::ReadFileTool(std::shared_ptr<ToolContext> ctx)
    : ctx_(std::move(ctx)) {}

ToolDescriptor ReadFileTool::descriptor() const {
    return ToolDescriptor{
        "read_file",
        "Read a UTF-8 text file and return its contents.",
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "absolute or relative file path"}}}
            }},
            {"required", {"path"}}
        },
        {"file:read"}
    };
}

std::vector<std::string>
ReadFileTool::resources_for(const ToolCall& call) const {
    auto path = arg_string(call.arguments, "path");
    if (path.empty()) {
        return {"file:read:*"};
    }
    return {"file:read:" + path};
}

ToolResult ReadFileTool::invoke(const ToolCall& call, ToolContext& ctx) {
    ToolContext& active = ctx_ ? *ctx_ : ctx;
    auto args = nlohmann::json::parse(call.arguments, nullptr, false);
    if (args.is_discarded() || !args.contains("path")) {
        return ToolResult{false, nullptr, "missing path", {}};
    }
    auto path = args["path"].get<std::string>();
    auto content = active.read_file(path);
    if (content.empty() && !active.file_exists(path)) {
        return ToolResult{false, nullptr, "file not found: " + path, {}};
    }
    return ToolResult{true, nlohmann::json{{"path", path}, {"content", content}}, "", {path}};
}

WriteFileTool::WriteFileTool(std::shared_ptr<ToolContext> ctx)
    : ctx_(std::move(ctx)) {}

ToolDescriptor WriteFileTool::descriptor() const {
    return ToolDescriptor{
        "write_file",
        "Write a UTF-8 text file. Creates or overwrites.",
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "absolute or relative file path"}}},
                {"content", {{"type", "string"}, {"description", "text to write"}}}
            }},
            {"required", {"path", "content"}}
        },
        {"file:write"}
    };
}

std::vector<std::string>
WriteFileTool::resources_for(const ToolCall& call) const {
    auto path = arg_string(call.arguments, "path");
    if (path.empty()) {
        return {"file:write:*"};
    }
    // A write to a path is also an implicit read of the path for the
    // orchestrator's dependency-tracking purposes: anything else that
    // runs in the same group and touches the same path would race the
    // write.
    return {"file:read:" + path, "file:write:" + path};
}

ToolResult WriteFileTool::invoke(const ToolCall& call, ToolContext&) {
    auto args = nlohmann::json::parse(call.arguments, nullptr, false);
    if (args.is_discarded() || !args.contains("path") || !args.contains("content")) {
        return ToolResult{false, nullptr, "missing path/content", {}};
    }
    auto path = args["path"].get<std::string>();
    auto content = args["content"].get<std::string>();
    if (auto parent = std::filesystem::path(path).parent_path();
        !parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return ToolResult{false, nullptr, "cannot open for write: " + path, {}};
    }
    out << content;
    if (!out) {
        return ToolResult{false, nullptr, "write failed: " + path, {}};
    }
    return ToolResult{true, nlohmann::json{{"path", path}, {"bytes", content.size()}}, "", {path}};
}

ShellTool::ShellTool(std::shared_ptr<ToolContext> ctx)
    : ctx_(std::move(ctx)) {}

ToolDescriptor ShellTool::descriptor() const {
    return ToolDescriptor{
        "shell",
        "Run a program with a fixed argv vector. Arguments are passed "
        "verbatim to execvp / CreateProcess, never re-parsed by a shell, "
        "so single quotes and shell metacharacters in arguments cannot "
        "cause injection.",
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"program", {{"type", "string"},
                              {"description", "executable to invoke"}}},
                {"args", {{"type", "array"},
                          {"items", {{"type", "string"}}},
                          {"description", "argv vector; the program is argv[0]"}}}
            }},
            {"required", {"program", "args"}}
        },
        {"shell"}
    };
}

ToolResult ShellTool::invoke(const ToolCall& call, ToolContext& ctx) {
    ToolContext& active = ctx_ ? *ctx_ : ctx;
    auto args = nlohmann::json::parse(call.arguments, nullptr, false);
    if (args.is_discarded() || !args.contains("program") || !args.contains("args")) {
        return ToolResult{false, nullptr, "missing program/args", {}};
    }
    if (!args["program"].is_string() || !args["args"].is_array()) {
        return ToolResult{false, nullptr, "program must be a string and args an array", {}};
    }
    std::string program = args["program"].get<std::string>();
    std::vector<std::string> argv;
    argv.reserve(args["args"].size());
    for (const auto& a : args["args"]) {
        if (!a.is_string()) {
            return ToolResult{false, nullptr, "every entry of args must be a string", {}};
        }
        argv.push_back(a.get<std::string>());
    }
    auto output = active.exec_argv(program, argv);
    return ToolResult{true, nlohmann::json{{"output", output}}, "", {}};
}

} // namespace swiftagent
