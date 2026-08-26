# Building the Story of Alicia Launcher

This page covers the normal ways to build the launcher from source. The packaging scripts already contain the project specific build steps, so most contributors do not need to reproduce them by hand.

## Requirements

| Platform | Required tools |
|---|---|
| Linux | C++20 compiler, CMake 3.21.1 or newer, Ninja, Qt 6, Swift, OpenSSL 3 |
| macOS | Xcode Command Line Tools, CMake, Qt 6, Swift from Xcode |

A full launcher build also uses a 32 bit MinGW toolchain for the Alicia diagnostic hook. The expected compiler names are `i686-w64-mingw32-gcc` and `i686-w64-mingw32-g++`.

`spdlog` is downloaded by CMake when it is not already available.

## Get the source

```sh
git clone https://github.com/Story-Of-Alicia/soa-launcher-qt.git
cd soa-launcher-qt
```

## Linux

### Development build

For normal development work, build directly with CMake and Ninja.

```sh
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON

cmake --build build-linux --parallel
ctest --test-dir build-linux --output-on-failure
```

Run the launcher with:

```sh
./build-linux/soa_launcher
```

If the 32 bit MinGW toolchain is missing, a normal developer build can still be created without the optional Alicia diagnostic hook.

### Local AppImage

Use the local packaging script when you want an AppImage for testing.

```sh
./packaging/linux/build-local.sh
```

The script creates:

```text
packaging/linux/Story_Of_Alicia-x86_64.AppImage
```

This is a local build. It does not create signed launcher update metadata and it does not enforce the release portability policy.

### Release AppImage

Maintainers use:

```sh
SOA_UPDATE_SIGNING_KEY=/path/to/soa-update-key.pem \
  ./packaging/linux/build-release-linux-local.sh
```

The release script runs the project tests, performs the Linux portability checks, creates the versioned AppImage, and generates signed update metadata.

## macOS

### Local app

Install Xcode Command Line Tools and Qt 6, then run:

```sh
./packaging/macos/build-local.sh
```

The script builds the launcher, runs the tests, deploys the Qt libraries, checks the application bundle, and prints the path to the finished unsigned `.app`.

The default build targets both Intel and Apple Silicon. The Qt installation must contain every architecture requested by the build. For a single architecture developer build, set `SOA_MACOS_ARCHS` to either `arm64` or `x86_64`.

Example for Apple Silicon:

```sh
SOA_MACOS_ARCHS=arm64 ./packaging/macos/build-local.sh
```

### Release DMG

Official macOS releases use:

```sh
./packaging/macos/build-release-macos-local.sh
```

The release script requires the project signing identity, Apple notarization credentials, and the launcher update signing key. The signing details are documented in `packaging/macos/SIGNING.md`.

## Tests

For an existing CMake build directory, run:

```sh
ctest --test-dir build-linux --output-on-failure
```

On macOS, `build-local.sh` already runs the tests as part of the build.

## Notes

The launcher supports Linux and macOS only.

Linux release builds should be made in the project release environment rather than on a rolling distribution when the result is intended for public use.

Local builds are for development and testing. Official release builds also perform the signing and verification steps required by the launcher update system.
