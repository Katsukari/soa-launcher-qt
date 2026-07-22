#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$SCRIPT_DIR"

BUILD_DIR="$SCRIPT_DIR/build-appimage"
APPDIR="$SCRIPT_DIR/AppDir"

require_command() {
  local command_name="$1"

  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "Required command not found: $command_name" >&2
    exit 1
  fi
}

check_portable_isa() {
  local root="$1"
  local failed=0
  local item
  local isa_info

  while IFS= read -r -d '' item; do
    if ! file -b "$item" 2>/dev/null | grep -q '^ELF '; then
      continue
    fi

    isa_info="$(
      readelf -nW "$item" 2>/dev/null |
        grep -E 'x86 ISA needed|x86 feature needed' || true
    )"

    if grep -qE 'x86-64-v[234]' <<< "$isa_info"; then
      printf '\nNon-portable ISA requirement found in:\n%s\n%s\n' \
        "$item" \
        "$isa_info" >&2
      failed=1
    fi
  done < <(find "$root" -type f -print0)

  if [ "$failed" -ne 0 ]; then
    echo "Refusing to create an AppImage containing x86-64-v2, v3, or v4 requirements." >&2
    exit 1
  fi
}

get_tool() {
  local name="$1"
  local url="$2"

  if [ ! -f "$name" ]; then
    echo "Downloading $name..."
    wget -q "$url" -O "$name"
    chmod +x "$name"
  fi
}

require_command cmake
require_command ninja
require_command wget
require_command file
require_command readelf
require_command find
require_command swiftc

if [ -z "${QMAKE:-}" ]; then
  QMAKE="$(command -v qmake6 || true)"
fi

if [ -z "$QMAKE" ] || [ ! -x "$QMAKE" ]; then
  echo "Could not find a Qt 6 qmake executable. Set QMAKE=/path/to/qmake6." >&2
  exit 1
fi

export QMAKE

unset CFLAGS
unset CXXFLAGS
unset CPPFLAGS
unset LDFLAGS

rm -rf "$BUILD_DIR"
rm -rf "$APPDIR"

cmake \
  -S "$PROJECT_ROOT" \
  -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=OFF \
  -DCMAKE_CXX_FLAGS="-march=x86-64 -mtune=generic"

cmake --build "$BUILD_DIR"

DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr

cp soa-launcher.png "$APPDIR/soa-launcher.png"
cp soa-launcher.desktop "$APPDIR/soa-launcher.desktop"

check_portable_isa "$APPDIR"

get_tool \
  linuxdeploy-x86_64.AppImage \
  "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"

get_tool \
  linuxdeploy-plugin-qt-x86_64.AppImage \
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

check_portable_isa "$APPDIR"

echo "Done. The .AppImage is in: $SCRIPT_DIR"