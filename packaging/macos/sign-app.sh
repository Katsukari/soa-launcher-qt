#!/usr/bin/env bash

set -euo pipefail

APP="${1:?Application bundle path is required}"
IDENTITY="${2:?Signing identity is required}"
ENTITLEMENTS="${3:-}"

if [ ! -d "$APP" ]; then
  echo "Application bundle does not exist: $APP" >&2
  exit 1
fi

RUNTIME_APP="$APP/Contents/Resources/Story of Alicia Runtime/payload/StoryOfAliciaRuntime.app"
if [ -d "$RUNTIME_APP" ]; then


  codesign --verify --deep --strict --verbose=4 "$RUNTIME_APP"
else
  RUNTIME_APP=""
fi

is_runtime_item() {
  [ -n "$RUNTIME_APP" ] &&
    { [ "$1" = "$RUNTIME_APP" ] || [[ "$1" == "$RUNTIME_APP/"* ]]; }
}

remove_signature() {
  codesign --remove-signature "$1" >/dev/null 2>&1 || true
}

sign_item() {
  local item="$1"
  remove_signature "$item"
  if [ "$IDENTITY" = "-" ]; then
    codesign --force --sign - "$item"
  else
    codesign --force --timestamp --options runtime --sign "$IDENTITY" "$item"
  fi
}

while IFS= read -r -d '' item; do
  if is_runtime_item "$item"; then
    continue
  fi
  if file -b "$item" | grep -q 'Mach-O'; then
    remove_signature "$item"
  fi
done < <(find "$APP/Contents" -type f -print0)

while IFS= read -r -d '' bundle; do
  if is_runtime_item "$bundle"; then
    continue
  fi
  remove_signature "$bundle"
done < <(find "$APP" -depth -type d \( \
  -name '*.framework' -o \
  -name '*.app' -o \
  -name '*.xpc' -o \
  -name '*.appex' -o \
  -name '*.bundle' \
\) ! -path "$APP" -print0)

remove_signature "$APP"

while IFS= read -r -d '' item; do
  if is_runtime_item "$item"; then
    continue
  fi
  if file -b "$item" | grep -q 'Mach-O'; then
    sign_item "$item"
  fi
done < <(find "$APP/Contents" -type f -print0)

while IFS= read -r -d '' bundle; do
  if is_runtime_item "$bundle"; then
    continue
  fi
  sign_item "$bundle"
done < <(find "$APP" -depth -type d \( \
  -name '*.framework' -o \
  -name '*.app' -o \
  -name '*.xpc' -o \
  -name '*.appex' -o \
  -name '*.bundle' \
\) ! -path "$APP" -print0)

if [ "$IDENTITY" = "-" ]; then
  if [ -n "$ENTITLEMENTS" ] && [ -f "$ENTITLEMENTS" ]; then
    codesign --force --entitlements "$ENTITLEMENTS" --sign - "$APP"
  else
    codesign --force --sign - "$APP"
  fi
elif [ -n "$ENTITLEMENTS" ] && [ -f "$ENTITLEMENTS" ]; then
  codesign --force --timestamp --options runtime --entitlements "$ENTITLEMENTS" --sign "$IDENTITY" "$APP"
else
  codesign --force --timestamp --options runtime --sign "$IDENTITY" "$APP"
fi

codesign --verify --deep --strict --verbose=4 "$APP"

if [ "$IDENTITY" != "-" ] && [ -n "$RUNTIME_APP" ]; then
  launcher_team="$(
    codesign -dvv "$APP" 2>&1 |
      sed -n 's/^TeamIdentifier=//p' |
      head -n 1
  )"
  runtime_team="$(
    codesign -dvv "$RUNTIME_APP" 2>&1 |
      sed -n 's/^TeamIdentifier=//p' |
      head -n 1
  )"
  if [ -z "$launcher_team" ] || [ "$launcher_team" = "not set" ] ||
     [ -z "$runtime_team" ] || [ "$runtime_team" = "not set" ] ||
     [ "$launcher_team" != "$runtime_team" ]; then
    echo "The bundled runtime must be signed with the launcher's Developer ID team." >&2
    echo "Launcher team: ${launcher_team:-not set}; runtime team: ${runtime_team:-not set}" >&2
    exit 1
  fi
elif [ "$IDENTITY" = "-" ]; then
  while IFS= read -r -d '' item; do
    if is_runtime_item "$item"; then
      continue
    fi
    if file -b "$item" | grep -q 'Mach-O'; then
      if codesign -dvv "$item" 2>&1 | grep -q '^TeamIdentifier='; then
        team_id="$(codesign -dvv "$item" 2>&1 | sed -n 's/^TeamIdentifier=//p' | head -n 1)"
        if [ -n "$team_id" ] && [ "$team_id" != "not set" ]; then
          echo "Unexpected TeamIdentifier after ad-hoc signing: $item ($team_id)" >&2
          exit 1
        fi
      fi
    fi
  done < <(find "$APP/Contents" -type f -print0)
fi
