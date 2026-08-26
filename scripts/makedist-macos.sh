#!/usr/bin/env bash
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cmake -B "$REPO/build" -DCMAKE_BUILD_TYPE=Release -S "$REPO"
cmake --build "$REPO/build" --parallel "$(sysctl -n hw.ncpu)"

# sed -E, not grep -P: macOS's built-in BSD grep has no -P (PCRE/lookbehind) support --
# only GNU grep does. -E (extended regex) is portable across BSD sed (macOS) and GNU sed
# (Linux), unlike -P, so all three platform scripts use this same form.
VERSION="$(sed -n -E 's/^project\(namix VERSION ([^ ]+).*/\1/p' "$REPO/CMakeLists.txt")"
ARCH="$(uname -m)"
STAGEDIR="$(mktemp -d)"
PKGDIR="$STAGEDIR/NAMix-${VERSION}"
mkdir -p "$PKGDIR"

ARTEFACTS="$REPO/build/NAMix_artefacts/Release"

# VST3 and AU bundles, Standalone .app -- JUCE's post-build step already ad-hoc-signs
# each of these (see the "Replacing invalid signature with ad-hoc signature" line in a
# normal build log), so no separate signing step here. This is NOT notarized: Gatekeeper
# will still warn on first launch, same disclosed tradeoff the Anti-Static (iPlug2)
# Windows build documents for its own unsigned .exe -- see README's "Before you install".
cp -r "$ARTEFACTS/VST3/Anti-Static NAM.vst3" "$PKGDIR/"
cp -r "$ARTEFACTS/AU/Anti-Static NAM.component" "$PKGDIR/"
cp -r "$ARTEFACTS/Standalone/Anti-Static NAM.app" "$PKGDIR/"

cp "$REPO/NOTICE" "$PKGDIR/"
cp "$REPO/LICENSE" "$PKGDIR/" 2>/dev/null || cp "$REPO/LICENCE" "$PKGDIR/" 2>/dev/null || true

mkdir -p "$REPO/dist"
ZIP="$REPO/dist/NAMix-${VERSION}-macos-${ARCH}.zip"
(cd "$STAGEDIR" && zip -r -y "$ZIP" "NAMix-${VERSION}" > /dev/null)
rm -rf "$STAGEDIR"

echo "Packaged: $ZIP"
echo ""
echo "Contents:"
unzip -l "$ZIP"
