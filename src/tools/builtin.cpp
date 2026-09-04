#include "tools/builtin.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace swiftagent {

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

    std::string exec(const std::string& cmd) override {
        std::string full = cmd + " 2>&1";
        FILE* pipe = ::popen(full.c_str(), "r");
        if (!pipe) {
            return "";
        }
        char buf[4096];
        std::string out;
        while (fgets(buf, sizeof(buf), pipe)) {
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

void register_builtin_tools(ToolRegistry& registry,
                            std::shared_ptr<ToolContext> ctx) {
    if (!ctx) {
        ctx = std::make_shared<BasicContext>();
    }
    registry.register_tool(std::make_unique<ReadFileTool>(ctx));
    registry.register_tool(std::make_unique<WriteFileTool>(ctx));
    registry.register_tool(std::make_unique<ShellTool>(ctx));
}

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
        "Run a shell command and capture stdout/stderr.",
        nlohmann::json{
            {"type", "object"},
            {"properties", {
                {"cmd", {{"type", "string"}, {"description", "shell command line"}}}
            }},
            {"required", {"cmd"}}
        },
        {"shell"}
    };
}

ToolResult ShellTool::invoke(const ToolCall& call, ToolContext& ctx) {
    ToolContext& active = ctx_ ? *ctx_ : ctx;
    auto args = nlohmann::json::parse(call.arguments, nullptr, false);
    if (args.is_discarded() || !args.contains("cmd")) {
        return ToolResult{false, nullptr, "missing cmd", {}};
    }
    auto cmd = args["cmd"].get<std::string>();
    auto output = active.exec(cmd);
    return ToolResult{true, nlohmann::json{{"output", output}}, "", {}};
}

} // namespace swiftagent
