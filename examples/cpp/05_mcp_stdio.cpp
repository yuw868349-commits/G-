// Spawn a subprocess speaking JSON-RPC on stdio as an MCP server, then
// list the tools the server exposes via the McpHost.
//
// This example expects Python 3 on PATH and the bundled
// `examples/scripts/echo_mcp.py` script to be runnable.
//
// Build: cmake --build build --target example_mcp
// Run:   ./build/example_mcp

#include <iostream>
#include <memory>
#include <string>

#include "mcp/mcp_host.hpp"
#include "tools/registry.hpp"

#if SWIFTAGENT_EXAMPLE_HAVE_POSIX_STDIO
#include "posix_stdio_transport.hpp"
#endif

int main()
{
    using namespace swiftagent;

    ToolRegistry registry;
    McpHost host(registry);

#if SWIFTAGENT_EXAMPLE_HAVE_POSIX_STDIO
    try
    {
        auto transport = std::make_unique<example::PosixStdioTransport>(
            "python3 examples/scripts/echo_mcp.py");
        host.attach_stdio(std::move(transport), "echo");
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
    return 0;
}
