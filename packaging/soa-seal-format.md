# SOA Seal v1

SOA Seal v1 is the signed-document format used for Story of Alicia launcher
update metadata.

The cryptographic primitive is standard Ed25519. SOA Seal adds a versioned,
domain-separated envelope so launcher update signatures cannot be treated as
plain signatures over arbitrary files.

## Key files

Create a key with:

```bash
./packaging/create-update-key.sh /path/to/private-directory
```

The command creates:

```text
soa-update-key.pem
soa-update-public-key.hex
soa-update-key-id.txt
```

`soa-update-key.pem` is private. `soa-update-public-key.hex` and the key ID are
public.

The launcher compiles the 64-character raw public key stored in:

```text
packaging/soa-update-public-key.hex
```

## Key ID

The public key has a human-readable identifier such as:

```text
SOA1-ABCD-1234-5678-90AB-CDEF-1234-5678-90AB
```

It is derived as:

```text
hash = SHA256("SOA-SEAL-KEY-ID-V1\0" || raw_ed25519_public_key)
key_id = "SOA1-" || uppercase_hex(hash[0:16]), grouped every 4 hex characters
```

The key ID is not a secret and is not a replacement for the public key. It is
bound into every SOA Seal signature.

## Seal sidecar

A `.seal` file contains exactly four fields:

```text
SOA-SEAL-V1
kind=manifest
key=SOA1-ABCD-1234-5678-90AB-CDEF-1234-5678-90AB
sig=BASE64_ED25519_SIGNATURE
```

The last line is a standard 64-byte Ed25519 signature encoded as base64.

Allowed document kinds are:

```text
manifest  manifest.json
history   versions.json
```

## Signed bytes

The signature is made over the following exact byte sequence:

```text
"SOA-SEAL-V1\0"
|| "kind=" || kind || "\0"
|| "key=" || key_id || "\0"
|| exact_document_bytes
```

The JSON is not parsed, normalized, re-indented, or re-encoded before signing.
The exact bytes served to the launcher are the bytes covered by the signature.

Because the document kind is part of the signed bytes, a valid manifest seal
cannot be reused for release history. Because the key ID is part of the signed
bytes and is independently derived from the compiled public key, changing the
key label also invalidates the seal.

## Verification

The launcher accepts a metadata document only when all of these are true:

1. The seal has the exact `SOA-SEAL-V1` four-line structure.
2. Its document kind matches the file being fetched.
3. Its `SOA1-...` key ID has the required format.
4. The key ID exactly matches the ID derived from the launcher's compiled
   Ed25519 public key.
5. The base64 signature decodes to exactly 64 bytes.
6. Ed25519 verification succeeds over the SOA Seal v1 signed bytes above.

There is no alternate raw-signature format accepted by the launcher.

## Publishing

Linux and macOS each use the same four metadata filenames inside separate
platform directories:

```text
manifest.json
manifest.json.seal
versions.json
versions.json.seal
```

The primary copies live in R2 under `/launcher/linux/` and `/launcher/macos/`.
The fallback copies live in the `launcher-updates` branch under `linux/` and
`macos/`.

`packaging/publish-release.sh` verifies all four seals, uploads only the
AppImage and DMG to the GitHub Release, commits the metadata to the fallback
branch, and publishes the same package and metadata bytes to R2.

Each metadata generator first retrieves the existing `versions.json` and seal
from both remote locations. If both copies exist, they must be byte-identical
and valid. The new release is merged with that history and the list is limited
to the newest three releases.

R2 package objects stay directly in each platform directory and use versioned filenames. Metadata remains at stable root URLs.
