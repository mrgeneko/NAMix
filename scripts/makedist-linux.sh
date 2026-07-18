#!/usr/bin/env bash
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cmake -B "$REPO/build" -DCMAKE_BUILD_TYPE=Release -S "$REPO"
cmake --build "$REPO/build" --parallel "$(nproc)"

VERSION="$(grep -oP '(?<=project\(namix-linux VERSION )\S+' "$REPO/CMakeLists.txt")"
ARCH="$(uname -m)"
STAGEDIR="$(mktemp -d)"
PKGDIR="$STAGEDIR/NAMix-${VERSION}"
mkdir -p "$PKGDIR"

# VST3 bundle — strip the shared library inside the bundle
BUNDLE="$REPO/build/NAMixLinux_artefacts/Release/VST3/Anti-Static NAM.vst3"
find "$BUNDLE" -name "*.so" -exec strip --strip-unneeded {} \;
cp -r "$BUNDLE" "$PKGDIR/"

# Standalone binary
STANDALONE="$REPO/build/NAMixLinux_artefacts/Release/Standalone/Anti-Static NAM"
if [ -f "$STANDALONE" ]; then
  strip --strip-unneeded "$STANDALONE"
  cp "$STANDALONE" "$PKGDIR/"
fi

# Licence and attribution
cp "$REPO/NOTICE" "$PKGDIR/"
cp "$REPO/LICENCE" "$PKGDIR/" 2>/dev/null || cp "$REPO/LICENSE" "$PKGDIR/" 2>/dev/null || true

mkdir -p "$REPO/dist"
TARBALL="$REPO/dist/NAMix-${VERSION}-linux-${ARCH}.tar.gz"
tar -czf "$TARBALL" -C "$STAGEDIR" "NAMix-${VERSION}"
rm -rf "$STAGEDIR"

echo "Packaged: $TARBALL"
echo ""
echo "Contents:"
tar -tzf "$TARBALL"

