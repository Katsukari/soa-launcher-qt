# macOS platform notes

The macOS launcher is distributed as a DMG containing the Story of Alicia Launcher app.

Players can get the current launcher from https://storyofalicia.com or from the project GitHub Releases.

The current build configuration targets macOS 12 or newer.

## Architectures

The provided macOS build scripts default to a universal launcher containing x86_64 and arm64.

The Qt installation used for a universal build must contain both requested architectures. The build script verifies the launcher binary, the Swift network library, and the bundled audio helper before accepting the app.

A single universal DMG is used rather than separate Intel and Apple Silicon downloads.

## Wine runtime model

The macOS launcher uses Wine runtimes. Proton is not supported on macOS.

The launcher can use Wine found on `PATH`, a selected Wine executable, a Wine application bundle, or a directory that contains a recognized Wine layout.

Runtime discovery includes common Homebrew locations, applications in `/Applications` and the user Applications directory, Whisky libraries, CrossOver, and Game Porting Toolkit style installations.

A runtime is probed before it is accepted. The launcher checks that the Wine entry point exists, is executable, can report a version, and is usable on the current Mac.

The launcher does not currently manage a downloadable Wine runtime of its own.

## Apple Silicon and Rosetta

On Apple Silicon, an Intel only Wine runtime requires Rosetta.

The launcher inspects the selected Wine executable architecture. If the runtime contains only x86_64 code, the launcher checks whether Rosetta is available before allowing setup or launch.

When Rosetta is missing, the launcher can request the normal macOS Rosetta installation prompt and then rescan the runtime.

Universal or native arm64 Wine runtimes do not need Rosetta for the launcher process itself.

## Prefix and application data

macOS launcher data lives below the Story of Alicia directory in the user Application Support location.

The default shared Wine prefix is:

```text
~/Library/Application Support/Story of Alicia/prefixes/shared
```

The launcher configuration is stored below the same application support root in:

```text
state/config.json
```

Logs are stored below:

```text
logs
```

The launcher keeps Alicia 1.0 and Alicia 2.0 as separate game install paths inside the active prefix.

## Prefix setup

macOS prefix setup uses Wine directly.

Required Windows components are installed through Winetricks when needed.

DXVK is disabled on macOS in this launcher. The macOS path uses the Wine graphics backend and launcher compatibility settings instead of installing DXVK.

The prefix stores launcher markers used to verify that required setup has completed before the game is started.

## macOS compatibility handling

The launcher applies macOS specific runtime environment handling before Wine is started.

It adds the selected runtime directories to the executable and library search paths needed by common Wine, Whisky, CrossOver, and Game Porting Toolkit layouts.

CrossOver installations receive the expected `CX_ROOT` value when the selected executable comes from a CrossOver bundle.

The launcher also carries macOS compatibility profile handling for the game launch path. Changes in this area should be tested on real hardware because behavior can differ between Wine builds and between Intel and Apple Silicon Macs.

## Audio helper

The macOS app bundles `soa-audio-host` with the Alicia diagnostic and compatibility files.

This helper is built as a native Mach O executable and links against Apple audio frameworks.

The build checks that the helper contains every requested launcher architecture before the app is accepted.

Do not remove it from the app bundle as packaging cleanup. It is part of the macOS game launch compatibility path.

## Network layer

Network transport is implemented in Swift under `src/core/network/` and is bundled as `libsoa_network.dylib` inside the app Frameworks directory.

The build verifies that this library contains every requested architecture.

New macOS HTTP, download, DNS, and update transport work should stay in the Swift network layer. C++ should consume the result through the existing bridge and handle UI or platform integration.

## Diagnostics

The normal launcher log is written below the Story of Alicia Application Support log directory.

Diagnostic Mode creates per run diagnostic directories below the launcher application data root.

A diagnostic run can include Wine output, Alicia hook output, a timeline, a summary, and an optional host process sample.

The macOS package includes the Windows x86 Alicia injector and hook plus the native audio helper required by the macOS launch path.

## Building locally

The supported local development entry point is:

```sh
./packaging/macos/build-local.sh
```

The script uses the Xcode generator, builds the requested architectures, runs the test suite, deploys Qt into the app, and verifies that the important bundled binaries have the expected architecture slices.

The default local build is unsigned by design.

Qt can be selected with `SOA_QT_PREFIX` when the automatic Qt lookup does not find the intended installation.

A different build directory can be selected with `SOA_BUILD_DIR`.

The requested architectures can be changed with `SOA_MACOS_ARCHS`, though release builds are expected to remain universal unless the project intentionally changes that policy.

## Signing and release builds

The signed release entry point is:

```sh
./packaging/macos/build-release-macos-local.sh
```

A signed release must be built on macOS.

The release process requires a Developer ID Application identity and App Store Connect notarization credentials.

The release builder performs the local build and tests, signs the app, submits it for notarization, staples the notarization ticket, checks Gatekeeper acceptance, creates the DMG, signs and notarizes the DMG, and runs the release verification step.

Private Apple credentials and private launcher signing keys must never be committed to the repository.

More detailed credential setup is documented in `packaging/macos/SIGNING.md`.

## Release expectations

The macOS release package is named in the form:

```text
Story_Of_Alicia-<version>-macos.dmg
```

The actual release script uses the project version in the final filename.

Before publication, the release should be tested on both Apple Silicon and Intel hardware when possible.

At minimum, test first launch, runtime discovery, Rosetta handling where applicable, prefix setup, game installation, sign in, game launch, settings, launcher updates, and Diagnostic Mode.

The finished public DMG is distributed through the official Story of Alicia website and GitHub Releases.
