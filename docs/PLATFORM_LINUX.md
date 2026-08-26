# Linux platform notes

The Linux launcher is distributed as an x86_64 AppImage.

Players can get the current launcher from https://storyofalicia.com or from the project GitHub Releases.

## Runtime model

The Linux package contains the launcher and the libraries it needs to run. It does not bundle Wine, Proton, UMU, or Winetricks as a managed runtime.

The launcher discovers compatible runtimes already installed on the system and also lets the user choose one manually.

Wine and Proton are treated as different runtime types.

A normal Wine runtime uses a Wine prefix directly.

A Proton runtime uses a compat data root and its real Wine prefix lives in the `pfx` directory below that root.

The launcher repairs the older doubled `pfx/pfx` layout when it can safely identify one created by an earlier launcher build.

## Runtime discovery

The launcher first checks normal executable lookup paths for Wine.

It also scans common runtime locations used by Steam, Lutris, Heroic, Bottles, Nix, and local Wine or Proton installations.

Steam library folders are read so Proton installations in additional Steam libraries can be found as well.

A manually selected executable or runtime directory is validated before it is accepted.

## Wine setup

Plain Wine uses the configured Wine executable and prefix.

Required Windows components are installed with Winetricks when they are missing. This means a usable host Winetricks installation is needed for plain Wine prefix setup.

DXVK is optional for plain Wine and is off by default. If it is enabled, the launcher installs it through Winetricks. A failed optional DXVK setup does not invalidate an otherwise usable prefix. The launcher turns the option back off and keeps the prefix usable with WineD3D.

## Proton and UMU

Proton is launched through `umu-run` rather than by invoking Proton as if it were a normal Wine executable.

The launcher sets its own UMU identity with `GAMEID=umu-storyofalicia` and `STORE=none`.

The selected Proton root is supplied through `PROTONPATH`.

GE Proton and UMU Proton builds can use the UMU Winetricks path when their runtime layout supports it. Required components are then installed through `umu-run winetricks` with `PROTON_VERB=runinprefix`.

Other Proton builds can fall back to host Winetricks when a usable Wine entry point and Winetricks installation are available.

The launcher does not install a second DXVK copy into Proton. Proton uses its own graphics stack. When the launcher DXVK option is disabled for Proton, `PROTON_USE_WINED3D=1` is used instead.

## Steam and Steam Deck behavior

The UMU environment starts from the current process environment so useful session values can survive when the launcher was started through Steam or Game Mode.

Conflicting Steam compatibility variables are cleared before UMU is started.

`SteamAppId` is removed while an inherited `SteamGameId` is left intact.

Steam overlay entries are removed from `LD_PRELOAD` because they can interfere with the compatibility process. Other preload entries are preserved.

A `TMPDIR` value is kept when the directory exists and removed when it points to a missing path.

This behavior is intentional and should be considered when changing process environment handling.

## Prefix and game paths

The default Linux Wine prefix is stored below the user home directory in `soa-launcher`.

For Proton, the configured path represents the compat data root and the launcher works with its `pfx` child as the actual prefix.

Alicia 1.0 and Alicia 2.0 keep separate game install paths inside the active prefix.

The launcher rejects or repairs configuration that points the active game installation outside the selected prefix.

## Game installation and repair

Game downloads, update checks, integrity work, and repairs use the Swift Courier network layer.

The two supported game versions have separate content endpoints and install markers.

The launcher verifies the configured game state before offering the normal launch path. Repair operations work against the selected game version and its configured install directory.

## Network layer

Network transport is implemented in Swift under `src/core/network/`.

Linux builds link the Swift network library and use OpenSSL 3 for the SOA Seal verification code used by launcher update metadata.

New Linux network work should stay in the Swift network layer rather than adding a second HTTP implementation in C++.

## Diagnostics

`launcher.log` is always written through spdlog in the launcher application data directory.

Diagnostic Mode creates a separate run directory for game diagnostics. Depending on the launch path it can contain `alicia.log`, `wine.log`, `timeline.jsonl`, and `summary.txt`.

The Alicia diagnostic hook is a Windows x86 component built with MinGW and packaged with release builds.

Release builds require the hook to build successfully.

## Desktop integration

The AppImage includes the launcher desktop entry, icon, Qt platform plugins, required Qt libraries, the Swift runtime pieces required by the launcher, translations, and the diagnostic hook.

Both XCB and Wayland platform support are included by the release packaging path.

The package is intended to run without depending on the build machine Qt installation.

## Building locally

For normal development, configure with Ninja and build the CMake project.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
```

Run the tests headlessly with:

```sh
QT_QPA_PLATFORM=offscreen LANG=C.UTF-8 LC_ALL=C.UTF-8 \
ctest --test-dir build --output-on-failure
```

The provided local AppImage build entry point is:

```sh
./packaging/linux/build-local.sh
```

The release builder is:

```sh
./packaging/linux/build-release-linux-local.sh
```

The release builder uses portable x86_64 compiler settings, enables the full test suite, requires the diagnostic hook, builds the AppImage, and runs portability checks before the release is accepted.

## Release expectations

The Linux release package is named in the form:

```text
Story_Of_Alicia-<version>-x86_64.AppImage
```

The actual release script uses the project version in the final filename.

A release AppImage should be tested on a clean system that does not have the build machine Qt or Swift development environment available.

Do not solve portability problems by requiring users to replace system libraries such as glibc.
