#!/usr/bin/env bash
# Packs a macOS build of IWP into dist/IWP-macos-<version>.zip (IWP.app + licences).
#
#   tools/deploy_macos.sh [build-dir] [config] [out-dir] [version]
#   tools/deploy_macos.sh build Release dist 0.1.0
#
# The build produces a plain executable with plugins/ next to it; this script assembles the .app bundle
# (host, SDK and helper in Contents/MacOS, plugins in Contents/PlugIns, where the host looks for them),
# lets macdeployqt copy Qt (given the SDK, the helper and every plugin: the plugins are loaded at run
# time, so macdeployqt would not see their Qt modules otherwise) and signs the bundle.
#
# Environment:
#   QT_ROOT_DIR          Qt prefix (used to find macdeployqt if it is not in PATH)
#   QTKEYCHAIN_ROOT      install prefix of QtKeychain: its dylib is bundled
#   CODESIGN_IDENTITY    e.g. "Developer ID Application: Name (TEAMID)"; default: ad-hoc signature
#                        (enough to run on this Mac; other Macs need a real identity + notarization)
set -euo pipefail

BUILD_DIR="${1:-build}"
CONFIG="${2:-Release}"
OUT_DIR="${3:-dist}"
VERSION="${4:-dev}"

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"

BIN="$BUILD_DIR/bin/$CONFIG"
for f in iwp_host iwp_askpass libiwp_sdk.dylib; do
    [ -f "$BIN/$f" ] || { echo "error: $BIN/$f not found, build the project first" >&2; exit 1; }
done

MACDEPLOYQT="$(command -v macdeployqt || true)"
if [ -z "$MACDEPLOYQT" ] && [ -n "${QT_ROOT_DIR:-}" ] && [ -x "$QT_ROOT_DIR/bin/macdeployqt" ]; then
    MACDEPLOYQT="$QT_ROOT_DIR/bin/macdeployqt"
fi
[ -n "$MACDEPLOYQT" ] || { echo "error: macdeployqt not found (add Qt's bin to PATH or set QT_ROOT_DIR)" >&2; exit 1; }

NAME="IWP-macos-$VERSION"
STAGE="$OUT_DIR/$NAME"
APP="$STAGE/IWP.app"
rm -rf "$STAGE"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/PlugIns" "$APP/Contents/Resources"

cp "$BIN/iwp_host" "$BIN/iwp_askpass" "$BIN/libiwp_sdk.dylib" "$APP/Contents/MacOS/"
cp "$BIN"/plugins/*.dylib "$APP/Contents/PlugIns/"
cp host/platform/app.icns "$APP/Contents/Resources/app.icns"

SHORT_VERSION="${VERSION#v}"
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key><string>iwp_host</string>
    <key>CFBundleIdentifier</key><string>org.iwp.workbench</string>
    <key>CFBundleName</key><string>Instrument Workbench</string>
    <key>CFBundleDisplayName</key><string>Instrument Workbench</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>${SHORT_VERSION}</string>
    <key>CFBundleVersion</key><string>1</string>
    <key>CFBundleIconFile</key><string>app.icns</string>
    <key>LSMinimumSystemVersion</key><string>12.0</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>NSPrincipalClass</key><string>NSApplication</string>
</dict>
</plist>
PLIST

# Plugins find libiwp_sdk.dylib through @loader_path/.. (the build tree); in the bundle it is in MacOS.
for p in "$APP"/Contents/PlugIns/*.dylib; do
    install_name_tool -add_rpath "@loader_path/../MacOS" "$p" 2>/dev/null || true
done

ARGS=(-executable="$APP/Contents/MacOS/iwp_askpass" -executable="$APP/Contents/MacOS/libiwp_sdk.dylib")
for p in "$APP"/Contents/PlugIns/*.dylib; do
    ARGS+=(-executable="$p")
done
if [ -n "${QTKEYCHAIN_ROOT:-}" ]; then
    ARGS+=(-libpath="$QTKEYCHAIN_ROOT/lib")
fi
echo "macdeployqt: $MACDEPLOYQT"
"$MACDEPLOYQT" "$APP" "${ARGS[@]}"

IDENTITY="${CODESIGN_IDENTITY:--}"
if [ "$IDENTITY" = "-" ]; then
    codesign --force --deep --sign - "$APP"
else
    codesign --force --deep --options runtime --timestamp --sign "$IDENTITY" "$APP"
fi

cp LICENSE README.md "$STAGE/"
cp -R THIRD_PARTY "$STAGE/THIRD_PARTY"

rm -f "$OUT_DIR/$NAME.zip"
ditto -c -k --keepParent "$STAGE" "$OUT_DIR/$NAME.zip"
echo "Done: $OUT_DIR/$NAME.zip"
