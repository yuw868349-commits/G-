#!/usr/bin/env python3
"""Echo MCP server: a minimal JSON-RPC server over stdio.

Speaks a tiny subset of the Model Context Protocol so the
`example_mcp_stdio` C++ binary can attach and read its tools list.
"""

import json
import sys


TOOLS = [
    {
        "name": "echo",
        "description": "Echo back whatever is in `text`.",
        "inputSchema": {
            "type": "object",
            "properties": {"text": {"type": "string"}},
            "required": ["text"],
        },
    },
    {
        "name": "reverse",
        "description": "Reverse the string in `text`.",
        "inputSchema": {
            "type": "object",
            "properties": {"text": {"type": "string"}},
            "required": ["text"],
        },
    },
]


def handle(request):
    method = request.get("method", "")
    req_id = request.get("id")

    if method == "tools/list":
        return {"id": req_id, "result": {"tools": TOOLS}}
    if method.startswith("tools/call"):
        return {
            "id": req_id,
            "error": {"code": -32601, "message": "tool call not implemented in echo server"},
        }
    if method == "initialize":
        return {"id": req_id, "result": {"protocolVersion": "0.1.0"}}

    return {"id": req_id, "error": {"code": -32601, "message": f"unknown method: {method}"}}


def main():
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            request = json.loads(raw)
        except json.JSONDecodeError:
            continue
        response = handle(request)
        sys.stdout.write(json.dumps(response) + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
