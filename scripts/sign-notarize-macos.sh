#!/usr/bin/env bash
set -euo pipefail

# Signs the three macOS build artefacts (VST3, AU, Standalone) with a real Developer ID
# Application identity, hardened runtime enabled, then submits each to Apple's notary
# service and staples the resulting ticket. Run this AFTER `cmake --build` and BEFORE
# scripts/makedist-macos.sh, which packages whatever is already in NAMix_artefacts/ as-is
# (and, seeing a real Developer ID signature already present, skips its own ad-hoc
# fallback -- see the check there for why re-signing here would matter: it would
# invalidate the stapled ticket, since stapling is tied to the signed bundle's exact hash).
#
# Required environment:
#   SIGN_IDENTITY     Codesigning identity, e.g. "Developer ID Application: Gene Ko (QD3S486P93)"
#   NOTARY_KEY_ID      App Store Connect API key ID
#   NOTARY_ISSUER_ID   App Store Connect API issuer ID
#   NOTARY_API_KEY_PATH  Path to the AuthKey_<KEYID>.p8 file

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTEFACTS="$REPO/build/NAMix_artefacts/Release"

: "${SIGN_IDENTITY:?SIGN_IDENTITY must be set}"
: "${NOTARY_KEY_ID:?NOTARY_KEY_ID must be set}"
: "${NOTARY_ISSUER_ID:?NOTARY_ISSUER_ID must be set}"
: "${NOTARY_API_KEY_PATH:?NOTARY_API_KEY_PATH must be set}"

BUNDLES=(
  "$ARTEFACTS/VST3/Anti-Static NAM.vst3"
  "$ARTEFACTS/AU/Anti-Static NAM.component"
  "$ARTEFACTS/Standalone/Anti-Static NAM.app"
)

for BUNDLE in "${BUNDLES[@]}"; do
  echo "Signing: $BUNDLE"
  # --options runtime enables the hardened runtime, required for notarization.
  # --timestamp requests a secure timestamp from Apple, also required.
  codesign --force --deep --options runtime --timestamp \
    --sign "$SIGN_IDENTITY" "$BUNDLE"
done

NOTARY_TMPDIR="$(mktemp -d)"
trap 'rm -rf "$NOTARY_TMPDIR"' EXIT

for BUNDLE in "${BUNDLES[@]}"; do
  NAME="$(basename "$BUNDLE")"
  ZIP="$NOTARY_TMPDIR/$NAME.zip"
  echo "Submitting for notarization: $NAME"
  # notarytool needs a zip (or dmg/pkg), not a bare bundle path -- it can't submit a
  # directory directly. ditto (not zip) to avoid mangling the signature on the way in,
  # same reasoning as makedist-macos.sh's own packaging step.
  ditto -c -k --keepParent "$BUNDLE" "$ZIP"
  xcrun notarytool submit "$ZIP" \
    --key "$NOTARY_API_KEY_PATH" \
    --key-id "$NOTARY_KEY_ID" \
    --issuer "$NOTARY_ISSUER_ID" \
    --wait
  echo "Stapling: $NAME"
  xcrun stapler staple "$BUNDLE"
done

echo "All bundles signed, notarized, and stapled."
