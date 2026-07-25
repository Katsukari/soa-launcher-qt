# Story of Alicia Launcher

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

| Platform                      | Status                                                               |
|-------------------------------|----------------------------------------------------------------------|
| Linux x86_64                  | AppImage  in releases                                                |
| Linux ARM64                   | On the todo list                                                     |
| macOS Intel and Apple Silicon | Experimental DMG (under heavy development with its own wine runtime) |
| Windows                       | Use the original Windows launcher                                    |

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

1. Download the DMG.
2. Open it and move the launcher into Applications.
3. Start the launcher and follow the setup instructions.

Apple Silicon Macs use the bundled Intel game runtime through Rosetta. The launcher will explain how to install Rosetta when it is required.

## Documentation

- [`BUILDING.md`](docs/BUILDING.md) — building the launcher from source
- [`CONTRIBUTING.md`](docs/CONTRIBUTING.md) — contributing code, translations, or documentation
- [`SECURITY.md`](docs/SECURITY.md) — reporting security problems
- [`SECURITY_INTEGRATION.md`](docs/SECURITY_INTEGRATION.md) — launcher and server security boundaries
- [`CONFIG_REFERENCE.md`](docs/CONFIG_REFERENCE.md) — persisted launcher configuration
- [`RUNTIME_ARCHITECTURE.md`](docs/RUNTIME_ARCHITECTURE.md) — macOS runtime package structure
- [`PLATFORM_LINUX.md`](docs/PLATFORM_LINUX.md) — Linux behavior and packaging
- [`PLATFORM_MACOS.md`](docs/PLATFORM_MACOS.md) — macOS behavior and packaging
- [`TRANSLATING.md`](docs/TRANSLATING.md) — adding or updating launcher translations

## Diagnostics

The launcher writes diagnostic logs when setup, updating, or game launching fails.

On macOS, a diagnostic archive can also be created with:

```sh
./packaging/macos/collect-diagnostics.sh
```

Diagnostic archives are automatically redacted where possible, but they should still be reviewed before sharing.

## License and assets

The launcher source code is distributed under the license in `LICENSE`.

Artwork, logos, fonts, game files, and modified textless versions of existing artwork remain owned by their respective copyright holders and are not automatically covered by the launcher's source-code license.
