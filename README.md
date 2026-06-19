# soa-launcher-qt

Story Of Alicia launcher for Linux and macOS.

> **Naming:** this repository is `soa-launcher-qt`, but the built application is
> just **soa-launcher** (shown as **Story Of Alicia**). The `-qt` suffix only
> distinguishes this cross-platform port from the original Windows launcher.

## About

A Qt6/C++ port of the Story Of Alicia launcher, bringing native Linux and macOS
support to a launcher that was previously Windows-only. Handles installing,
updating, verifying, and launching the game.

On Linux and macOS the game runs through Wine/Proton, so the launcher also manages
the Wine prefix, DXVK, and a custom Wine/Proton binary path and also configuration the
Windows original never needed.

## Architecture

The launcher is primarily **C++/Qt6**, which provides the cross-platform UI and
core logic on every supported OS.

The download / verification engine is written in **Swift**, bridged to the C++ side
through a small C interface. C++ has no networking in its standard library. The
alternatives are a C-style API (libcurl) or a heavy framework (Boost.Asio) where
Swift ships first-class async networking (`URLSession`) and hashing (`CryptoKit`).

Swift is a **build-time** dependency only. The shipped binary is native linked code,
so end users never need a Swift toolchain installed, only people compiling from
source do.

## Building

Requirements:
- Qt 6
- CMake
- A C++ compiler (C++20 or later)
- A Swift toolchain (build-time only, for the download engine)

```sh
cmake -B build
cmake --build build
```

The resulting binary is `soa-launcher`.

## Platforms

| Platform | Status        |
|----------|---------------|
| Linux    | Supported     |
| macOS    | Supported     |
| Windows  | Use the original Windows launcher |

## License

See [LICENSE](LICENSE).