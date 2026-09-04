// Spawn a subprocess speaking MCP over stdio, attach to it via McpHost,
// list the tools, and call one of them. Demonstrates end-to-end MCP
// integration: full handshake (initialize + notifications/initialized),
// tools/list, and tools/call.
//
// This example expects Python 3 on PATH and the bundled
// `examples/scripts/file_mcp.py` script to be runnable. The real MCP
// Python SDK v2.x is used; install it with `pip install "mcp[cli]"`.
//
// Build: cmake --build build --target example_mcp
// Run:   ./build/example_mcp [python-script]
//        default script: examples/scripts/file_mcp.py

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "core/types.hpp"
#include "mcp/mcp_host.hpp"
#include "tools/registry.hpp"

#if PRAXIS_EXAMPLE_HAVE_POSIX_STDIO
#include "posix_stdio_transport.hpp"
#endif

class StdoutContext final : public praxis::ToolContext
{
public:
    [[nodiscard]] std::string read_file(const std::string& path) override {
        (void)path;
        return {};
    }
    [[nodiscard]] std::string exec(const std::string& cmd) override {
        (void)cmd;
        return {};
    }
    [[nodiscard]] bool file_exists(const std::string& path) const override {
        (void)path;
        return false;
    }
};

int main(int argc, char** argv)
{
    using namespace praxis;

    std::string script = "examples/scripts/file_mcp.py";
    if (argc > 1) {
        script = argv[1];
    }

    ToolRegistry registry;
    McpHost host(registry);

#if PRAXIS_EXAMPLE_HAVE_POSIX_STDIO
    try
    {
        auto transport = std::make_unique<example::PosixStdioTransport>(
            "python3 " + script);
        host.attach_stdio(std::move(transport), "file");
    }
    catch (const std::exception& e)
    {
        std::cerr << "mcp attach failed: " << e.what() << "\n";
        return 1;
    }
#else
    std::cerr << "this example requires a POSIX-like OS\n";
    return 1;
#endif

    std::cout << "registered " << host.tools().size() << " tool(s):\n";
    for (const auto& name : host.tools())
    {
        std::cout << "  - " << name << "\n";
    }

    // Call the list_dir tool on the project root and print the output.
    auto* tool = registry.find("file__list_dir");
    if (tool == nullptr) {
        std::cerr << "tool not found\n";
        return 2;
    }
    ToolCall call;
    call.name = "file__list_dir";
    call.arguments = R"({"path":"."})";
    StdoutContext ctx;
    auto result = tool->invoke(call, ctx);
    if (!result.ok) {
        std::cerr << "tool call failed: " << result.error_message << "\n";
        return 3;
    }
    if (result.output.is_string()) {
        std::cout << "list_dir output:\n" << result.output.get<std::string>() << "\n";
    } else {
        std::cout << "list_dir output: " << result.output.dump() << "\n";
    }

    // Round-trip a write/read to verify the full call/response cycle.
    auto* write_tool = registry.find("file__write_file");
    auto* read_tool = registry.find("file__read_file");
    if (write_tool != nullptr && read_tool != nullptr) {
        ToolCall write_call;
        write_call.name = "file__write_file";
        write_call.arguments =
            R"({"path":"/tmp/praxis_mcp_probe.txt","content":"hello from c++ via mcp\n"})";
        auto write_result = write_tool->invoke(write_call, ctx);
        if (!write_result.ok) {
            std::cerr << "write_file failed: " << write_result.error_message << "\n";
            return 4;
        }
        std::cout << "write_file: " << write_result.output.dump() << "\n";

        ToolCall read_call;
        read_call.name = "file__read_file";
        read_call.arguments = R"({"path":"/tmp/praxis_mcp_probe.txt"})";
        auto read_result = read_tool->invoke(read_call, ctx);
        if (!read_result.ok) {
            std::cerr << "read_file failed: " << read_result.error_message << "\n";
            return 5;
        }
        std::cout << "read_file: " << read_result.output.dump() << "\n";
    }
    return 0;
}
