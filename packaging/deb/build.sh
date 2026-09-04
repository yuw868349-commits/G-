#!/usr/bin/env bash
# Build a source tarball that matches the Debian package layout.
# Usage: ./packaging/deb/build.sh <version>
set -euo pipefail

VERSION="${1:-0.1.0}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$WORK/praxis-${VERSION}/DEBIAN"
mkdir -p "$WORK/praxis-${VERSION}/usr/local/bin"
mkdir -p "$WORK/praxis-${VERSION}/usr/local/include/praxis"
mkdir -p "$WORK/praxis-${VERSION}/usr/local/share/praxis/examples"
mkdir -p "$WORK/praxis-${VERSION}/usr/local/share/doc/praxis"
mkdir -p "$WORK/praxis-${VERSION}/usr/local/lib/pkgconfig"

cmake -S "$ROOT" -B "$WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/build" -j

cp "$WORK/build/praxis" "$WORK/praxis-${VERSION}/usr/local/bin/"
cp -r "$ROOT/src/core" "$ROOT/src/llm" "$ROOT/src/mcp" "$ROOT/src/platform" \
      "$ROOT/src/tools" "$ROOT/src/ui" \
      "$WORK/praxis-${VERSION}/usr/local/include/praxis/"
cp "$ROOT/LICENSE" "$WORK/praxis-${VERSION}/usr/local/share/doc/praxis/copyright"
cp "$ROOT/README.md" "$WORK/praxis-${VERSION}/usr/local/share/doc/praxis/"

cat >"$WORK/praxis-${VERSION}/DEBIAN/control" <<EOF
Package: praxis
Version: ${VERSION}
Section: devel
Priority: optional
Architecture: amd64
Depends: libstdc++6 (>= 12), libc6 (>= 2.36)
Maintainer: Praxis Contributors <noreply@example.com>
Description: C++23 agent execution engine
 Praxis runs plan-act-reflect multi-turn tasks with a built-in
 context manager, tool executor, cache, replay, and telemetry. It
 speaks MCP (stdio + SSE) to attach external tools and ships with
 a Python SDK (pybind11).
Homepage: https://github.com/yuw868349-commits/praxis
EOF

cat >"$WORK/praxis-${VERSION}/usr/local/lib/pkgconfig/praxis.pc" <<EOF
prefix=/usr/local
exec_prefix=\${prefix}
includedir=\${prefix}/include
libdir=\${prefix}/lib

Name: praxis
Description: C++23 agent execution engine
Version: ${VERSION}
Requires:
Libs:
Cflags: -I\${includedir}
EOF

dpkg-deb --build "$WORK/praxis-${VERSION}" "praxis_${VERSION}_amd64.deb"
echo "wrote praxis_${VERSION}_amd64.deb"
