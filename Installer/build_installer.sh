#!/bin/bash
# IRONCORE macOS installer builder
#
# Unsigned (testing):   ./Installer/build_installer.sh
# Signed + notarized:   DEV_ID_APP="Developer ID Application: Name (TEAMID)" \
#                       DEV_ID_INSTALLER="Developer ID Installer: Name (TEAMID)" \
#                       NOTARY_PROFILE="ironcore-notary" ./Installer/build_installer.sh
#
# One-time setup for notarization (stores credentials in the keychain):
#   xcrun notarytool store-credentials ironcore-notary --apple-id you@example.com --team-id TEAMID
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/^project(IRONCORE VERSION \([0-9.]*\)).*/\1/p' "$ROOT/CMakeLists.txt")"
ART="$ROOT/build/IRONCORE_artefacts/Release"
OUT="$ROOT/dist"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "==> Building IRONCORE $VERSION (Release, universal)"
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/build" --config Release --target IRONCORE_VST3 IRONCORE_AU -j 12 >/dev/null

mkdir -p "$WORK/vst3" "$WORK/au" "$OUT"
cp -R "$ART/VST3/IRONCORE.vst3" "$WORK/vst3/"
cp -R "$ART/AU/IRONCORE.component" "$WORK/au/"

if [[ -n "${DEV_ID_APP:-}" ]]; then
  echo "==> Code signing plug-ins with: $DEV_ID_APP"
  for b in "$WORK/vst3/IRONCORE.vst3" "$WORK/au/IRONCORE.component"; do
    codesign --force --deep --timestamp --options runtime --sign "$DEV_ID_APP" "$b"
    codesign --verify --strict --verbose=2 "$b"
  done
else
  echo "==> No DEV_ID_APP set: ad-hoc signing (for testing only)"
  for b in "$WORK/vst3/IRONCORE.vst3" "$WORK/au/IRONCORE.component"; do codesign --force --deep --sign - "$b"; done
fi

echo "==> Creating component packages"
pkgbuild --root "$WORK/vst3" --identifier de.frequenzbetrieb.ironcore.vst3 --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/VST3" "$WORK/IRONCORE-VST3.pkg" >/dev/null
pkgbuild --root "$WORK/au" --identifier de.frequenzbetrieb.ironcore.au --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/Components" "$WORK/IRONCORE-AU.pkg" >/dev/null

sed "s/@VERSION@/$VERSION/g" "$ROOT/Installer/distribution.xml" > "$WORK/distribution.xml"
PKG="$OUT/IRONCORE-$VERSION-macOS.pkg"
SIGN_ARGS=()
[[ -n "${DEV_ID_INSTALLER:-}" ]] && SIGN_ARGS=(--sign "$DEV_ID_INSTALLER")

echo "==> Building product installer"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" \
             --resources "$ROOT/Installer/resources" ${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"} "$PKG" >/dev/null

if [[ -n "${NOTARY_PROFILE:-}" && -n "${DEV_ID_INSTALLER:-}" ]]; then
  echo "==> Notarizing (this can take a few minutes)"
  xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
  xcrun stapler staple "$PKG"
fi

cp "$ROOT/docs/IRONCORE-Handbuch.html" "$OUT/"
echo "==> Done: $PKG"
