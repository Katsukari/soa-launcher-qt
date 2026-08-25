# macOS signing and local release guide

The macOS release is built locally. GitHub Actions is not part of the signing
or release process described here.

The Apple credentials can be prepared from Linux, but the final signed and
notarized release must be built on macOS because the release scripts use
Apple's `codesign`, `notarytool`, `stapler`, Gatekeeper, and DMG tools.

There are two macOS build entry points:

- `packaging/macos/build-local.sh` builds and tests an **unsigned** `.app`.
  It does not require signing, notarization, or update-signing credentials.
- `packaging/macos/build-release-macos-local.sh` is the complete release
  entry point. It builds, tests, signs, notarizes, staples, creates the DMG,
  verifies it, and generates signed launcher-update metadata automatically.

The remaining `.sh` files in `packaging/macos/` are shared helpers used by the
release entry point. They are not alternative release workflows.

Never commit a `.p12`, `.p8`, private update key, password, or other private
signing material to the repository.

## Apple quick links

These are the Apple pages used by this guide:

- [Apple Developer Program](https://developer.apple.com/programs/)
- [Certificates, Identifiers & Profiles → Certificates](https://developer.apple.com/account/resources/certificates/list)
- [Apple: Create Developer ID certificates](https://developer.apple.com/help/account/certificates/create-developer-id-certificates/)
- [App Store Connect → Users and Access → Integrations](https://appstoreconnect.apple.com/access/integrations/api)
- [Apple: App Store Connect API / API keys](https://developer.apple.com/help/app-store-connect/get-started/app-store-connect-api)
- [Apple: Notarizing macOS software before distribution](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)
- [Apple Developer ID overview](https://developer.apple.com/developer-id/)

## 1. Create the Developer ID Application certificate from Linux

An active [Apple Developer Program](https://developer.apple.com/programs/) membership is required.

### Generate the private key and CSR

Install OpenSSL, then create a private RSA key and certificate signing request:

```bash
sudo apt install openssl

umask 077

openssl genrsa -out developer-id.key 2048

openssl req -new \
  -key developer-id.key \
  -out developer-id.csr \
  -subj "/CN=Story of Alicia Release Key/emailAddress=YOUR_EMAIL"
```

`CN` is only a descriptive name for the key. Use an email address you control.
Country, city, state, and organization fields can be omitted if they are not
needed. If you want to include a real two-letter country code, add `/C=XX`
before `/CN`.

Keep `developer-id.key` private. It is the private key that will later be
combined with the certificate Apple issues.

### Request the certificate from Apple

Open [Certificates, Identifiers & Profiles → Certificates](https://developer.apple.com/account/resources/certificates/list), then create a
**Developer ID Application** certificate and upload `developer-id.csr`.

Apple's step-by-step reference is [Create Developer ID certificates](https://developer.apple.com/help/account/certificates/create-developer-id-certificates/).

Use **Developer ID Application**, not Developer ID Installer. The launcher is
distributed as a DMG rather than a PKG.

Download the resulting certificate. The examples below assume it is named:

```text
developerID_application.cer
```

Convert Apple's DER certificate to PEM:

```bash
openssl x509 \
  -inform DER \
  -in developerID_application.cer \
  -out developerID_application.pem
```

Combine the certificate with the original private key into a password-protected
`.p12`:

```bash
openssl pkcs12 -export \
  -legacy \
  -inkey developer-id.key \
  -in developerID_application.pem \
  -out developer-id.p12 \
  -name "Story of Alicia Developer ID" \
  -keypbe PBE-SHA1-3DES \
  -certpbe PBE-SHA1-3DES \
  -macalg sha1
```

Use a strong, non-empty export password.

Confirm that the `.p12` can be opened before transferring it:

```bash
openssl pkcs12 \
  -legacy \
  -in developer-id.p12 \
  -info \
  -noout
```

You can also print the certificate's SHA-1 fingerprint:

```bash
openssl x509 \
  -inform DER \
  -in developerID_application.cer \
  -noout \
  -fingerprint \
  -sha1 \
  | cut -d= -f2 \
  | tr -d ':\n'
```

The release script can use that 40-character fingerprint as
`SOA_DEVELOPER_IDENTITY`.

If exactly one Developer ID Application identity is installed on the Mac,
`build-release-macos-local.sh` selects it automatically, so setting the
fingerprint is optional.

### Developer ID identity privacy

The fingerprint can be used to select the signing identity without putting the
full identity string into shell configuration, but it does not hide the
identity embedded in the signed application.

An individual Apple Developer membership produces a Developer ID certificate
containing the member's verified certificate identity and Team ID.

## 2. Create the App Store Connect notarization key

The App Store Connect `.p8` key is separate from the Developer ID certificate.
It is not created from the `.cer`, `.pem`, `.key`, or `.p12`.

In [App Store Connect → Users and Access → Integrations](https://appstoreconnect.apple.com/access/integrations/api):

1. Open **Users and Access → Integrations**.
2. Select **Team Keys**.
3. Generate an API key for the release/notarization process.
4. Record its **Key ID** and **Issuer ID**.
5. Download the `AuthKey_*.p8` file and store it securely.

Apple's API-key documentation is [App Store Connect API](https://developer.apple.com/help/app-store-connect/get-started/app-store-connect-api).

Apple only provides the private `.p8` download when the key is created, so keep
a secure backup.

The local release scripts use the `.p8` file directly with `xcrun notarytool`.
It is not imported into the macOS keychain.

For Apple's explanation of notarization, `notarytool`, and stapling, see
[Notarizing macOS software before distribution](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution).

## 3. Create the launcher update-signing key from Linux

Apple code signing and launcher update signing are separate systems.

The DMG is signed with the Apple Developer ID identity. Launcher self-update
metadata is signed with an Ed25519 private key whose public half is committed
in:

```text
packaging/soa-update-public-key.hex
```

Generate the update-signing key once on a trusted system:

```bash
sudo apt install openssl xxd

umask 077

openssl genpkey \
  -algorithm Ed25519 \
  -out soa-update-ed25519.pem
```

Print the 64-character public-key value:

```bash
openssl pkey \
  -in soa-update-ed25519.pem \
  -pubout \
  -outform DER \
  | tail -c 32 \
  | xxd -p -c 64
```

Put that public value in:

```text
packaging/soa-update-public-key.hex
```

Keep the private file:

```text
soa-update-ed25519.pem
```

offline and backed up securely.

Do not rotate this key casually. Existing launcher installations trust the
public key that was compiled into them. Losing the private key prevents new
metadata from being signed with the key those installations already trust.

`build-release-macos-local.sh` requires the real update-signing key because it
generates release metadata automatically. It will not intentionally complete a
release using an ephemeral update key.

## 4. Move the release credentials to the Mac

Securely transfer these private files to the Mac that will produce the release:

```text
developer-id.p12
AuthKey_XXXXXXXXXX.p8
soa-update-ed25519.pem
```

### Import the Developer ID certificate

For background on Developer ID distribution and Gatekeeper, see
[Apple Developer ID](https://developer.apple.com/developer-id/).

Import `developer-id.p12` into the login keychain using **Keychain Access**.
The certificate and its private key must both be present.

Verify that macOS sees the signing identity:

```bash
security find-identity -v -p codesigning
```

You should see a valid:

```text
Developer ID Application: ...
```

identity.

The `.p8` notarization key and Ed25519 update-signing key do **not** need to be
imported into Keychain Access. Keep them in a private directory readable only
by the release user.

For example:

```bash
mkdir -p "$HOME/private"
chmod 700 "$HOME/private"
```

Then place the private files there and restrict their permissions:

```bash
chmod 600 \
  "$HOME/private/AuthKey_XXXXXXXXXX.p8" \
  "$HOME/private/soa-update-ed25519.pem"
```

## 5. Prepare the Mac build environment

The build scripts expect the normal project build dependencies plus Apple's
developer tools.

At minimum, the release machine needs the tools checked by the scripts,
including:

```text
cmake
codesign
cpack
ditto
hdiutil
lipo
security
spctl
xcrun
swift
file
otool
i686-w64-mingw32-gcc
i686-w64-mingw32-g++
```

The update metadata generator also uses:

```text
OpenSSL 3
jq
curl
```

A complete Qt 6 macOS installation is required. The default release is
universal:

```text
x86_64 + arm64
```

so the Qt installation selected by `SOA_QT_PREFIX` must contain both
architectures.

The build scripts verify the actual architectures before continuing.

## 6. Build the signed and notarized release

From the repository root, configure the paths and IDs for the current shell:

```bash
export SOA_QT_PREFIX="$HOME/Qt/6.9.3/macos"

export SOA_NOTARY_KEY="$HOME/private/AuthKey_XXXXXXXXXX.p8"
export SOA_NOTARY_KEY_ID="XXXXXXXXXX"
export SOA_NOTARY_ISSUER_ID="00000000-0000-0000-0000-000000000000"

export SOA_UPDATE_SIGNING_KEY="$HOME/private/soa-update-ed25519.pem"
```

If more than one Developer ID Application identity is installed, select the
correct one explicitly:

```bash
export SOA_DEVELOPER_IDENTITY="YOUR_40_CHARACTER_SHA1_FINGERPRINT"
```

If exactly one Developer ID Application identity is installed, that variable
can normally be omitted.

If the default `openssl` is not OpenSSL 3, point the metadata generator at the
correct executable:

```bash
export SOA_OPENSSL="/path/to/openssl"
```

For a Homebrew OpenSSL 3 installation, that can for example be:

```bash
export SOA_OPENSSL="$(brew --prefix openssl@3)/bin/openssl"
```

Run the complete release:

```bash
./packaging/macos/build-release-macos-local.sh
```

The release script already performs the complete chain:

```text
build-local.sh
    ↓
build + tests
    ↓
macdeployqt
    ↓
sign-app.sh
    ↓
notarize app archive
    ↓
staple + Gatekeeper check
    ↓
create DMG
    ↓
sign DMG
    ↓
notarize DMG
    ↓
staple + Gatekeeper check
    ↓
generate-macos-update-metadata.sh
    ↓
verify-release.sh
```

Do not manually repeat these signing/notarization steps unless you are
debugging a helper script.

The default DMG output is:

```text
build-macos-release-local/Story_Of_Alicia-<version>-macos.dmg
```

Use a fresh `SOA_BUILD_DIR` rather than renaming an already configured CMake
build directory. CMake records absolute build paths.

To use another build directory:

```bash
export SOA_BUILD_DIR="$PWD/build-macos-release-test"
./packaging/macos/build-release-macos-local.sh
```

## 7. Update metadata is generated automatically

A successful release automatically runs:

```text
packaging/macos/generate-macos-update-metadata.sh
```

The current generator produces the signed macOS metadata files in
`packaging/macos/`:

```text
manifest.json
manifest.json.seal
versions.json
versions.json.seal
```

The history generator keeps at most three releases.

When the real update key is used, it verifies that the private key matches the
public key in `packaging/soa-update-public-key.hex`. If existing remote history is
available, it also verifies that history before extending it.

You therefore do not need a separate metadata-generation command for a normal
release.

## 8. Verify an existing DMG again

The release entry point already runs the full verification helper.

To rerun the audit later without rebuilding:

```bash
./packaging/macos/verify-release.sh \
  build-macos-release-local/Story_Of_Alicia-<version>-macos.dmg \
  packaging/macos/manifest.json \
  packaging/macos/manifest.json.seal \
  packaging/macos/versions.json \
  packaging/macos/versions.json.seal
```

`verify-release.sh` checks the release independently, including signatures,
stapled tickets, Gatekeeper assessment, architectures, deployment target,
nested Mach-O signatures, and non-portable library paths.

You can also use Apple's checks directly when diagnosing a problem:

```bash
codesign --verify \
  --strict \
  --verbose=4 \
  build-macos-release-local/Story_Of_Alicia-<version>-macos.dmg

xcrun stapler validate \
  build-macos-release-local/Story_Of_Alicia-<version>-macos.dmg

spctl --assess \
  --type open \
  --context context:primary-signature \
  --verbose=4 \
  build-macos-release-local/Story_Of_Alicia-<version>-macos.dmg
```

## 9. Build only an unsigned local `.app`

When you only want the application bundle for local development or testing,
use:

```bash
./packaging/macos/build-local.sh
```

This path deliberately does **not**:

- require a Developer ID identity,
- sign the application,
- submit anything to Apple,
- require notarization credentials,
- require the private update-signing key,
- create a DMG release, or
- generate signed release metadata.

It still builds the application, runs the tests, deploys Qt, and verifies the
important bundled architecture/runtime pieces.

The output is an unsigned `.app` in the selected local build directory.

## 10. What each macOS script is for

| Script | Purpose |
| --- | --- |
| `build-local.sh` | Build/test/deploy an unsigned local `.app` |
| `build-release-macos-local.sh` | Complete signed/notarized release build |
| `sign-app.sh` | Sign nested Mach-O code and the `.app` |
| `notarize.sh` | Submit an app archive or DMG to Apple and wait for the result |
| `verify-release.sh` | Audit the finished DMG and its SOA Seal metadata |
| `generate-macos-update-metadata.sh` | Generate and sign macOS update metadata |
| `packaging/publish-release.sh` | Publish both platform packages and metadata after both builds are ready |

For a normal macOS release build, run:

```bash
./packaging/macos/build-release-macos-local.sh
```

The release builder creates and verifies:

```text
build-macos-release-local/Story_Of_Alicia-<version>-macos.dmg
packaging/macos/manifest.json
packaging/macos/manifest.json.seal
packaging/macos/versions.json
packaging/macos/versions.json.seal
```

## 11. Publish the release locally

GitHub Actions is not used for releases. Create and push the release tag first,
and create the `launcher-updates` branch before the first publish. The branch
stores only update metadata under `linux/` and `macos/`.

Authenticate the GitHub CLI and Git push access:

```bash
gh auth login
gh auth setup-git
```

Set the R2 credentials in the current shell:

```bash
export SOA_R2_ACCOUNT_ID="..."
export SOA_R2_BUCKET="..."
export AWS_ACCESS_KEY_ID="..."
export AWS_SECRET_ACCESS_KEY="..."
```

After the Linux and macOS release builders have both completed, put the
AppImage and DMG on the same machine and run:

```bash
./packaging/publish-release.sh \
  /path/to/Story_Of_Alicia-1.0.1-x86_64.AppImage \
  /path/to/Story_Of_Alicia-1.0.1-macos.dmg
```

The publisher uploads only the AppImage and DMG as custom GitHub Release assets. GitHub still shows its automatic source-code archives separately. Update metadata is committed to the `launcher-updates` branch and uploaded to R2.

Before either platform generator creates `versions.json`, it fetches the
existing signed history from R2 and from the raw `launcher-updates` branch. If
both copies exist, they must match. The current release is merged into that
verified history and only the newest three releases are retained.

The launcher reads metadata from R2 first and falls back to the raw
`launcher-updates` branch. Package downloads use R2 first and the matching
GitHub Release asset as the mirror.

R2 stores packages directly in the platform directory with versioned filenames, such as `Story_Of_Alicia-1.0.1-x86_64.AppImage` and `Story_Of_Alicia-1.0.1-macos.dmg`, so retained history entries never overwrite one another.
