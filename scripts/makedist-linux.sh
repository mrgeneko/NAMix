#!/usr/bin/env bash
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cmake -B "$REPO/build" -DCMAKE_BUILD_TYPE=Release -S "$REPO"
cmake --build "$REPO/build" --parallel "$(nproc)"

BUNDLE="$REPO/build/GatewayLinux_artefacts/Release/VST3/Gateway.vst3"

find "$BUNDLE" -name "*.so" -exec strip --strip-unneeded {} \;

VERSION="$(grep -oP '(?<=project\(gateway-linux VERSION )\S+' "$REPO/CMakeLists.txt")"

mkdir -p "$REPO/dist"
TARBALL="$REPO/dist/Gateway-${VERSION}-linux.tar.gz"
tar -czf "$TARBALL" -C "$(dirname "$BUNDLE")" "$(basename "$BUNDLE")"
echo "Packaged: $TARBALL"
