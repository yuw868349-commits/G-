#!/usr/bin/env bash
# Create the standard SwiftAgent issue labels on GitHub via the API.
# Run once after creating the repo (or whenever you need to reset labels).
#
# Requires: GH_TOKEN / GITHUB_TOKEN env var with repo scope.
set -euo pipefail

REPO="${REPO:-yuw868349-commits/G-}"
API="https://api.github.com/repos/${REPO}/labels"

declare -a LABELS=(
  '{"name":"bug","color":"d73a4a","description":"Something isn'\''t working"}'
  '{"name":"enhancement","color":"a2eeef","description":"New feature or request"}'
  '{"name":"question","color":"d876e3","description":"Further information is requested"}'
  '{"name":"documentation","color":"0075ca","description":"Improvements or additions to docs"}'
  '{"name":"good first issue","color":"7057ff","description":"Good for newcomers"}'
  '{"name":"help wanted","color":"008672","description":"Extra attention is needed"}'
  '{"name":"duplicate","color":"cfd3d7","description":"This issue or pull request already exists"}'
  '{"name":"invalid","color":"e4e669","description":"This doesn'\''t seem right"}'
  '{"name":"wontfix","color":"ffffff","description":"This will not be worked on"}'
  '{"name":"priority: high","color":"b60205","description":"High priority"}'
  '{"name":"priority: medium","color":"fbca04","description":"Medium priority"}'
  '{"name":"priority: low","color":"0e8a16","description":"Low priority"}'
  '{"name":"platform: linux","color":"1d76db","description":"Linux specific"}'
  '{"name":"platform: macos","color":"1d76db","description":"macOS specific"}'
  '{"name":"platform: windows","color":"1d76db","description":"Windows specific"}'
  '{"name":"area: core","color":"5319e7","description":"Core engine"}'
  '{"name":"area: mcp","color":"5319e7","description":"MCP host"}'
  '{"name":"area: python","color":"5319e7","description":"Python SDK"}'
  '{"name":"area: docs","color":"5319e7","description":"Documentation"}'
  '{"name":"area: ci","color":"5319e7","description":"CI / build infrastructure"}'
)

for body in "${LABELS[@]}"; do
  echo "create $(echo "$body" | python3 -c 'import sys,json;print(json.load(sys.stdin)["name"])')"
  curl -sS -X POST \
    -H "Authorization: Bearer ${GH_TOKEN:-${GITHUB_TOKEN}}" \
    -H "Accept: application/vnd.github+json" \
    "$API" \
    -d "$body" >/dev/null || true
done

echo "done"
