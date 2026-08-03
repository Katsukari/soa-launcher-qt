#!/usr/bin/env bash

set -euo pipefail

echo "SOA AppImage packager: minimal Qt plugins revision 2026-08-04"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

resolve_project_root() {
  local requested="${1:-${SOA_SOURCE_DIR:-}}"
  local candidate

  if [ -n "$requested" ]; then
    candidate="$(cd "$requested" 2>/dev/null && pwd)" || {
      echo "Launcher source directory does not exist: $requested" >&2
      exit 1
    }
  elif [ -f "$PWD/CMakeLists.txt" ]; then
    candidate="$PWD"
  else
    candidate="$SCRIPT_DIR"
    while [ "$candidate" != "/" ] && [ ! -f "$candidate/CMakeLists.txt" ]; do
      candidate="$(dirname "$candidate")"
    done
  fi

  if [ ! -f "$candidate/CMakeLists.txt" ]; then
    echo "Could not find the launcher CMakeLists.txt." >&2
    echo "Run this script from the repository root, pass the source directory as its first argument," >&2
    echo "or set SOA_SOURCE_DIR=/path/to/soa-launcher-qt." >&2
    exit 1
  fi

  printf '%s\n' "$candidate"
}

if [ "$#" -gt 1 ]; then
  echo "Usage: $0 [launcher-source-directory]" >&2
  exit 2
fi

PROJECT_ROOT="$(resolve_project_root "${1:-}")"

cd "$SCRIPT_DIR"

echo "Launcher source directory: $PROJECT_ROOT"

BUILD_DIR="$SCRIPT_DIR/build-appimage"
APPDIR="$SCRIPT_DIR/AppDir"
LINUXDEPLOY_TAG="${LINUXDEPLOY_TAG:-1-alpha-20250213-2}"
LINUXDEPLOY_SHA256="${LINUXDEPLOY_SHA256:-4648f278ab3ef31f819e67c30d50f462640e5365a77637d7e6f2ad9fd0b4522a}"

require_command() {
  local command_name="$1"
  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "Required command not found: $command_name" >&2
    exit 1
  fi
}

run_portability_check() {
  local root="$1"

  if [ "${SOA_ALLOW_NONPORTABLE_LOCAL_BUILD:-0}" = "1" ]; then
    echo "Skipping Linux portability checks for local test build: $root" >&2
    return 0
  fi

  "$SCRIPT_DIR/check-linux-portability.sh" "$root"
}

get_tool() {
  local name="$1"
  local url="$2"
  local expected="$3"
  local temporary="${name}.download"

  if [ -z "$expected" ]; then
    echo "A SHA-256 checksum is required for $name." >&2
    exit 1
  fi

  if [ -f "$name" ]; then
    local actual
    actual="$(sha256sum "$name" | awk '{print $1}')"
    if [ "$actual" != "$expected" ]; then
      rm -f "$name"
    fi
  fi

  if [ ! -f "$name" ]; then
    echo "Downloading $name..."
    wget -q "$url" -O "$temporary"
    echo "$expected  $temporary" | sha256sum --check --status
    mv "$temporary" "$name"
    chmod +x "$name"
  fi
}

require_command cmake
require_command ninja
require_command wget
require_command file
require_command readelf
require_command find
require_command sha256sum
require_command swiftc
require_command timeout
require_command desktop-file-validate
require_command jq
require_command openssl

TEMPORARY_UPDATE_KEY=""
UPDATE_HISTORY_TEMP=""
if [ -z "${SOA_UPDATE_SIGNING_KEY:-}" ]; then
  TEMPORARY_UPDATE_KEY="$(mktemp)"
  if [ -n "${SOA_UPDATE_SIGNING_KEY_B64:-}" ]; then
    printf '%s' "$SOA_UPDATE_SIGNING_KEY_B64" | base64 --decode >"$TEMPORARY_UPDATE_KEY"
  elif [ "${GITHUB_REF_TYPE:-}" = "tag" ]; then
    echo "Tagged release builds require the SOA_UPDATE_SIGNING_KEY_B64 secret." >&2
    rm -f "$TEMPORARY_UPDATE_KEY"
    exit 1
  else
    echo "Using an ephemeral update key for this non-release build." >&2
    openssl genpkey -algorithm Ed25519 -out "$TEMPORARY_UPDATE_KEY"
  fi
  chmod 600 "$TEMPORARY_UPDATE_KEY"
  SOA_UPDATE_SIGNING_KEY="$TEMPORARY_UPDATE_KEY"
fi

cleanup_update_key() {
  if [ -n "$TEMPORARY_UPDATE_KEY" ]; then
    rm -f "$TEMPORARY_UPDATE_KEY"
  fi
  if [ -n "$UPDATE_HISTORY_TEMP" ]; then
    rm -rf "$UPDATE_HISTORY_TEMP"
  fi
}
trap cleanup_update_key EXIT

if [ -z "${SOA_UPDATE_SIGNING_KEY:-}" ] || [ ! -f "$SOA_UPDATE_SIGNING_KEY" ]; then
  echo "SOA_UPDATE_SIGNING_KEY must point to the offline Ed25519 private key." >&2
  exit 1
fi

DERIVED_UPDATE_PUBLIC_KEY_HEX="$(
  openssl pkey -in "$SOA_UPDATE_SIGNING_KEY" -pubout -outform DER \
    | tail -c 32 \
    | od -An -v -tx1 \
    | tr -d ' \n'
)"
if [ -n "${SOA_UPDATE_PUBLIC_KEY_HEX:-}" ] \
    && [ "${SOA_UPDATE_PUBLIC_KEY_HEX,,}" != "$DERIVED_UPDATE_PUBLIC_KEY_HEX" ]; then
  echo "SOA_UPDATE_PUBLIC_KEY_HEX does not match SOA_UPDATE_SIGNING_KEY." >&2
  exit 1
fi
SOA_UPDATE_PUBLIC_KEY_HEX="$DERIVED_UPDATE_PUBLIC_KEY_HEX"

LAUNCHER_VERSION="${SOA_LAUNCHER_VERSION:-$(
  sed -nE 's/^[[:space:]]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
    "$PROJECT_ROOT/CMakeLists.txt" | head -n 1
)}"
if [[ ! "$LAUNCHER_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+([+-][0-9A-Za-z.-]+)?$ ]]; then
  echo "Could not determine a valid launcher version." >&2
  exit 1
fi

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
rm -rf "$SCRIPT_DIR/squashfs-root"

cmake \
  -S "$PROJECT_ROOT" \
  -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DSOA_LAUNCHER_UPDATE_PUBLIC_KEY_HEX="$SOA_UPDATE_PUBLIC_KEY_HEX" \
  -DCMAKE_CXX_FLAGS="-march=x86-64 -mtune=generic"

cmake --build "$BUILD_DIR"

DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr

mkdir -p "$APPDIR/usr/share/soa-launcher/update"
jq -n \
  --arg version "$LAUNCHER_VERSION" \
  --arg manifest_url "https://r2.storyofalicia.com/launcher/linux_launcher_version.json" \
  --arg fallback_manifest_url \
    "https://github.com/Story-Of-Alicia/soa-launcher-qt/releases/latest/download/linux_launcher_version.json" \
  --arg signing_public_key "$SOA_UPDATE_PUBLIC_KEY_HEX" \
  --arg soa_developer_key "$SOA_UPDATE_PUBLIC_KEY_HEX" \
  '{schema: 1, version: $version, platform: "linux-x86_64",
    manifest_url: $manifest_url, fallback_manifest_url: $fallback_manifest_url,
    signing_public_key: $signing_public_key, soa_developer_key: $soa_developer_key}' \
  >"$APPDIR/usr/share/soa-launcher/update/linux_launcher_version.json"

cp soa-launcher.png "$APPDIR/soa-launcher.png"
cp soa-launcher.desktop "$APPDIR/soa-launcher.desktop"

desktop-file-validate "$APPDIR/usr/share/applications/soa-launcher.desktop"

get_tool \
  linuxdeploy-x86_64.AppImage \
  "https://github.com/linuxdeploy/linuxdeploy/releases/download/$LINUXDEPLOY_TAG/linuxdeploy-x86_64.AppImage" \
  "$LINUXDEPLOY_SHA256"

SWIFT_BIN="$(dirname "$(command -v swiftc)")"
SWIFT_LIB="$(dirname "$SWIFT_BIN")/lib/swift/linux"

export LD_LIBRARY_PATH="${SWIFT_LIB}:${LD_LIBRARY_PATH:-}"
export NO_STRIP=1

QT_PLUGIN_DIR="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
QT_TRANSLATIONS_DIR="$("$QMAKE" -query QT_INSTALL_TRANSLATIONS)"
QT_PLATFORM_DIR="$QT_PLUGIN_DIR/platforms"

copy_qt_plugin() {
  local category="$1"
  local filename="$2"
  local required="${3:-0}"
  local source="$QT_PLUGIN_DIR/$category/$filename"
  local destination="$APPDIR/usr/plugins/$category"
  local dependencies

  if [ ! -f "$source" ]; then
    if [ "$required" = "1" ]; then
      echo "Required Qt plugin not found: $source" >&2
      exit 1
    fi
    return 0
  fi

  dependencies="$(ldd "$source" 2>/dev/null || true)"
  if grep -qE '=> not found|libheif\.so\.1' <<< "$dependencies"; then
    if [ "$required" = "1" ]; then
      echo "Required Qt plugin has unresolved dependencies: $source" >&2
      grep '=> not found' <<< "$dependencies" >&2 || true
      exit 1
    fi
    echo "Skipping optional Qt plugin with unresolved dependencies: $source" >&2
    grep '=> not found' <<< "$dependencies" >&2 || true
    return 0
  fi

  mkdir -p "$destination"
  install -m 755 "$source" "$destination/$filename"
  echo "Included Qt plugin: $category/$filename"
}

copy_qt_plugin_directory() {
  local category="$1"
  local source

  if [ ! -d "$QT_PLUGIN_DIR/$category" ]; then
    return 0
  fi

  while IFS= read -r -d '' source; do
    copy_qt_plugin "$category" "$(basename "$source")"
  done < <(find "$QT_PLUGIN_DIR/$category" -maxdepth 1 -type f -name '*.so' -print0)
}

copy_qt_plugin platforms libqxcb.so 1

if [ -f "$QT_PLATFORM_DIR/libqwayland.so" ]; then
  copy_qt_plugin platforms libqwayland.so 1
elif [ -f "$QT_PLATFORM_DIR/libqwayland-egl.so" ] \
    && [ -f "$QT_PLATFORM_DIR/libqwayland-generic.so" ]; then
  copy_qt_plugin platforms libqwayland-egl.so 1
  copy_qt_plugin platforms libqwayland-generic.so 1
else
  echo "Required Qt Wayland platform plugin not found in: $QT_PLATFORM_DIR" >&2
  exit 1
fi

copy_qt_plugin imageformats libqgif.so
copy_qt_plugin imageformats libqico.so
copy_qt_plugin imageformats libqjpeg.so
copy_qt_plugin imageformats libqsvg.so
copy_qt_plugin imageformats libqwebp.so
copy_qt_plugin iconengines libqsvgicon.so
copy_qt_plugin_directory platforminputcontexts
copy_qt_plugin_directory xcbglintegrations
copy_qt_plugin_directory wayland-decoration-client
copy_qt_plugin_directory wayland-graphics-integration-client
copy_qt_plugin_directory wayland-shell-integration
copy_qt_plugin_directory networkinformation
copy_qt_plugin_directory tls

mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/translations"
cat >"$APPDIR/usr/bin/qt.conf" <<'EOF'
[Paths]
Prefix = ..
Plugins = plugins
Translations = translations
EOF

if [ -d "$QT_TRANSLATIONS_DIR" ]; then
  find "$QT_TRANSLATIONS_DIR" -maxdepth 1 -type f \
    \( -name 'qt_*.qm' -o -name 'qtbase_*.qm' \) \
    -exec cp -f '{}' "$APPDIR/usr/translations/" \;
fi

while IFS= read -r -d '' packaged_plugin; do
  if ldd "$packaged_plugin" 2>/dev/null | grep -q 'libheif\.so\.1'; then
    echo "A curated Qt plugin unexpectedly references libheif.so.1: $packaged_plugin" >&2
    exit 1
  fi
done < <(find "$APPDIR/usr/plugins" -type f -name '*.so' -print0)

./linuxdeploy-x86_64.AppImage \
  --appdir "$APPDIR" \
  --output appimage \
  --desktop-file "$APPDIR/usr/share/applications/soa-launcher.desktop" \
  --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/soa-launcher.png"

run_portability_check "$APPDIR"

OUTPUT="$SCRIPT_DIR/Story_Of_Alicia-x86_64.AppImage"
if [ ! -f "$OUTPUT" ]; then
  echo "The expected AppImage was not produced: $OUTPUT" >&2
  exit 1
fi

if [ "${SOA_ALLOW_NONPORTABLE_LOCAL_BUILD:-0}" != "1" ]; then
  outer_isa="$(readelf -nW "$OUTPUT" 2>/dev/null | grep -E 'x86 ISA needed|x86 feature needed' || true)"
  if grep -qE 'x86-64-v[234]' <<< "$outer_isa"; then
    printf 'The AppImage runtime has a nonportable ISA requirement:\n%s\n' "$outer_isa" >&2
    exit 1
  fi
fi

(
  cd "$SCRIPT_DIR"
  APPIMAGE_EXTRACT_AND_RUN=1 "$OUTPUT" --appimage-extract >/dev/null
)

run_portability_check "$SCRIPT_DIR/squashfs-root"

SMOKE_ROOT="$(mktemp -d)"
SMOKE_LOG="$(mktemp)"

cleanup_smoke() {
  rm -rf "$SMOKE_ROOT"
  rm -f "$SMOKE_LOG"
}

trap 'cleanup_smoke; cleanup_update_key' EXIT

mkdir -p \
  "$SMOKE_ROOT/home" \
  "$SMOKE_ROOT/config" \
  "$SMOKE_ROOT/data" \
  "$SMOKE_ROOT/cache" \
  "$SMOKE_ROOT/runtime"

SMOKE_STATUS=0
set +e

if command -v xvfb-run >/dev/null 2>&1; then
  timeout 8 env \
    LD_LIBRARY_PATH= \
    HOME="$SMOKE_ROOT/home" \
    XDG_CONFIG_HOME="$SMOKE_ROOT/config" \
    XDG_DATA_HOME="$SMOKE_ROOT/data" \
    XDG_CACHE_HOME="$SMOKE_ROOT/cache" \
    XDG_RUNTIME_DIR="$SMOKE_ROOT/runtime" \
    QT_QPA_PLATFORM=xcb \
    xvfb-run -a -s "-screen 0 1280x800x24" \
    "$SCRIPT_DIR/squashfs-root/AppRun" >"$SMOKE_LOG" 2>&1
  SMOKE_STATUS=$?
elif [ -n "${DISPLAY:-}" ]; then
  timeout 8 env \
    LD_LIBRARY_PATH= \
    HOME="$SMOKE_ROOT/home" \
    XDG_CONFIG_HOME="$SMOKE_ROOT/config" \
    XDG_DATA_HOME="$SMOKE_ROOT/data" \
    XDG_CACHE_HOME="$SMOKE_ROOT/cache" \
    XDG_RUNTIME_DIR="$SMOKE_ROOT/runtime" \
    QT_QPA_PLATFORM=xcb \
    "$SCRIPT_DIR/squashfs-root/AppRun" >"$SMOKE_LOG" 2>&1
  SMOKE_STATUS=$?
elif [ "${CI:-false}" = "true" ]; then
  echo "xvfb-run is required for the headless AppImage smoke test in CI." >&2
  SMOKE_STATUS=1
else
  echo "Skipping GUI smoke test because xvfb-run is unavailable and DISPLAY is unset." >&2
fi

set -e

if [ "$SMOKE_STATUS" -ne 0 ] && [ "$SMOKE_STATUS" -ne 124 ]; then
  cat "$SMOKE_LOG" >&2
  exit "$SMOKE_STATUS"
fi

cleanup_smoke

rm -rf "$SCRIPT_DIR/squashfs-root"

VERSIONED_OUTPUT="$SCRIPT_DIR/Story_Of_Alicia_Launcher_${LAUNCHER_VERSION}_x86_64.AppImage"
mv "$OUTPUT" "$VERSIONED_OUTPUT"
OUTPUT="$VERSIONED_OUTPUT"

if [ -z "${SOA_UPDATE_HISTORY_INPUT:-}" ]; then
  UPDATE_HISTORY_TEMP="$(mktemp -d)"
  for history_base in \
      "https://github.com/Story-Of-Alicia/soa-launcher-qt/releases/latest/download" \
      "https://r2.storyofalicia.com/launcher"; do
    if wget -q "$history_base/linux_launcher_versions.json" \
        -O "$UPDATE_HISTORY_TEMP/linux_launcher_versions.json" \
        && wget -q "$history_base/linux_launcher_versions.json.sig" \
        -O "$UPDATE_HISTORY_TEMP/linux_launcher_versions.json.sig"; then
      SOA_UPDATE_HISTORY_INPUT="$UPDATE_HISTORY_TEMP/linux_launcher_versions.json"
      export SOA_UPDATE_HISTORY_INPUT
      break
    fi
  done
fi

"$SCRIPT_DIR/generate-linux-update-metadata.sh" "$LAUNCHER_VERSION" "$OUTPUT"

echo "Done. The versioned AppImage and signed update metadata are in: $SCRIPT_DIR"
