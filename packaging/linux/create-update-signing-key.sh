#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 PRIVATE_KEY_OUTPUT" >&2
  exit 2
fi

OUTPUT="$1"
if [ -e "$OUTPUT" ]; then
  echo "Refusing to overwrite: $OUTPUT" >&2
  exit 1
fi

umask 077
openssl genpkey -algorithm Ed25519 -out "$OUTPUT"
PUBLIC_HEX="$(openssl pkey -in "$OUTPUT" -pubout -outform DER | tail -c 32 | od -An -v -tx1 | tr -d ' \n')"

echo "Private key created at $OUTPUT. Keep it offline and never commit it."
echo "SOA_UPDATE_PUBLIC_KEY_HEX=$PUBLIC_HEX"
echo "soa_developer_key=$PUBLIC_HEX"
