#!/usr/bin/env bash










set -euo pipefail


SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"


cd "$SCRIPT_DIR"

BUILD_DIR="build-appimage"
APPDIR="AppDir"





if [ -z "${QMAKE:-}" ]; then
  QMAKE="$(command -v qmake6 || true)"
fi
if [ -z "$QMAKE" ] || [ ! -x "$QMAKE" ]; then
  echo "Could not find a Qt 6 qmake executable. Set QMAKE=/path/to/qmake6." >&2
  exit 1
fi
export QMAKE

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF





cmake --build "$BUILD_DIR" --clean-first


rm -rf "$APPDIR"
DESTDIR="$PWD/$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr



cp soa-launcher.png     "$APPDIR/soa-launcher.png"
cp soa-launcher.desktop "$APPDIR/soa-launcher.desktop"


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


SWIFT_BIN="$(dirname "$(command -v swiftc)")"
SWIFT_LIB="$(dirname "$SWIFT_BIN")/lib/swift/linux"
export LD_LIBRARY_PATH="${SWIFT_LIB}:${LD_LIBRARY_PATH:-}"


export QML_SOURCES_PATHS=""


export NO_STRIP=1



export EXCLUDE_QT_PLUGINS="imageformats/kimg_jxr.so;imageformats/kimg_heif.so;imageformats/kimg_avif.so;imageformats/kimg_jp2.so;imageformats/kimg_exr.so;imageformats/kimg_dds.so;imageformats/kimg_eps.so;imageformats/kimg_ff.so;imageformats/kimg_hdr.so;imageformats/kimg_jxl.so;imageformats/kimg_psd.so;imageformats/kimg_pcx.so;imageformats/kimg_ras.so;imageformats/kimg_rgb.so;imageformats/kimg_tga.so;imageformats/kimg_xcf.so;imageformats/kimg_pic.so;imageformats/kimg_qoi.so"

./linuxdeploy-x86_64.AppImage \
  --appdir "$APPDIR" \
  --plugin qt \
  --output appimage \
  --desktop-file "$APPDIR/usr/share/applications/soa-launcher.desktop" \
  --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/soa-launcher.png"

echo "Done. The .AppImage is in: $SCRIPT_DIR"
