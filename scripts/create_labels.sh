#!/usr/bin/env bash
# Create the standard Praxis issue labels on GitHub via the API.
# Run once after creating the repo (or whenever you need to reset labels).
# Idempotent: uses PATCH (update) first, falls back to POST (create).
#
# Requires: GH_TOKEN / GITHUB_TOKEN env var with repo scope.
set -euo pipefail

# Auth shim: if `gh` is not authenticated but a token is in the
# environment, log gh in with it before doing anything else.
if ! gh auth status >/dev/null 2>&1; then
    if [ -n "${GH_TOKEN:-${GITHUB_TOKEN:-}}" ]; then
        echo ">>> gh not authenticated; logging in from GH_TOKEN env"
        echo "${GH_TOKEN:-${GITHUB_TOKEN}}" | gh auth login --with-token >/dev/null
    else
        echo "error: not authenticated." >&2
        echo "  Either run 'gh auth login' or set GH_TOKEN / GITHUB_TOKEN." >&2
        exit 1
    fi
fi

REPO="${REPO:-$(gh repo view --json nameWithOwner -q .nameWithOwner 2>/dev/null || echo yuw868349-commits/praxis)}"
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

auth_token() {
    gh auth token 2>/dev/null \
        || echo "${GH_TOKEN:-${GITHUB_TOKEN:-}}"
}

for body in "${LABELS[@]}"; do
    name=$(echo "$body" | python3 -c 'import sys,json;print(json.load(sys.stdin)["name"])')
    # GitHub's labels URL must be URL-encoded.  Names with spaces,
    # colons, etc. need that to work, so do it unconditionally.
    encoded_name=$(python3 -c "import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=''))" "$name")
    echo "upsert $name"
    status=$(curl -sS -o /dev/null -w "%{http_code}" -X PATCH \
        -H "Authorization: Bearer $(auth_token)" \
        -H "Accept: application/vnd.github+json" \
        "${API}/${encoded_name}" \
        -d "$body" || echo "000")
    if [ "$status" != "200" ]; then
        curl -sS -o /dev/null -X POST \
            -H "Authorization: Bearer $(auth_token)" \
            -H "Accept: application/vnd.github+json" \
            "$API" \
            -d "$body" || true
    fi
done

echo "done"
