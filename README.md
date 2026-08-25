# Story of Alicia Launcher

[![Build macOS and Linux](https://github.com/Story-Of-Alicia/soa-launcher-qt/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/Story-Of-Alicia/soa-launcher-qt/actions/workflows/cmake-multi-platform.yml)

The official **Story of Alicia** launcher for Linux and macOS.

The launcher can:

- Install and update the game
- Verify and repair damaged files
- Manage both supported game versions
- Set up Wine or Proton on Linux
- Use the bundled Story of Alicia runtime on macOS
- Sign in through Discord
- Start the game and collect useful diagnostics when something goes wrong

The launcher is designed to work for regular players without requiring knowledge of Wine, Proton, prefixes, or command-line tools.

> This project is a launcher only. It does not contain the game itself.

## Supported platforms

| Platform                      | Status                                                    |
|-------------------------------|-----------------------------------------------------------|
| Linux x86_64                  | AppImage  in releases                                     |
| Linux ARM64                   | On the todo list                                          |
| macOS Intel and Apple Silicon | Experimental .app that you have to build yourself for now |
| Windows                       | Use the original Windows launcher                         |

## Installing the launcher

Download the newest package from the project's **Releases** page.

### Linux

1. Download the AppImage.
2. Allow it to run:

   ```sh
   chmod +x Story_Of_Alicia-x86_64.AppImage
   ```

3. Open it:

   ```sh
   ./Story_Of_Alicia-x86_64.AppImage
   ```

Linux users can choose between Wine, Proton through UMU, or a custom runtime.

### macOS

Currently the macOS launcher has to be built from source.
Read [`BUILDING.md`](docs/BUILDING.md).

You can read about why in [`PLATFORM_MACOS.md`](docs/PLATFORM_MACOS.md).

## Documentation
- [`BUILDING.md`](docs/BUILDING.md) - building the launcher from source
- [`CONTRIBUTING.md`](docs/CONTRIBUTING.md) - contributing code, translations, or documentation
- [`SECURITY.md`](docs/SECURITY_INTEGRATION.md) - launcher security
- [`CONFIG_REFERENCE.md`](docs/CONFIG_REFERENCE.md) - persisted launcher configuration
- [`PLATFORM_LINUX.md`](docs/PLATFORM_LINUX.md) - Linux behavior and packaging
- [`PLATFORM_MACOS.md`](docs/PLATFORM_MACOS.md) - macOS behavior and packaging

## License and assets

The launcher source code is distributed under the license in [`LICENSE`](LICENSE).

Artwork, logos, fonts, game files, and modified textless versions of existing artwork remain owned by their respective copyright holders and are not automatically covered by the launcher's source-code license.