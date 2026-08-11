# Story of Alicia Launcher

The official **Story of Alicia** launcher for Linux and macOS.

The launcher can:

- Install and update the game
- Verify and repair damaged files
- Manage both supported game versions
- Set up Wine or Proton on Linux
- Detect and use Wine installations on macOS
- Sign in through Discord
- Start the game and collect useful diagnostics when something goes wrong

The launcher is designed to work for regular players without requiring knowledge of Wine, Proton, prefixes, or command-line tools.

> This project is a launcher only. It does not contain the game itself.

## Supported platforms

| Platform | Package |
|---|---|
| Linux x86_64 | AppImage |
| macOS Apple Silicon | DMG; Rosetta may be needed for Intel-only Wine |
| macOS Intel | DMG |
| Windows | Use the original Windows launcher |

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

Linux users can choose between Wine and Proton through UMU.

### macOS

1. Download the DMG.
2. Open it and move the launcher into Applications.
3. Start the launcher and follow the setup instructions.

The launcher scans for Wine on the Mac and also lets the user select a Wine app,
executable, or installation folder. On Apple Silicon, macOS may require Rosetta
when the selected Wine installation is Intel-only.

## Documentation

- [`LAUNCHER_UPDATES.md`](docs/LAUNCHER_UPDATES.md) — signed Linux launcher updates
- [`FIRST_TEST.md`](packaging/macos/FIRST_TEST.md) — macOS hardware test checklist
- [`SIGNING.md`](packaging/macos/SIGNING.md) — macOS signing and notarization

## Diagnostics

The launcher always writes `launcher.log`. On Linux and macOS, enabling
**Diagnostic Mode** in Advanced Settings adds a labeled folder for each game
run containing `alicia.log`, `wine.log`, `timeline.jsonl`, and `summary.txt`.
macOS can also add an optional host-process sample. Leave Diagnostic Mode off
for normal play.

`alicia.log` comes from a Windows x86 diagnostic hook adapted for Wine from
[SergeantSerk's log-hook project](https://github.com/SergeantSerk/log-hook).
The exact upstream snapshot, author attribution, modifications, MinHook license,
and redistribution warning are recorded in
[`third_party/alicia-log-hook/UPSTREAM.md`](third_party/alicia-log-hook/UPSTREAM.md).

On macOS, a diagnostic archive can also be created with:

```sh
./packaging/macos/collect-diagnostics.sh
```

Diagnostic archives are automatically redacted where possible, but they should still be reviewed before sharing.

## License and assets

The launcher source code is distributed under the license in `LICENSE`.

Artwork, logos, fonts, game files, and modified textless versions of existing artwork remain owned by their respective copyright holders and are not automatically covered by the launcher's source-code license.
