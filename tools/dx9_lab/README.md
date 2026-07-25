# Historical standalone 32-bit D3D9 probe

This Windows x86 probe is retained only to reproduce the Stage D graphics
investigation. Launcher 0.3.0 does not build, bundle, or execute it.

For an isolated manual experiment, use a 32-bit MinGW compiler:

```sh
cd tools/dx9_lab
CC=i686-w64-mingw32-g++ ./build-probe.sh
```

The probe exercises `Direct3DCreate9`, `CreateDevice`, `Clear`, `BeginScene`,
`EndScene`, and `Present`. Its result must not be treated as proof that the full
game is compatible.
