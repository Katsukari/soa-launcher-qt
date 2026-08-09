#!/usr/bin/env bash
# Story of Alicia — Linux diagnostics collector.
# Counterpart to collect-macos.sh. Gathers everything
# needed to diagnose a failed launch into one archive, and runs the analyser
# over the newest Wine log so the bundle contains a verdict, not just raw data.

set -uo pipefail

STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
OUT_DIR="${SOA_DIAGNOSTICS_DIR:-$HOME/soa-linux-diagnostics-$STAMP}"
ARCHIVE="$OUT_DIR.tar.gz"
DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANALYSER="$SCRIPT_DIR/soa-log-analyze.py"

mkdir -p "$OUT_DIR"

{
    echo "Story of Alicia Linux diagnostic summary"
    echo "Created (UTC): $STAMP"
    echo
    echo "Kernel: $(uname -srmo 2>/dev/null)"
    [ -r /etc/os-release ] && . /etc/os-release && echo "Distribution: ${PRETTY_NAME:-unknown}"
    echo "glibc: $(getconf GNU_LIBC_VERSION 2>/dev/null || echo unknown)"
    echo "Session type: ${XDG_SESSION_TYPE:-unknown}"
    echo "Desktop: ${XDG_CURRENT_DESKTOP:-unknown}"
    echo
    echo "Wine binaries on PATH:"
    for name in wine wine64 wineserver proton; do
        path="$(command -v "$name" 2>/dev/null)" && echo "  $name -> $path ($("$path" --version 2>/dev/null | head -1))"
    done
    echo
    echo "Graphics:"
    command -v glxinfo >/dev/null 2>&1 && glxinfo -B 2>/dev/null | grep -E 'OpenGL renderer|OpenGL version|Device:' || echo "  glxinfo not installed"
    command -v vulkaninfo >/dev/null 2>&1 && vulkaninfo --summary 2>/dev/null | grep -E 'deviceName|driverInfo' || echo "  vulkaninfo not installed"
    echo
    echo "Audio:"
    command -v pactl >/dev/null 2>&1 && pactl info 2>/dev/null | grep -E 'Server Name|Default Sink' || echo "  pactl not available"
    echo
    echo "32-bit runtime present: $([ -e /lib/i386-linux-gnu ] || [ -e /usr/lib32 ] && echo yes || echo no)"
} >"$OUT_DIR/summary.txt" 2>&1

# Launcher-owned logs
for candidate in \
    "$DATA_HOME/Story of Alicia" \
    "$CONFIG_HOME/Story of Alicia" \
    "$HOME/.local/state/Story of Alicia"; do
    [ -d "$candidate" ] || continue
    mkdir -p "$OUT_DIR/launcher"
    find "$candidate" -maxdepth 3 -name '*.log' -o -maxdepth 3 -name '*.jsonl' \
        -o -maxdepth 3 -name 'summary.txt' 2>/dev/null \
        | while read -r file; do cp -a "$file" "$OUT_DIR/launcher/" 2>/dev/null; done
done

# Prefix registry (small, and it carries the graphics/audio settings)
if [ -n "${WINEPREFIX:-}" ] && [ -d "$WINEPREFIX" ]; then
    mkdir -p "$OUT_DIR/prefix"
    for reg in system.reg user.reg userdef.reg; do
        [ -f "$WINEPREFIX/$reg" ] && cp -a "$WINEPREFIX/$reg" "$OUT_DIR/prefix/"
    done
fi

# Run the analyser over the newest Wine log we found.
newest="$(find "$OUT_DIR" -name 'wine*.log' -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)"
if [ -n "$newest" ] && [ -f "$ANALYSER" ] && command -v python3 >/dev/null 2>&1; then
    alicia="$(find "$OUT_DIR" -name 'alicia*.log' | head -1)"
    if [ -n "$alicia" ]; then
        python3 "$ANALYSER" "$newest" --alicia "$alicia" --platform linux \
            --json "$OUT_DIR/analysis.json" >"$OUT_DIR/analysis.txt" 2>&1
    else
        python3 "$ANALYSER" "$newest" --platform linux \
            --json "$OUT_DIR/analysis.json" >"$OUT_DIR/analysis.txt" 2>&1
    fi
    echo "Analysis written to analysis.txt"
fi

tar -czf "$ARCHIVE" -C "$(dirname "$OUT_DIR")" "$(basename "$OUT_DIR")" 2>/dev/null

echo "Diagnostics directory: $OUT_DIR"
echo "Archive:               $ARCHIVE"
[ -f "$OUT_DIR/analysis.txt" ] && { echo; echo "--- verdict ---"; sed -n '/^VERDICT/,$p' "$OUT_DIR/analysis.txt"; }
