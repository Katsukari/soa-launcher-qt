#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 VERSION APPIMAGE" >&2
  exit 2
fi

VERSION="$1"
APPIMAGE="$2"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_URL="${SOA_UPDATE_BASE_URL:-https://r2.storyofalicia.com/launcher}"
GITHUB_REPOSITORY="${SOA_UPDATE_GITHUB_REPOSITORY:-Story-Of-Alicia/soa-launcher-qt}"
SIGNING_KEY="${SOA_UPDATE_SIGNING_KEY:-}"
HISTORY_INPUT="${SOA_UPDATE_HISTORY_INPUT:-}"
MANIFEST="$SCRIPT_DIR/linux_launcher_version.json"
HISTORY="$SCRIPT_DIR/linux_launcher_versions.json"

for command_name in jq openssl sha256sum stat base64; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "Required command not found: $command_name" >&2
    exit 1
  fi
done

if [ ! -f "$APPIMAGE" ]; then
  echo "AppImage not found: $APPIMAGE" >&2
  exit 1
fi
if [ -z "$SIGNING_KEY" ] || [ ! -f "$SIGNING_KEY" ]; then
  echo "SOA_UPDATE_SIGNING_KEY must point to the offline Ed25519 private key." >&2
  exit 1
fi

FILE_NAME="$(basename "$APPIMAGE")"
SHA256="$(sha256sum "$APPIMAGE" | awk '{print $1}')"
SIZE="$(stat -c '%s' "$APPIMAGE")"
RELEASED_AT="${SOA_UPDATE_RELEASED_AT:-$(date -u +%Y-%m-%dT%H:%M:%SZ)}"

jq -n \
  --arg version "$VERSION" \
  --arg file_name "$FILE_NAME" \
  --arg url "$BASE_URL/$FILE_NAME" \
  --arg github_url "https://github.com/$GITHUB_REPOSITORY/releases/download/v$VERSION/$FILE_NAME" \
  --arg sha256 "$SHA256" \
  --argjson size "$SIZE" \
  --arg released_at "$RELEASED_AT" \
  '{schema: 1, platform: "linux-x86_64", version: $version,
    file_name: $file_name, url: $url, mirrors: [$github_url], sha256: $sha256, size: $size,
    released_at: $released_at, required: false}' >"$MANIFEST"

if [ -n "$HISTORY_INPUT" ] && [ -f "$HISTORY_INPUT" ]; then
  HISTORY_SIGNATURE_INPUT="${SOA_UPDATE_HISTORY_SIGNATURE_INPUT:-$HISTORY_INPUT.sig}"
  if [ ! -f "$HISTORY_SIGNATURE_INPUT" ]; then
    echo "Existing history requires its detached signature: $HISTORY_SIGNATURE_INPUT" >&2
    exit 1
  fi
  VERIFY_SIGNATURE="$(mktemp)"
  VERIFY_PUBLIC_KEY="$(mktemp)"
  trap 'rm -f "$VERIFY_SIGNATURE" "$VERIFY_PUBLIC_KEY"' EXIT
  base64 --decode "$HISTORY_SIGNATURE_INPUT" >"$VERIFY_SIGNATURE"
  openssl pkey -in "$SIGNING_KEY" -pubout -out "$VERIFY_PUBLIC_KEY"
  if ! openssl pkeyutl -verify -pubin -inkey "$VERIFY_PUBLIC_KEY" -rawin \
      -in "$HISTORY_INPUT" -sigfile "$VERIFY_SIGNATURE" >/dev/null; then
    echo "Existing launcher history has an invalid signature." >&2
    exit 1
  fi
  rm -f "$VERIFY_SIGNATURE" "$VERIFY_PUBLIC_KEY"
  trap - EXIT
  jq --slurpfile current "$MANIFEST" \
    'def release_fields:
       {schema, platform, version, minimum_version, message, file_name, url, mirrors,
        sha256, size, released_at, required}
       | with_entries(select(.value != null));
     {schema: 1, platform: "linux-x86_64",
      releases: ([.releases[] | release_fields
                  | select(.version != $current[0].version)]
                 + [($current[0] | release_fields)])
                | sort_by(.version)}' "$HISTORY_INPUT" >"$HISTORY"
else
  jq -n --slurpfile current "$MANIFEST" \
    '{schema: 1, platform: "linux-x86_64", releases: $current}' >"$HISTORY"
fi

sign_file() {
  local input="$1"
  local raw_signature
  raw_signature="$(mktemp)"
  trap 'rm -f "$raw_signature"' RETURN
  openssl pkeyutl -sign -rawin -inkey "$SIGNING_KEY" -in "$input" -out "$raw_signature"
  base64 -w 0 "$raw_signature" >"$input.sig"
  printf '\n' >>"$input.sig"
  rm -f "$raw_signature"
  trap - RETURN
}

sign_file "$MANIFEST"
sign_file "$HISTORY"

echo "Generated signed Linux update metadata:"
printf '  %s\n' "$MANIFEST" "$MANIFEST.sig" "$HISTORY" "$HISTORY.sig"
