#!/usr/bin/env bash
# Build a source tarball that matches the Debian package layout.
# Usage: ./packaging/deb/build.sh <version>
set -euo pipefail

VERSION="${1:-0.1.0}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/swiftagent-${VERSION}/DEBIAN"
mkdir -p "$WORK/swiftagent-${VERSION}/usr/local/bin"
mkdir -p "$WORK/swiftagent-${VERSION}/usr/local/include/swiftagent"
mkdir -p "$WORK/swiftagent-${VERSION}/usr/local/share/swiftagent/examples"
mkdir -p "$WORK/swiftagent-${VERSION}/usr/local/share/doc/swiftagent"
mkdir -p "$WORK/swiftagent-${VERSION}/usr/local/lib/pkgconfig"

cmake -S "$ROOT" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/build" -j

cp "$WORK/build/swiftagent" "$WORK/swiftagent-${VERSION}/usr/local/bin/"
cp -r "$ROOT/src/core" "$ROOT/src/llm" "$ROOT/src/mcp" "$ROOT/src/platform" \
      "$ROOT/src/tools" "$ROOT/src/ui" \
      "$WORK/swiftagent-${VERSION}/usr/local/include/swiftagent/"
cp "$ROOT/LICENSE" "$WORK/swiftagent-${VERSION}/usr/local/share/doc/swiftagent/copyright"
cp "$ROOT/README.md" "$WORK/swiftagent-${VERSION}/usr/local/share/doc/swiftagent/"

cat >"$WORK/swiftagent-${VERSION}/DEBIAN/control" <<EOF
Package: swiftagent
Version: ${VERSION}
Section: devel
Priority: optional
Architecture: amd64
Depends: libstdc++6 (>= 12), libc6 (>= 2.36)
Maintainer: SwiftAgent Contributors <noreply@example.com>
Description: C++23 agent execution engine
 SwiftAgent runs plan-act-reflect multi-turn tasks with a built-in
 context manager, tool executor, cache, replay, and telemetry. It
 speaks MCP (stdio + SSE) to attach external tools and ships with
 a Python SDK (pybind11).
Homepage: https://github.com/yuw868349-commits/G-
EOF

cat >"$WORK/swiftagent-${VERSION}/usr/local/lib/pkgconfig/swiftagent.pc" <<EOF
prefix=/usr/local
exec_prefix=\${prefix}
includedir=\${prefix}/include
libdir=\${prefix}/lib

Name: swiftagent
Description: C++23 agent execution engine
Version: ${VERSION}
Requires:
Libs:
Cflags: -I\${includedir}
EOF

dpkg-deb --build "$WORK/swiftagent-${VERSION}" "swiftagent_${VERSION}_amd64.deb"
echo "wrote swiftagent_${VERSION}_amd64.deb"
