#!/usr/bin/env bash
# Builds a self-contained AppImage for the Story Of Alicia launcher.
#
# linuxdeploy reads the binary's real dependencies and bundles them all -
# Swift runtime, Qt libraries, the xcb platform plugin, ICU, everything.
#
# Usage (from anywhere):
#   ./packaging/build_appimage.sh
#
# All build artifacts (build dir, AppDir, downloaded tools, the final
# .AppImage) are kept inside packaging/.
set -euo pipefail

# Resolve paths: the script lives in packaging/, the project root is its parent.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Work inside packaging/ so all artifacts land here.
cd "$SCRIPT_DIR"

BUILD_DIR="build-appimage"
APPDIR="AppDir"

# --- 1. Build (Release, Ninja - Swift needs Ninja). Source is the repo root. ---
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
cmake --build "$BUILD_DIR"

# --- 2. Install into a clean AppDir ---
rm -rf "$APPDIR"
DESTDIR="$PWD/$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr

# appimagetool expects the icon and desktop file at the AppDir ROOT (not just in
# the share/ hierarchy). Copy them up from packaging/.
cp soa-launcher.png     "$APPDIR/soa-launcher.png"
cp soa-launcher.desktop "$APPDIR/soa-launcher.desktop"

# --- 3. Fetch linuxdeploy + the Qt plugin if not present (into packaging/) ---
get_tool() {
  local name="$1" url="$2"
  if [ ! -f "$name" ]; then
    echo "Downloading $name..."
    wget -q "$url" -O "$name"
    chmod +x "$name"
  fi
}
get_tool linuxdeploy-x86_64.AppImage \
  "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
get_tool linuxdeploy-plugin-qt-x86_64.AppImage \
  "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"

# --- 4. Point the loader at the Swift runtime so it gets bundled ---
SWIFT_BIN="$(dirname "$(command -v swiftc)")"
SWIFT_LIB="$(dirname "$SWIFT_BIN")/lib/swift/linux"
export LD_LIBRARY_PATH="${SWIFT_LIB}:${LD_LIBRARY_PATH:-}"

# --- 5. Run linuxdeploy: bundle everything + make the AppImage ---
export QML_SOURCES_PATHS=""   # not a QML app

# Bundled strip is too old for modern .relr.dyn ELF sections (Arch) - skip it.
export NO_STRIP=1

# Exclude exotic KDE image-format plugins the launcher doesn't use (they pull
# obscure codec libs). PNG is built into Qt.
export EXCLUDE_QT_PLUGINS="imageformats/kimg_jxr.so;imageformats/kimg_heif.so;imageformats/kimg_avif.so;imageformats/kimg_jp2.so;imageformats/kimg_exr.so;imageformats/kimg_dds.so;imageformats/kimg_eps.so;imageformats/kimg_ff.so;imageformats/kimg_hdr.so;imageformats/kimg_jxl.so;imageformats/kimg_psd.so;imageformats/kimg_pcx.so;imageformats/kimg_ras.so;imageformats/kimg_rgb.so;imageformats/kimg_tga.so;imageformats/kimg_xcf.so;imageformats/kimg_pic.so;imageformats/kimg_qoi.so"

./linuxdeploy-x86_64.AppImage \
  --appdir "$APPDIR" \
  --plugin qt \
  --output appimage \
  --desktop-file "$APPDIR/usr/share/applications/soa-launcher.desktop" \
  --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/soa-launcher.png"

echo "Done. The .AppImage is in: $SCRIPT_DIR"