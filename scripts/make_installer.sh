#!/bin/bash
# Packages the built AU + VST3 + standalone app into a double-click .pkg installer, and the zip the plugin
# downloads when it updates itself.
# Usage: bash scripts/make_installer.sh <artefacts dir> [build number]
set -e
cd "$(dirname "$0")/.."
ART="${1:-build/MUMusicMaker_artefacts/Release}"
BUILD="${2:-local}"
BASE="$(grep -m1 'set(MUM_VERSION_BASE' CMakeLists.txt | sed -E 's/.*"([0-9.]+)".*/\1/')"
if [ "$BUILD" = "local" ]; then VERSION="$BASE.0"; else VERSION="$BASE.$BUILD"; fi
NAME="MU Music Maker"

AU="$ART/AU/$NAME.component"
VST3="$ART/VST3/$NAME.vst3"
APP="$ART/Standalone/$NAME.app"
[ -d "$AU" ] || { echo "AU not found at $AU"; exit 1; }
[ -d "$VST3" ] || { echo "VST3 not found at $VST3"; exit 1; }

rm -rf dist pkgroot
mkdir -p dist pkgroot/Library/Audio/Plug-Ins/Components pkgroot/Library/Audio/Plug-Ins/VST3
cp -R "$AU" pkgroot/Library/Audio/Plug-Ins/Components/
cp -R "$VST3" pkgroot/Library/Audio/Plug-Ins/VST3/
if [ -d "$APP" ]; then
  mkdir -p pkgroot/Applications
  cp -R "$APP" pkgroot/Applications/
fi

# Ad-hoc sign so Gatekeeper is happier on Apple Silicon (replace with a Developer ID later).
codesign --force --deep --sign - "pkgroot/Library/Audio/Plug-Ins/Components/$NAME.component" || true
codesign --force --deep --sign - "pkgroot/Library/Audio/Plug-Ins/VST3/$NAME.vst3" || true
[ -d "pkgroot/Applications/$NAME.app" ] && codesign --force --deep --sign - "pkgroot/Applications/$NAME.app" || true

# After installing, hand the plugin to the person at the keyboard, so it can update itself later
# without asking for an administrator password.
rm -rf pkgscripts && mkdir pkgscripts
cat > pkgscripts/postinstall <<'POST'
#!/bin/bash
OWNER="$(stat -f%Su /dev/console 2>/dev/null)"
if [ -n "$OWNER" ] && [ "$OWNER" != "root" ]; then
  chown -R "$OWNER" "/Library/Audio/Plug-Ins/Components/MU Music Maker.component" 2>/dev/null || true
  chown -R "$OWNER" "/Library/Audio/Plug-Ins/VST3/MU Music Maker.vst3" 2>/dev/null || true
fi
exit 0
POST
chmod +x pkgscripts/postinstall

pkgbuild --root pkgroot --scripts pkgscripts --identifier com.musicallyunorthodox.musicmaker.pkg --version "$VERSION" \
         --install-location / "dist/MUMusicMaker-$VERSION-build$BUILD.pkg"
cp "dist/MUMusicMaker-$VERSION-build$BUILD.pkg" dist/MUMusicMaker-Mac.pkg
rm "dist/MUMusicMaker-$VERSION-build$BUILD.pkg"

# What the plugin downloads when it updates itself: the two plugin bundles, zipped,
# and a small feed saying which build is newest and what the zip's checksum is.
rm -rf update && mkdir -p update
cp -R pkgroot/Library/Audio/Plug-Ins/Components update/
cp -R pkgroot/Library/Audio/Plug-Ins/VST3 update/
( cd update && zip -qry ../dist/MUMusicMaker-bundles.zip Components VST3 )
SHA="$(shasum -a 256 dist/MUMusicMaker-bundles.zip | cut -d' ' -f1)"
BUILDNUM="$BUILD"; [ "$BUILD" = "local" ] && BUILDNUM=0
printf '{"build": %s, "version": "%s", "file": "MUMusicMaker-bundles.zip", "sha256": "%s"}\n' "$BUILDNUM" "$VERSION" "$SHA" > dist/latest.json

rm -rf pkgroot pkgscripts update
echo "Installer written to dist/"
ls -la dist
cat dist/latest.json
