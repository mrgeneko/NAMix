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

# VST3 and AU bundles -- JUCE's post-build step already ad-hoc-signs each of these (see
# the "Replacing invalid signature with ad-hoc signature" line in a normal build log).
# The Standalone .app does NOT get that treatment: the linker only applies its automatic
# ad-hoc signature to the raw executable, not a proper bundle-level signature with sealed
# resources, which codesign's --deep verification (and Gatekeeper, once quarantined by a
# browser download) rejects as an invalid signature -- "code has no resources but
# signature indicates they must be present" -- rather than merely untrusted. That reads to
# the user as "app is damaged, move to Trash" instead of the expected, dismissable
# "unidentified developer" prompt. Explicitly re-signing the whole bundle here fixes it.
# This is still NOT notarized: Gatekeeper will still warn on first launch with the
# expected "unidentified developer" prompt, same disclosed tradeoff the Anti-Static
# (iPlug2) Windows build documents for its own unsigned .exe -- see README's "Before you
# install".
cp -r "$ARTEFACTS/VST3/Anti-Static NAM.vst3" "$PKGDIR/"
cp -r "$ARTEFACTS/AU/Anti-Static NAM.component" "$PKGDIR/"
cp -r "$ARTEFACTS/Standalone/Anti-Static NAM.app" "$PKGDIR/"
codesign --force --deep --sign - "$PKGDIR/Anti-Static NAM.app"

cp "$REPO/NOTICE" "$PKGDIR/"
cp "$REPO/LICENSE" "$PKGDIR/" 2>/dev/null || cp "$REPO/LICENCE" "$PKGDIR/" 2>/dev/null || true

mkdir -p "$REPO/dist"
ZIP="$REPO/dist/NAMix-${VERSION}-macos-${ARCH}.zip"
# ditto, not zip: the plain `zip` CLI is documented to mangle extended attributes/resource
# forks on signed bundles, which can itself invalidate a previously-valid signature on
# unzip. ditto is Apple's own tool for this and is what Xcode/notarization workflows use.
(cd "$STAGEDIR" && ditto -c -k --sequesterRsrc --keepParent "NAMix-${VERSION}" "$ZIP")
rm -rf "$STAGEDIR"

echo "Packaged: $ZIP"
echo ""
echo "Contents:"
unzip -l "$ZIP"
