#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 VERSION DMG" >&2
  exit 2
fi

VERSION="$1"
DMG="$2"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_URL="${SOA_UPDATE_BASE_URL:-https://r2.storyofalicia.com/launcher/macos}"
GITHUB_REPOSITORY="${SOA_UPDATE_GITHUB_REPOSITORY:-Story-Of-Alicia/soa-launcher-qt}"
OPENSSL_BIN="${SOA_OPENSSL:-$(command -v openssl || true)}"

PLATFORM="macos"
MANIFEST="$SCRIPT_DIR/$PLATFORM-version.json"
HISTORY="$SCRIPT_DIR/$PLATFORM-versions.json"
TEMPORARY_UPDATE_KEY=""
UPDATE_HISTORY_TEMP=""
USING_EPHEMERAL_KEY=0

cleanup() {
  if [ -n "$TEMPORARY_UPDATE_KEY" ]; then
    rm -f "$TEMPORARY_UPDATE_KEY"
  fi
  if [ -n "$UPDATE_HISTORY_TEMP" ]; then
    rm -rf "$UPDATE_HISTORY_TEMP"
  fi
}
trap cleanup EXIT

for command_name in jq curl tail od tr awk wc date mktemp; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "Required command not found: $command_name" >&2
    exit 1
  fi
done

if [ -z "$OPENSSL_BIN" ] || [ ! -x "$OPENSSL_BIN" ]; then
  echo "OpenSSL was not found. Set SOA_OPENSSL to the OpenSSL 3 executable." >&2
  exit 1
fi
if [ ! -f "$DMG" ]; then
  echo "DMG not found: $DMG" >&2
  exit 1
fi
if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+([+-][0-9A-Za-z.-]+)?$ ]]; then
  echo "Invalid launcher version: $VERSION" >&2
  exit 1
fi

if [ -z "${SOA_UPDATE_SIGNING_KEY:-}" ]; then
  TEMPORARY_UPDATE_KEY="$(mktemp)"
  if [ -n "${SOA_UPDATE_SIGNING_KEY_B64:-}" ]; then
    printf '%s' "$SOA_UPDATE_SIGNING_KEY_B64" \
      | "$OPENSSL_BIN" base64 -d -A >"$TEMPORARY_UPDATE_KEY"
  elif [ "${GITHUB_REF_TYPE:-}" = "tag" ]; then
    echo "Tagged release builds require the SOA_UPDATE_SIGNING_KEY_B64 secret." >&2
    exit 1
  else
    echo "Using an ephemeral update key for this non-release build." >&2
    "$OPENSSL_BIN" genpkey -algorithm Ed25519 -out "$TEMPORARY_UPDATE_KEY"
    USING_EPHEMERAL_KEY=1
  fi
  chmod 600 "$TEMPORARY_UPDATE_KEY"
  SOA_UPDATE_SIGNING_KEY="$TEMPORARY_UPDATE_KEY"
fi

if [ ! -f "$SOA_UPDATE_SIGNING_KEY" ]; then
  echo "SOA_UPDATE_SIGNING_KEY must point to an Ed25519 private key." >&2
  exit 1
fi

DERIVED_UPDATE_PUBLIC_KEY_HEX="$(
  "$OPENSSL_BIN" pkey -in "$SOA_UPDATE_SIGNING_KEY" -pubout -outform DER \
    | tail -c 32 \
    | od -An -v -tx1 \
    | tr -d ' \n'
)"
DEFAULT_PUBLIC_KEY_HEX="$(tr -d '[:space:]' < "$SCRIPT_DIR/../update-public-key.hex")"
EXPECTED_PUBLIC_KEY_HEX="$(printf '%s' "${SOA_UPDATE_PUBLIC_KEY_HEX:-$DEFAULT_PUBLIC_KEY_HEX}" | tr '[:upper:]' '[:lower:]')"
if [ "$USING_EPHEMERAL_KEY" -eq 0 ] \
    && [ -n "$EXPECTED_PUBLIC_KEY_HEX" ] \
    && [ "$EXPECTED_PUBLIC_KEY_HEX" != "$DERIVED_UPDATE_PUBLIC_KEY_HEX" ]; then
  echo "SOA_UPDATE_PUBLIC_KEY_HEX does not match SOA_UPDATE_SIGNING_KEY." >&2
  exit 1
fi

FILE_NAME="$(basename "$DMG")"
SHA256="$("$OPENSSL_BIN" dgst -sha256 "$DMG" | awk '{print $NF}')"
SIZE="$(wc -c <"$DMG" | tr -d ' ')"
RELEASED_AT="${SOA_UPDATE_RELEASED_AT:-$(date -u +%Y-%m-%dT%H:%M:%SZ)}"

validate_history() {
  local input="$1"
  jq -e --arg platform "$PLATFORM" '
    (.schema == 1)
    and (.platform == $platform)
    and (.releases | type == "array")
    and ((.releases | length) >= 1)
    and ((.releases | length) <= 3)
    and (all(.releases[]; .platform == $platform))
    and (([.releases[].version] | unique | length) == (.releases | length))
  ' "$input" >/dev/null
}

jq -n \
  --arg version "$VERSION" \
  --arg platform "$PLATFORM" \
  --arg file_name "$FILE_NAME" \
  --arg url "$BASE_URL/$FILE_NAME" \
  --arg github_url "https://github.com/$GITHUB_REPOSITORY/releases/download/v$VERSION/$FILE_NAME" \
  --arg sha256 "$SHA256" \
  --argjson size "$SIZE" \
  --arg released_at "$RELEASED_AT" \
  '{schema: 1, platform: $platform, version: $version,
    file_name: $file_name, url: $url, mirrors: [$github_url], sha256: $sha256, size: $size,
    released_at: $released_at, required: false}' >"$MANIFEST"

HISTORY_INPUT="${SOA_UPDATE_HISTORY_INPUT:-}"
if [ -z "$HISTORY_INPUT" ] && [ "$USING_EPHEMERAL_KEY" -eq 0 ]; then
  UPDATE_HISTORY_TEMP="$(mktemp -d)"
  if curl --fail --location --silent --show-error \
      "$BASE_URL/$PLATFORM-versions.json" \
      --output "$UPDATE_HISTORY_TEMP/$PLATFORM-versions.json" \
      && curl --fail --location --silent --show-error \
      "$BASE_URL/$PLATFORM-versions.json.sig" \
      --output "$UPDATE_HISTORY_TEMP/$PLATFORM-versions.json.sig"; then
    HISTORY_INPUT="$UPDATE_HISTORY_TEMP/$PLATFORM-versions.json"
  fi
fi

if [ -n "$HISTORY_INPUT" ] && [ -f "$HISTORY_INPUT" ]; then
  HISTORY_SIGNATURE_INPUT="${SOA_UPDATE_HISTORY_SIGNATURE_INPUT:-$HISTORY_INPUT.sig}"
  if [ ! -f "$HISTORY_SIGNATURE_INPUT" ]; then
    echo "Existing launcher history requires its detached signature: $HISTORY_SIGNATURE_INPUT" >&2
    exit 1
  fi
  VERIFY_SIGNATURE="$(mktemp)"
  VERIFY_PUBLIC_KEY="$(mktemp)"
  "$OPENSSL_BIN" base64 -d -A -in "$HISTORY_SIGNATURE_INPUT" -out "$VERIFY_SIGNATURE"
  "$OPENSSL_BIN" pkey -in "$SOA_UPDATE_SIGNING_KEY" -pubout -out "$VERIFY_PUBLIC_KEY"
  if ! "$OPENSSL_BIN" pkeyutl -verify -pubin -inkey "$VERIFY_PUBLIC_KEY" -rawin \
      -in "$HISTORY_INPUT" -sigfile "$VERIFY_SIGNATURE" >/dev/null; then
    rm -f "$VERIFY_SIGNATURE" "$VERIFY_PUBLIC_KEY"
    echo "Existing launcher history has an invalid signature." >&2
    exit 1
  fi
  if ! validate_history "$HISTORY_INPUT"; then
    rm -f "$VERIFY_SIGNATURE" "$VERIFY_PUBLIC_KEY"
    echo "Existing launcher history must contain one to three unique $PLATFORM releases." >&2
    exit 1
  fi
  rm -f "$VERIFY_SIGNATURE" "$VERIFY_PUBLIC_KEY"
  jq --slurpfile current "$MANIFEST" --arg platform "$PLATFORM" \
    'def release_fields:
       {schema, platform, version, minimum_version, message, file_name, url, mirrors,
        sha256, size, released_at, required}
       | with_entries(select(.value != null));
     {schema: 1, platform: $platform,
      releases: (([.releases[] | release_fields
                   | select(.version != $current[0].version)]
                  + [($current[0] | release_fields)])
                 | sort_by(.released_at)
                 | if length > 3 then .[-3:] else . end)}' "$HISTORY_INPUT" >"$HISTORY"
else
  jq -n --slurpfile current "$MANIFEST" --arg platform "$PLATFORM" \
    '{schema: 1, platform: $platform, releases: $current}' >"$HISTORY"
fi

if ! validate_history "$HISTORY" \
    || ! jq -e --arg version "$VERSION" \
      '([.releases[].version] | map(select(. == $version)) | length) == 1' \
      "$HISTORY" >/dev/null; then
  echo "Generated $PLATFORM launcher history is invalid or does not contain the current release exactly once." >&2
  exit 1
fi

sign_file() {
  local input="$1"
  local raw_signature
  raw_signature="$(mktemp)"
  "$OPENSSL_BIN" pkeyutl -sign -rawin -inkey "$SOA_UPDATE_SIGNING_KEY" \
    -in "$input" -out "$raw_signature"
  "$OPENSSL_BIN" base64 -A -in "$raw_signature" -out "$input.sig"
  printf '\n' >>"$input.sig"
  rm -f "$raw_signature"
}

sign_file "$MANIFEST"
sign_file "$HISTORY"

echo "Generated signed $PLATFORM update metadata:"
printf '  %s\n' "$MANIFEST" "$MANIFEST.sig" "$HISTORY" "$HISTORY.sig"
