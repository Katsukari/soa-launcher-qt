#!/usr/bin/env bash

set -euo pipefail

APP_SUPPORT="$HOME/Library/Application Support/Story of Alicia"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
OUT_DIR="${SOA_DIAGNOSTICS_DIR:-$HOME/Desktop/SOA-macOS-diagnostics-$STAMP}"
ARCHIVE="$OUT_DIR.zip"

mkdir -p "$OUT_DIR"

{
  echo "Story of Alicia macOS diagnostic summary"
  echo "Created (UTC): $STAMP"
  echo
  /usr/bin/sw_vers 2>/dev/null || true
  echo "Host architecture: $(/usr/bin/uname -m)"
  echo "Hardware model: $(/usr/sbin/sysctl -n hw.model 2>/dev/null || echo unknown)"
  echo "Logical CPUs: $(/usr/sbin/sysctl -n hw.logicalcpu 2>/dev/null || echo unknown)"
  echo "Memory bytes: $(/usr/sbin/sysctl -n hw.memsize 2>/dev/null || echo unknown)"
  if /usr/bin/arch -x86_64 /usr/bin/true >/dev/null 2>&1; then
    echo "Rosetta x86_64 execution: available"
  else
    echo "Rosetta x86_64 execution: unavailable"
  fi
  echo
  echo "Display summary:"
  /usr/sbin/system_profiler SPDisplaysDataType -detailLevel mini 2>/dev/null \
    | /usr/bin/grep -E 'Chipset Model:|Resolution:|Main Display:|Retina:|Metal Support:' \
    || true
} > "$OUT_DIR/system.txt"

LOG="$APP_SUPPORT/logs/launcher.log"
if [ -f "$LOG" ]; then



  /usr/bin/tail -n 12000 "$LOG" \
    | /usr/bin/sed -E \
        -e "s|$HOME|~|g" \
        -e 's/(-OP[[:space:]]+)\[[^]]*\]/\1[REDACTED]/g' \
        -e 's/([?&](token|code|state|oauth_state|launcher_state)=)[^&[:space:]]+/\1[REDACTED]/g' \
        -e 's/(Bearer[[:space:]]+)[A-Za-z0-9._~+\/-]+/\1[REDACTED]/g' \
        -e 's/("([Tt][Oo][Kk][Ee][Nn]|[Pp][Aa][Ss][Ss][Ww][Oo][Rr][Dd]|[Ss][Ee][Cc][Rr][Ee][Tt])"[[:space:]]*:[[:space:]]*")[^"]*/\1[REDACTED]/g' \
    > "$OUT_DIR/launcher-tail-redacted.log"
else
  echo "No launcher log was found at ~/Library/Application Support/Story of Alicia/logs/launcher.log" \
    > "$OUT_DIR/launcher-tail-redacted.log"
fi

{
  echo "Current Alicia/Wine host processes (PID, parent PID, elapsed time, command):"
  /bin/ps -axo pid=,ppid=,etime=,command= 2>/dev/null \
    | /usr/bin/grep -Ei '[A]licia\.exe|[w]ine(64)?-preloader|[w]ineserver' \
    | /usr/bin/sed -E \
        -e "s|$HOME|~|g" \
        -e 's/(-OP[[:space:]]+)\[[^]]*\]/\1[REDACTED]/g' \
    || echo "No matching Alicia/Wine process was present when diagnostics were collected."
  echo
  echo "The launcher log line 'attached diagnostics to Alicia.exe host PID' identifies"
  echo "the actual host process selected for diagnostics. Prefix tasklist checks remain"
  echo "the authoritative source for Alicia's Windows-process lifetime."
} > "$OUT_DIR/game-processes.txt"


DIAGNOSTICS="$APP_SUPPORT/logs/diagnostics"
mkdir -p "$OUT_DIR/alicia-launches"
if [ -d "$DIAGNOSTICS" ]; then
  while IFS= read -r run; do
    [ -n "$run" ] || continue
    label="$(basename "$run")"
    destination="$OUT_DIR/alicia-launches/$label"
    mkdir -p "$destination"
    for artifact in \
      "$run/summary.txt" \
      "$run/timeline.jsonl" \
      "$run/wine.log" \
      "$run/alicia.log" \
      "$run/host-sample.txt" \
      "$run/host-sample-status.txt"; do
      [ -f "$artifact" ] || continue
      /usr/bin/sed -E \
        -e "s|$HOME|~|g" \
        -e 's/(-OP[[:space:]]+)\[[^]]*\]/\1[REDACTED]/g' \
        -e 's/([?&](token|code|state|oauth_state|launcher_state)=)[^&[:space:]]+/\1[REDACTED]/g' \
        -e 's/(Bearer[[:space:]]+)[A-Za-z0-9._~+\/-]+/\1[REDACTED]/g' \
        "$artifact" > "$destination/$(basename "$artifact")"
    done
  done < <(/bin/ls -1dt "$DIAGNOSTICS"/run-* 2>/dev/null | /usr/bin/head -n 12 || true)
fi

PREFIX="$APP_SUPPORT/prefixes/shared"
{
  echo "Prefix exists: $([ -d "$PREFIX/drive_c" ] && echo yes || echo no)"
  echo "system.reg exists: $([ -f "$PREFIX/system.reg" ] && echo yes || echo no)"
  echo "user.reg exists: $([ -f "$PREFIX/user.reg" ] && echo yes || echo no)"
  echo "WoW64 syswow64 exists: $([ -d "$PREFIX/drive_c/windows/syswow64" ] && echo yes || echo no)"
  if [ -f "$PREFIX/system.reg" ]; then
    /usr/bin/grep -m 1 -E '^#arch=' "$PREFIX/system.reg" || true
  fi
  echo
  echo "Relevant game and compatibility files found under drive_c (paths have home replaced with ~):"
  if [ -d "$PREFIX/drive_c" ]; then
    /usr/bin/find "$PREFIX/drive_c" -type f \
      \( -iname 'Alicia.exe' -o -iname 'alice.cfg' -o -iname 'cudart32_30_9.dll' \
         -o -iname 'PhysXCore.dll' -o -iname 'PhysXLoader.dll' -o -iname '*.dmp' \) \
      -print 2>/dev/null | /usr/bin/sed "s|$HOME|~|g" || true
  fi
} > "$OUT_DIR/prefix-summary.txt"

cat > "$OUT_DIR/REVIEW_BEFORE_SHARING.txt" <<'TEXT'
Review every file in this folder before sharing it.

The collector deliberately does not copy config.json, .env, Keychain data, the
full Wine prefix, or the full game directory. It applies best-effort redaction
to the launcher log, but usernames, local folder names, URLs, or other private
information may still remain.
TEXT

rm -f "$ARCHIVE"
/usr/bin/ditto -c -k --keepParent "$OUT_DIR" "$ARCHIVE"
printf 'Created diagnostic bundle:\n  %s\n\nReview it before sharing.\n' "$ARCHIVE"



ANALYSER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/soa-log-analyze.py"
NEWEST_RUN="$(ls -dt "$HOME/Library/Application Support/Story of Alicia/logs/diagnostics/"*/ 2>/dev/null | head -1)"
if [ -n "$NEWEST_RUN" ] && [ -f "$ANALYSER" ] && command -v python3 >/dev/null 2>&1; then
    python3 "$ANALYSER" "$NEWEST_RUN" --platform darwin >"$OUT_DIR/analysis.txt" 2>&1 \
        && echo "Analysis written to $OUT_DIR/analysis.txt"
fi
