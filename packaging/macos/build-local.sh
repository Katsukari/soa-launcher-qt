#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${SOA_BUILD_DIR:-$PROJECT_ROOT/build-macos-local}"
ARCHS="${SOA_MACOS_ARCHS:-${SOA_MACOS_ARCH:-x86_64;arm64}}"
BUILD_TYPE="${SOA_BUILD_TYPE:-Release}"
LOCAL_SIGN="${SOA_LOCAL_SIGN:-0}"

for tool in cmake swift xcrun lipo otool file i686-w64-mingw32-gcc i686-w64-mingw32-g++; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Required tool not found: $tool" >&2
    exit 1
  fi
done

QT_PREFIX="${SOA_QT_PREFIX:-${QT_ROOT_DIR:-}}"
if [ -z "$QT_PREFIX" ] && command -v qtpaths6 >/dev/null 2>&1; then
  QT_PREFIX="$(qtpaths6 --query QT_INSTALL_PREFIX 2>/dev/null || true)"
fi
if [ -z "$QT_PREFIX" ] && command -v qmake6 >/dev/null 2>&1; then
  QT_PREFIX="$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || true)"
fi
if [ -z "$QT_PREFIX" ] && command -v qmake >/dev/null 2>&1; then
  QT_VERSION="$(qmake -query QT_VERSION 2>/dev/null || true)"
  if [[ "$QT_VERSION" == 6.* ]]; then
    QT_PREFIX="$(qmake -query QT_INSTALL_PREFIX 2>/dev/null || true)"
  fi
fi
if [ -z "$QT_PREFIX" ] && command -v brew >/dev/null 2>&1; then
  QT_PREFIX="$(brew --prefix qt 2>/dev/null || brew --prefix qt@6 2>/dev/null || true)"
fi

MACDEPLOYQT="${MACDEPLOYQT:-$(command -v macdeployqt || true)}"
if [ -z "$MACDEPLOYQT" ] && [ -n "$QT_PREFIX" ] && [ -x "$QT_PREFIX/bin/macdeployqt" ]; then
  MACDEPLOYQT="$QT_PREFIX/bin/macdeployqt"
fi
if [ -z "$MACDEPLOYQT" ] || [ ! -x "$MACDEPLOYQT" ]; then
  echo "macdeployqt was not found. Install Qt 6 or set SOA_QT_PREFIX/MACDEPLOYQT." >&2
  exit 1
fi

CMAKE_QT_ARGS=()
if [ -n "$QT_PREFIX" ]; then
  CMAKE_QT_ARGS+=("-DCMAKE_PREFIX_PATH=$QT_PREFIX")
fi

cmake \
  -S "$PROJECT_ROOT" \
  -B "$BUILD_DIR" \
  -G Xcode \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  "${CMAKE_QT_ARGS[@]}" \
  -DSOA_REQUIRE_ALICIA_LOG_HOOK=ON \
  -DBUILD_TESTING=ON

cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --parallel
ctest --test-dir "$BUILD_DIR" -C "$BUILD_TYPE" --output-on-failure

apps=("$BUILD_DIR/$BUILD_TYPE"/*.app)
if [ "${#apps[@]}" -ne 1 ] || [ ! -d "${apps[0]}" ]; then
  printf 'Expected exactly one app bundle in %s/%s, found: %s\n' \
    "$BUILD_DIR" "$BUILD_TYPE" "${apps[*]}" >&2
  exit 1
fi
APP="${apps[0]}"

"$MACDEPLOYQT" "$APP" -always-overwrite -verbose=1

if [ "$LOCAL_SIGN" = "1" ]; then
  if ! command -v codesign >/dev/null 2>&1; then
    echo "SOA_LOCAL_SIGN=1 was requested, but codesign is unavailable." >&2
    exit 1
  fi
  "$SCRIPT_DIR/sign-app.sh" "$APP" - "$SCRIPT_DIR/entitlements.plist"
fi

BINARY="$APP/Contents/MacOS/soa_launcher"
ACTUAL_ARCHS="$(lipo -archs "$BINARY")"
IFS=';' read -r -a REQUESTED_ARCHS <<< "$ARCHS"
for requested_arch in "${REQUESTED_ARCHS[@]}"; do
  case " $ACTUAL_ARCHS " in
    *" $requested_arch "*) ;;
    *)
      echo "The launcher binary does not contain requested architecture $requested_arch: $ACTUAL_ARCHS" >&2
      exit 1
      ;;
  esac
done

COURIER="$APP/Contents/Frameworks/libsoa_network.dylib"
if [ ! -f "$COURIER" ]; then
  echo "The Swift Courier library is missing from the application bundle." >&2
  exit 1
fi
COURIER_ARCHS="$(lipo -archs "$COURIER")"
for requested_arch in "${REQUESTED_ARCHS[@]}"; do
  case " $COURIER_ARCHS " in
    *" $requested_arch "*) ;;
    *)
      echo "The Swift network library does not contain requested architecture $requested_arch: $COURIER_ARCHS" >&2
      exit 1
      ;;
  esac
done
HOOK_ROOT="$APP/Contents/Resources/alicia-log-hook"
AUDIO_HOST="$HOOK_ROOT/soa-audio-host"
if [ ! -s "$AUDIO_HOST" ]; then
  echo "The bundled CoreAudio helper is missing: $AUDIO_HOST" >&2
  exit 1
fi
AUDIO_HOST_ARCHS="$(lipo -archs "$AUDIO_HOST")"
for requested_arch in "${REQUESTED_ARCHS[@]}"; do
  case " $AUDIO_HOST_ARCHS " in
    *" $requested_arch "*) ;;
    *)
      echo "The audio host does not contain requested architecture $requested_arch: $AUDIO_HOST_ARCHS" >&2
      exit 1
      ;;
  esac
done
for hook_artifact in SoaAliciaLogInjector.exe SoaAliciaLogHook.dll README.md MINHOOK_LICENSE.txt; do
  if [ ! -s "$HOOK_ROOT/$hook_artifact" ]; then
    echo "The bundled Alicia injector/compatibility component is missing: $HOOK_ROOT/$hook_artifact" >&2
    exit 1
  fi
done
for hook_artifact in SoaAliciaLogInjector.exe SoaAliciaLogHook.dll; do
  hook_description="$(file -b "$HOOK_ROOT/$hook_artifact")"
  if [[ "$hook_description" != *"PE32 executable"* ]] \
      || [[ "$hook_description" == *"PE32+"* ]]; then
    echo "The Alicia injector/compatibility component is not Windows x86 PE32: $hook_description" >&2
    exit 1
  fi
done
if otool -L "$BINARY" | grep -F "$PROJECT_ROOT" >/dev/null; then
  echo "The launcher still contains a build-machine library path." >&2
  exit 1
fi

if [ "$LOCAL_SIGN" = "1" ]; then
  codesign --verify --deep --strict --verbose=4 "$APP"
fi

printf '\nBuilt local test app:\n  %s\n\n' "$APP"
if [ "$LOCAL_SIGN" = "1" ]; then
  printf 'Local ad-hoc signing: enabled\n'
else
  printf 'Local ad-hoc signing: skipped\n'
fi
printf 'Open it with:\n  open "%s"\n' "$APP"
printf '\nTo opt into local ad-hoc signing on a later build:\n  SOA_LOCAL_SIGN=1 ./packaging/macos/build-local.sh\n'
