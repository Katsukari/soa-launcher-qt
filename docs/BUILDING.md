# Building the Story of Alicia Launcher

This page covers building, packaging, and testing the launcher.

## Players

Use the packaging script for your platform.

### Get the source

```sh
git clone https://github.com/Story-Of-Alicia/soa-launcher-qt.git
cd soa-launcher-qt
```

### Linux

The Linux build needs CMake, Ninja, Qt 6, Swift, OpenSSL, and the AppImage packaging tools.

On Ubuntu 22.04:

```sh
sudo apt-get update
sudo apt-get install --yes --no-install-recommends \
  build-essential clang cmake ninja-build \
  qt6-base-dev qt6-base-dev-tools qmake6 qt6-wayland-dev libqt6svg6-dev \
  libssl-dev desktop-file-utils file libfuse2 libgl1-mesa-dev \
  libwayland-dev libxcb-cursor0 libxkbcommon-dev libxkbcommon-x11-0 \
  binutils jq patchelf wget xvfb
```

On Arch Linux or CachyOS:

```sh
sudo pacman -S --needed \
  base-devel clang cmake ninja \
  qt6-base qt6-svg qt6-wayland openssl \
  desktop-file-utils file fuse2 jq patchelf wget xorg-server-xvfb
```

Install Swift using the official [Swift for Linux instructions](https://www.swift.org/install/linux/). The release build currently uses Swift 6.3.2.

Build the AppImage:

```sh
./packaging/linux/build-appimage.sh
```

The finished file is written to `packaging/linux/`. Open it with:

```sh
chmod +x packaging/linux/Story_Of_Alicia_Launcher_*_x86_64.AppImage
./packaging/linux/Story_Of_Alicia_Launcher_*_x86_64.AppImage
```

### macOS

Install Apple's Command Line Tools:

```sh
xcode-select --install
```

If Homebrew is not installed, install it using the command from [brew.sh](https://brew.sh/):

```sh
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Follow the PATH instruction printed by Homebrew, then install the build dependencies:

```sh
brew install cmake ninja qt openssl@3
```

Build the launcher:

```sh
./packaging/macos/build-local.sh
```

The script prints the path to the finished `.app` and the command used to open it.

To create a local DMG after the app builds:

```sh
cd build-macos-local
cpack -G DragNDrop --verbose
```

Rosetta is not needed to compile the native launcher. Apple Silicon users need Rosetta when using a wine runtime.

```sh
sudo /usr/sbin/softwareupdate --install-rosetta --agree-to-license
```

## Developers

This section covers manual builds, tests, packaging behavior, and diagnostics. Use the same platform dependencies listed above.

### Toolchain

The launcher uses:

- C++20;
- CMake 3.21.1 or newer;
- Ninja;
- Qt 6;
- Swift;
- OpenSSL 3; and
- `spdlog`, downloaded during the first CMake configuration.

CI currently uses Qt 6.8.3 and Swift 6.3.2. Matching those versions is useful when reproducing CI failures.

### Manual Linux build

```sh
cmake \
  -S . \
  -B build-linux \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_TESTING=ON

cmake --build build-linux --parallel

cmake -E env \
  QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build-linux --output-on-failure
```

Run the development executable with:

```sh
./build-linux/soa_launcher
```

Ubuntu 22.04 is the reference environment for Linux releases. AppImages built on rolling distributions can accidentally depend on a newer glibc, so use Ubuntu 22.04 or CI for public artifacts.

### Manual macOS build

The launcher is built as `arm64` on Apple Silicon and `x86_64` on Intel.

```sh
QT_PREFIX="$(brew --prefix qt)"
OPENSSL_PREFIX="$(brew --prefix openssl@3)"

cmake \
  -S . \
  -B build-macos \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_TESTING=ON \
  -DCMAKE_PREFIX_PATH="${QT_PREFIX};${OPENSSL_PREFIX}"

cmake --build build-macos --parallel
ctest --test-dir build-macos --output-on-failure
open build-macos/soa_launcher.app
```

For an official macOS release, maintainers also need the project signing identity and Apple's notarization tools. Local `.app` and DMG builds may remain unsigned. Release signing is documented in [PLATFORM_MACOS.md](PLATFORM_MACOS.md) and [SECURITY.md](SECURITY_INTEGRATION.md).

### Packaging notes

`packaging/linux/build-appimage.sh` performs a clean release build, creates the AppDir, deploys the required Qt and Swift libraries, runs portability checks, and produces the AppImage. A normal local build uses a temporary update-signing key and is not part of the official update trust chain.

`packaging/macos/build-local.sh` builds and tests the launcher, runs `macdeployqt`, checks the app architecture and Swift library paths, and can apply a local ad-hoc signature when `SOA_LOCAL_SIGN=1` is set.

Useful macOS overrides are:

| Variable | Purpose |
|---|---|
| `SOA_BUILD_DIR` | Choose the local CMake build directory |
| `SOA_MACOS_ARCH` | Build `arm64` or `x86_64` |
| `SOA_BUILD_TYPE` | Choose the CMake build type |
| `SOA_QT_PREFIX` | Point to a Qt installation |
| `MACDEPLOYQT` | Point directly to `macdeployqt` |
| `SOA_LOCAL_SIGN=1` | Ad-hoc sign the local app |

### macOS logs and diagnostics

The launcher log is stored at:

```text
~/Library/Application Support/Story of Alicia/logs/launcher.log
```

Per-launch diagnostics are stored at:

```text
~/Library/Application Support/Story of Alicia/logs/diagnostics/
```

That is the canonical macOS diagnostics directory. The launcher, collector, analyzer, and timeline summarizer should all use it.

Normal launches can create:

```text
alicia-*.timeline.jsonl
alicia-*.log
```

When developer deep diagnostics are enabled, a launch can also create:

```text
alicia-*.log.sample.txt
alicia-*.log.sample-status.txt
```

Deep diagnostics remain off by default because heavy tracing and process sampling can affect startup timing.

#### Collect a diagnostic archive

After a failed or incomplete launch, run:

```sh
./packaging/macos/collect-diagnostics.sh
```

The collector writes a ZIP to the Desktop containing:

- a system summary;
- the redacted tail of `launcher.log`;
- current Alicia and Wine host processes;
- a prefix summary; and
- up to twelve recent timelines with their matching logs and samples.

It deliberately avoids copying `config.json`, Keychain data, the full Wine prefix, and the full game directory. Redaction is best effort, so review the archive before sharing it.

#### Analyze launches manually

Summarize recent launch timelines:

```sh
python3 tools/summarize-launch-timelines.py \
  "$HOME/Library/Application Support/Story of Alicia/logs/diagnostics" \
  --limit 20
```

Analyze one launch:

```sh
python3 tools/analyze-macos-launch.py \
  /path/to/alicia.timeline.jsonl \
  /path/to/alicia.log \
  --output diagnosis.txt
```

The collector runs these tools when they are available. If an analyzer is missing, the collector still creates the raw diagnostic archive.

#### Historical D3D9 lab

`tools/dx9_lab` is a standalone 32-bit Windows D3D9 probe used during graphics testing. It is not built, bundled, or run by the launcher.

Build it with a 32-bit MinGW compiler:

```sh
cd tools/dx9_lab
CC=i686-w64-mingw32-g++ ./build-probe.sh
```

The probe tests basic Direct3D 9 device creation and presentation. A successful probe does not prove that Alicia works, so diagnostics never run it automatically.

### Related documentation

- [CONTRIBUTING.md](CONTRIBUTING.md) - contributing code, translations, or documentation
- [SECURITY.md](SECURITY_INTEGRATION.md) - launcher security and release trust
- [CONFIG_REFERENCE.md](CONFIG_REFERENCE.md) - persisted launcher configuration
- [PLATFORM_LINUX.md](PLATFORM_LINUX.md) - Linux behavior and packaging
- [PLATFORM_MACOS.md](PLATFORM_MACOS.md) - macOS behavior and packaging
