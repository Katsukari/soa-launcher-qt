#!/usr/bin/env bash
set -euo pipefail
cc="${MINGW_CXX:-i686-w64-mingw32-g++}"
"$cc" -std=c++17 -Os -s -mwindows soa_d3d9_probe.cpp -ld3d9 -o soa-d3d9-probe.exe
