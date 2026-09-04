#!/usr/bin/env python3
"""File MCP server.

A small, dependency-light implementation of the Model Context Protocol
that speaks over newline-delimited JSON-RPC on stdio. It exposes three
tools to a parent process:

  * list_dir(path)   - list entries in a directory
  * read_file(path)  - return a file's contents
  * write_file(path, content) - write content to a file

The wire format is the standard MCP ``tools/list`` and ``tools/call``
RPCs, so any MCP-compatible client (including the C++ ``McpHost``) can
attach to it. The script does not use the official ``mcp`` Python SDK
because that SDK re-routes file descriptors 0 and 1 on startup, which
breaks pipe-based parent processes.

Run:
    python3 examples/scripts/file_mcp.py
"""

from __future__ import annotations

import json
import os
import sys
import threading
from pathlib import Path
from queue import Empty, Queue
from typing import Any


SERVER_INFO = {"name": "file-mcp", "version": "0.1.0"}
PROTOCOL_VERSION = "2024-11-05"


def _ok(req_id: Any, result: Any) -> dict[str, Any]:
    return {"jsonrpc": "2.0", "id": req_id, "result": result}


def _err(req_id: Any, code: int, message: str) -> dict[str, Any]:
    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "error": {"code": code, "message": message},
    }


def _tool_result(text: str, is_error: bool = False) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "content": [{"type": "text", "text": text}],
        "isError": is_error,
    }
    return payload


def _list_dir(arguments: dict[str, Any]) -> dict[str, Any]:
    p = Path(arguments.get("path", "."))
    if not p.exists() or not p.is_dir():
        return _tool_result(f"error: not a directory: {p}", is_error=True)
    entries = sorted(os.listdir(p))
    return _tool_result("\n".join(entries) if entries else "(empty)")


def _read_file(arguments: dict[str, Any]) -> dict[str, Any]:
    p = Path(arguments.get("path", ""))
    if not p.exists() or not p.is_file():
        return _tool_result(f"error: not a file: {p}", is_error=True)
    return _tool_result(p.read_text(encoding="utf-8"))


def _write_file(arguments: dict[str, Any]) -> dict[str, Any]:
    p = Path(arguments.get("path", ""))
    content = arguments.get("content", "")
    if not p:
        return _tool_result("error: missing path", is_error=True)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(content, encoding="utf-8")
    return _tool_result(f"wrote {len(content)} bytes to {p}")


TOOLS: list[dict[str, Any]] = [
    {
        "name": "list_dir",
        "description": "List entries in a directory.",
        "inputSchema": {
            "type": "object",
            "properties": {"path": {"type": "string"}},
            "required": ["path"],
        },
    },
    {
        "name": "read_file",
        "description": "Read a UTF-8 text file and return its contents.",
        "inputSchema": {
            "type": "object",
            "properties": {"path": {"type": "string"}},
            "required": ["path"],
        },
    },
    {
        "name": "write_file",
        "description": "Write a UTF-8 text file.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {"type": "string"},
                "content": {"type": "string"},
            },
            "required": ["path", "content"],
        },
    },
]

DISPATCH: dict[str, Any] = {
    "list_dir": _list_dir,
    "read_file": _read_file,
    "write_file": _write_file,
}


def handle(request: dict[str, Any]) -> dict[str, Any] | None:
    method = request.get("method", "")
    req_id = request.get("id")

    if method == "initialize":
        return _ok(
            req_id,
            {
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {"tools": {"listChanged": False}},
                "serverInfo": SERVER_INFO,
            },
        )

    if method == "ping":
        return _ok(req_id, {})

    if method == "tools/list":
        return _ok(req_id, {"tools": TOOLS})

    if method == "tools/call":
        params = request.get("params") or {}
        name = params.get("name", "")
        arguments = params.get("arguments") or {}
        fn = DISPATCH.get(name)
        if fn is None:
            return _err(req_id, -32601, f"unknown tool: {name}")
        try:
            result = fn(arguments)
        except Exception as exc:  # noqa: BLE001
            return _err(req_id, -32000, f"{type(exc).__name__}: {exc}")
        return _ok(req_id, result)

    # Notifications have no id; ignore them silently.
    if req_id is None:
        return None

    return _err(req_id, -32601, f"unknown method: {method}")


def _reader_loop(q: "Queue[str]") -> None:
    """Read newline-delimited JSON from stdin and queue it for the main loop."""
    for raw in sys.stdin:
        q.put(raw)
    q.put("")  # sentinel for EOF


def main() -> None:
    q: "Queue[str]" = Queue()
    threading.Thread(target=_reader_loop, args=(q,), daemon=True).start()

    while True:
        try:
            line = q.get(timeout=0.1)
        except Empty:
            continue
        if not line:
            return
        text = line.strip()
        if not text:
            continue
        try:
            request = json.loads(text)
        except json.JSONDecodeError:
            continue
        if not isinstance(request, dict):
            continue
        response = handle(request)
        if response is None:
            continue
        sys.stdout.write(json.dumps(response) + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
