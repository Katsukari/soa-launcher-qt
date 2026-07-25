#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${SOA_BUILD_DIR:-$PROJECT_ROOT/build-macos-local}"
ARCH="${SOA_MACOS_ARCH:-$(uname -m)}"
BUILD_TYPE="${SOA_BUILD_TYPE:-Release}"
LOCAL_SIGN="${SOA_LOCAL_SIGN:-0}"
RUNTIME_PACKAGE="${SOA_MACOS_RUNTIME_PACKAGE:-}"

for tool in cmake ninja swift xcrun lipo otool python3; do
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

if [ -z "$RUNTIME_PACKAGE" ]; then
  RUNTIME_SEARCH_ROOT="$PROJECT_ROOT/../soa_wine_runtime/runtime/out/packages"
  if [ -d "$RUNTIME_SEARCH_ROOT" ]; then
    RUNTIME_MANIFEST="$(
      find "$RUNTIME_SEARCH_ROOT" -mindepth 3 -maxdepth 3 \
        -type f -name runtime.json -print |
        LC_ALL=C sort |
        tail -n 1
    )"
    if [ -n "$RUNTIME_MANIFEST" ]; then
      RUNTIME_PACKAGE="$(dirname "$RUNTIME_MANIFEST")"
    fi
  fi
fi

if [ -z "$RUNTIME_PACKAGE" ] ||
   [ ! -f "$RUNTIME_PACKAGE/runtime.json" ] ||
   [ ! -x "$RUNTIME_PACKAGE/payload/StoryOfAliciaRuntime.app/Contents/Resources/wine/bin/wine" ] ||
   [ ! -x "$RUNTIME_PACKAGE/payload/StoryOfAliciaRuntime.app/Contents/Resources/tools/self-test-macos.sh" ]; then
  echo "A complete Story of Alicia runtime package is required." >&2
  echo "Build ../soa_wine_runtime first or set SOA_MACOS_RUNTIME_PACKAGE." >&2
  exit 1
fi
"$SCRIPT_DIR/verify-runtime-package.py" "$RUNTIME_PACKAGE"

CMAKE_QT_ARGS=()
if [ -n "$QT_PREFIX" ]; then
  CMAKE_QT_ARGS+=("-DCMAKE_PREFIX_PATH=$QT_PREFIX")
fi

cmake \
  -S "$PROJECT_ROOT" \
  -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  "${CMAKE_QT_ARGS[@]}" \
  -DSOA_MACOS_RUNTIME_PACKAGE="$RUNTIME_PACKAGE" \
  -DSOA_REQUIRE_BUNDLED_RUNTIME=ON \
  -DBUILD_TESTING=ON

cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure

APP="$(find "$BUILD_DIR" -maxdepth 1 -type d -name '*.app' -print -quit)"
if [ -z "$APP" ]; then
  echo "No .app bundle was produced in $BUILD_DIR" >&2
  exit 1
fi

"$MACDEPLOYQT" "$APP" -always-overwrite -verbose=1

if [ "$LOCAL_SIGN" = "1" ]; then
  if ! command -v codesign >/dev/null 2>&1; then
    echo "SOA_LOCAL_SIGN=1 was requested, but codesign is unavailable." >&2
    exit 1
  fi
  "$SCRIPT_DIR/sign-app.sh" "$APP" - "$SCRIPT_DIR/entitlements.plist"
fi

RUNTIME_ROOT="$APP/Contents/Resources/Story of Alicia Runtime"
if [ ! -f "$RUNTIME_ROOT/runtime.json" ] ||
   [ ! -x "$RUNTIME_ROOT/payload/StoryOfAliciaRuntime.app/Contents/Resources/wine/bin/wine" ] ||
   [ ! -x "$RUNTIME_ROOT/payload/StoryOfAliciaRuntime.app/Contents/Resources/tools/self-test-macos.sh" ]; then
  echo "The launcher-owned runtime package is missing from the application bundle." >&2
  exit 1
fi
"$SCRIPT_DIR/verify-runtime-package.py" "$RUNTIME_ROOT"

BINARY="$APP/Contents/MacOS/soa_launcher"
ACTUAL_ARCHS="$(lipo -archs "$BINARY")"
case " $ACTUAL_ARCHS " in
  *" $ARCH "*) ;;
  *)
    echo "The launcher binary does not contain requested architecture $ARCH: $ACTUAL_ARCHS" >&2
    exit 1
    ;;
esac

COURIER="$APP/Contents/Frameworks/libsoa_network.dylib"
if [ ! -f "$COURIER" ]; then
  echo "The Swift Courier library is missing from the application bundle." >&2
  exit 1
fi
if otool -L "$BINARY" | grep -F "$PROJECT_ROOT" >/dev/null; then
  echo "The launcher still contains a build-machine library path." >&2
  exit 1
fi

if [ "$LOCAL_SIGN" = "1" ]; then
  codesign --verify --deep --strict --verbose=4 "$APP"
fi

printf '\nBuilt local test app:\n  %s\n\n' "$APP"
printf 'Bundled runtime:\n  %s\n\n' "$RUNTIME_PACKAGE"
if [ "$LOCAL_SIGN" = "1" ]; then
  printf 'Local ad-hoc signing: enabled\n'
else
  printf 'Local ad-hoc signing: skipped\n'
fi
printf 'Open it with:\n  open "%s"\n' "$APP"
printf '\nTo opt into local ad-hoc signing on a later build:\n  SOA_LOCAL_SIGN=1 ./packaging/macos/build-local.sh\n'
