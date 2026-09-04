#!/usr/bin/env bash
# Setup GitHub repository metadata for the Praxis rename.
#
# Prerequisites:
#   gh auth login   (one-time, interactive)
#
# What this script does:
#   1. Renames the existing repository from `swift-agent` to `praxis`
#      (or creates a fresh `praxis` repo if the source repo doesn't
#      exist).
#   2. Updates the repo description, homepage, and topics.
#   3. Pushes the local main branch to the new remote.
#   4. Creates a `v0.1.0` tag and a GitHub release.
#
# Idempotent: re-running is safe; existing fields are overwritten
# in place.

set -euo pipefail

ORG="yuw868349-commits"
OLD_NAME="swift-agent"
NEW_NAME="praxis"

# Detect which token / auth mode the user has.
if ! gh auth status >/dev/null 2>&1; then
    echo "error: not authenticated.  Run 'gh auth login' first." >&2
    exit 1
fi

# Step 1: rename or create the repo.
if gh repo view "${ORG}/${OLD_NAME}" >/dev/null 2>&1; then
    echo ">>> renaming ${ORG}/${OLD_NAME} -> ${ORG}/${NEW_NAME}"
    gh repo rename "${NEW_NAME}" --repo "${ORG}/${OLD_NAME}"
elif gh repo view "${ORG}/${NEW_NAME}" >/dev/null 2>&1; then
    echo ">>> repo ${ORG}/${NEW_NAME} already exists, skipping rename"
else
    echo ">>> creating ${ORG}/${NEW_NAME}"
    gh repo create "${ORG}/${NEW_NAME}" \
        --public \
        --description "Praxis — a C++23 agent execution engine. Turns plans into executed tool calls." \
        --homepage "https://github.com/${ORG}/${NEW_NAME}#readme"
fi

# Step 2: update description, homepage, and topics.
echo ">>> updating description / homepage / topics"
gh repo edit "${ORG}/${NEW_NAME}" \
    --description "Praxis — a C++23 agent execution engine. Turns plans into executed tool calls." \
    --homepage "https://github.com/${ORG}/${NEW_NAME}#readme" \
    --add-topic agent \
    --add-topic llm \
    --add-topic mcp \
    --add-topic cpp \
    --add-topic cpp23 \
    --add-topic orchestrator \
    --add-topic tool-use \
    --add-topic python \
    --delete-branch false \
    --enable-issues \
    --enable-projects false \
    --enable-wiki false

# Step 3: ensure the local main branch points at the new remote and
# the new remote is the upstream.
echo ">>> setting upstream to ${ORG}/${NEW_NAME}"
git remote set-url origin "https://github.com/${ORG}/${NEW_NAME}.git"
git branch --set-upstream-to=origin/main main 2>/dev/null || true

# Step 4: push main (only if not already present upstream).
if ! git ls-remote --heads origin main | grep -q main; then
    echo ">>> pushing main to ${ORG}/${NEW_NAME}"
    git push -u origin main
else
    echo ">>> main already present on remote, skipping push"
fi

# Step 5: create / refresh the v0.1.0 release.
if ! gh release view "v0.1.0" --repo "${ORG}/${NEW_NAME}" >/dev/null 2>&1; then
    echo ">>> creating v0.1.0 tag and release"
    if ! git ls-remote --tags origin v0.1.0 | grep -q v0.1.0; then
        git tag -a v0.1.0 -m "v0.1.0" HEAD
        git push origin v0.1.0
    fi
    gh release create "v0.1.0" \
        --repo "${ORG}/${NEW_NAME}" \
        --title "Praxis v0.1.0" \
        --notes "First public release of **Praxis** — a C++23 cross-platform agent execution engine.

Highlights:
- Plan-act-reflect loop with budget enforcement
- Built-in tools: \`read_file\`, \`write_file\`, \`shell\` (execvp-backed, no shell injection)
- MCP host: stdio and SSE transports
- Model cascade: chore / decision roles with divergence-based escalation
- Cache, replay, telemetry, and side-effect observation
- Python SDK (pybind11)
- CLI and Web panel (default-bound to loopback, optional basic-auth)
- Linux / macOS / Windows" \
        --target main
fi

echo
echo "done.  next:"
echo "  - https://github.com/${ORG}/${NEW_NAME}"
echo "  - https://github.com/${ORG}/${NEW_NAME}/settings  (description / topics verified)"
echo "  - https://github.com/${ORG}/${NEW_NAME}/releases/tag/v0.1.0"
