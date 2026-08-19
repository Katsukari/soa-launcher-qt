# macOS signing and release guide from Linux

`.github/workflows/build-macos.yml` builds one universal application and DMG
containing both arm64 and x86_64 slices. Intel and Apple Silicon therefore use
the same download, manifest, and three-version update history.

Pull requests can build without credentials. A public release needs all Apple
signing and notarization secrets below. The workflow signs the nested Mach-O
files, app, and DMG; submits the app and DMG to Apple; staples the tickets; and
verifies the result before uploading the artifact.

A Mac is not required to create the credentials or build the release. OpenSSL
on Linux creates the certificate request and `.p12`, while GitHub's macOS runner
provides `codesign`, `notarytool`, and `stapler`. A real Mac is still strongly
recommended for testing the finished DMG and launcher behavior.

Never commit a `.p12`, `.p8`, private update key, password, or base64-encoded
secret to the repository.

If the six Apple secrets are already configured, continue at
[Test the signed universal build](#5-test-the-signed-universal-build).

## 1. Create the Developer ID Application certificate on Linux

An active Apple Developer Program membership is required. Apple only permits
the Account Holder to create Developer ID certificates.

1. Install OpenSSL, generate a private RSA key, and create the certificate
   signing request:

       sudo apt install openssl
       umask 077
       openssl genrsa -out developer-id.key 2048
       openssl req -new \
         -key developer-id.key \
         -out developer-id.csr \
         -subj "/CN=Story of Alicia Release Key/emailAddress=YOUR_EMAIL"

   `CN` is only a descriptive name for the key, so it does not need to be a
   person's name. Use an email address you control. Country, city, state, and
   organization are unnecessary; omit them instead of entering invented data.
   To include a real two-letter country code, add `/C=XX` before `/CN`.

2. In [Certificates, Identifiers & Profiles](https://developer.apple.com/account/resources/certificates/list),
   create a **Developer ID Application** certificate and upload the CSR. Do not
   choose Developer ID Installer; this project publishes a DMG, not a PKG. See
   [Create Developer ID certificates](https://developer.apple.com/help/account/certificates/create-developer-id-certificates/).
3. Download the resulting `.cer`. The examples below assume it is named
   `developerID_application.cer`.
4. Convert Apple's DER certificate to PEM:

       openssl x509 \
         -inform DER \
         -in developerID_application.cer \
         -out developerID_application.pem

5. Combine the certificate and the original private key into a `.p12`:

       openssl pkcs12 -export \
         -inkey developer-id.key \
         -in developerID_application.pem \
         -out developer-id.p12 \
         -name "Story of Alicia Developer ID"

   Enter a strong, non-empty export password. This password becomes the
   `APPLE_DEVELOPER_ID_P12_PASSWORD` secret.

6. Confirm that the `.p12` can be opened before uploading it:

       openssl pkcs12 -in developer-id.p12 -info -noout

7. Generate the certificate's 40-character SHA-1 fingerprint:

       openssl x509 \
         -inform DER \
         -in developerID_application.cer \
         -noout \
         -fingerprint \
         -sha1 \
         | cut -d= -f2 \
         | tr -d ':\n'

   Use this fingerprint as `APPLE_DEVELOPER_IDENTITY`. Apple's `codesign`
   accepts the fingerprint as an unambiguous signing identity, so the full name
   does not need to be stored in that GitHub secret. The complete
   `Developer ID Application: … (TEAMID)` value also works; if it is already
   configured, changing it to the fingerprint is optional.

8. Encode the `.p12` as one line:

       base64 -w 0 developer-id.p12

   Copy the complete output into `APPLE_DEVELOPER_ID_P12_BASE64`.

### Developer ID privacy

The fingerprint keeps the full name out of the identity secret, but it does not
hide the identity in the signed application. An individual Apple Developer
membership produces a certificate containing the member's verified legal name
and Team ID. The supported alternative is an organization membership belonging
to a verified legal entity; aliases and trade names cannot replace the
certificate identity.

## 2. Create the App Store Connect notarization key

The `.p8` is a separate notarization credential. It is not created from the
Developer ID `.cer`, `.pem`, `.key`, or `.p12`.

1. Open [App Store Connect → Users and Access → Integrations](https://appstoreconnect.apple.com/access/integrations/api).
2. Select **Team Keys**. If **Request Access** appears, the Account Holder must
   request access and wait for Apple's approval.
3. Select **Generate API Key**, name it something like `Story of Alicia
   Notarization`, and choose the **Developer** role.
4. Record the Key ID and Issuer ID, then download the key. Apple allows each
   `.p8` private key to be downloaded only once. It will have a name similar to
   `AuthKey_AB12C3DEFG.p8`. See
   [App Store Connect API](https://developer.apple.com/help/app-store-connect/get-started/app-store-connect-api/).
5. Encode the downloaded key as one line on Linux:

       base64 -w 0 AuthKey_AB12C3DEFG.p8

   Copy the complete output into `APPLE_NOTARY_KEY_P8_BASE64`.

The workflow passes this key directly to `xcrun notarytool`; it does not need to
be imported into a keychain.

## 3. Add the GitHub Actions secrets

In the GitHub repository, open **Settings → Secrets and variables → Actions →
New repository secret**. GitHub documents the process in
[Using secrets in GitHub Actions](https://docs.github.com/actions/security-guides/using-secrets-in-github-actions).

Add these repository secrets exactly as named:

| Secret | Value |
| --- | --- |
| `APPLE_DEVELOPER_ID_P12_BASE64` | One-line base64 value of the `.p12` |
| `APPLE_DEVELOPER_ID_P12_PASSWORD` | Password used when exporting the `.p12` |
| `APPLE_DEVELOPER_IDENTITY` | 40-character SHA-1 fingerprint from the `.cer` |
| `APPLE_NOTARY_KEY_P8_BASE64` | One-line base64 value of the `.p8` |
| `APPLE_NOTARY_KEY_ID` | App Store Connect API Key ID |
| `APPLE_NOTARY_ISSUER_ID` | App Store Connect Issuer ID |

These must be repository or organization Actions secrets available to this
repository. The workflow does not attach a GitHub Environment, so secrets stored
only inside an Environment will not be visible.

There is no secret named exactly `APPLE_DEVELOPER_ID`. The workflow uses the
three `APPLE_DEVELOPER_ID_*` secrets shown above.

## 4. Keep the launcher update-signing key configured

Apple signing proves the DMG came from the Developer ID holder. Launcher
self-update metadata uses a separate Ed25519 key. Official Linux and macOS
artifacts both require the repository secret `SOA_UPDATE_SIGNING_KEY_B64` to
match `packaging/update-public-key.hex` (and, when set, the repository variable
`SOA_UPDATE_PUBLIC_KEY_HEX`). If that secret already exists and builds pass, do
not rotate it.

For a first release, generate the key once on a trusted Linux system and keep a
secure offline backup:

    sudo apt install openssl xxd
    umask 077
    openssl genpkey -algorithm Ed25519 -out soa-update-ed25519.pem
    openssl pkey -in soa-update-ed25519.pem -pubout -outform DER \
      | tail -c 32 | xxd -p -c 64
    base64 -w 0 soa-update-ed25519.pem

Put the printed 64-character public hex value in
`packaging/update-public-key.hex` and the `SOA_UPDATE_PUBLIC_KEY_HEX` repository
variable. Put the clipboard value in the `SOA_UPDATE_SIGNING_KEY_B64`
repository secret. The public key must be committed before building the
release. Losing the private key prevents publishing updates trusted by existing
installations; rotating the public key requires shipping a launcher that trusts
the new key first.

## 5. Test the signed universal build

Run **Build macOS (Universal)** from the Actions tab with `workflow_dispatch`,
or push the intended commit to `main`. Confirm that these steps run rather than
being skipped:

- Import Developer ID certificate
- Prepare notarization key
- Bundle and sign macOS application
- Sign and notarize macOS DMG

The official artifact name is `soa-launcher-macos-universal-dmg`. A name ending
in `-ephemeral-metadata` means `SOA_UPDATE_SIGNING_KEY_B64` was unavailable and
that build must not be published as a launcher update.

No Mac is required for this CI test. The `macos-15` runner imports the `.p12`,
selects it with `APPLE_DEVELOPER_IDENTITY`, signs the universal app and DMG,
submits both to Apple, staples the tickets, and runs Apple's verification tools.
If the certificate password, fingerprint, Key ID, Issuer ID, or encoded files
are wrong, the corresponding import, signing, or notarization step will fail.

For final user-facing validation, download the DMG on a real Mac, open it, run
the launcher, and check it again with:

    codesign --verify --strict --verbose=4 Story_Of_Alicia-macos.dmg
    xcrun stapler validate Story_Of_Alicia-macos.dmg
    spctl --assess --type open --context context:primary-signature \
      --verbose=4 Story_Of_Alicia-macos.dmg

## 6. Publish a release

`release.yml` does not rebuild packages. It downloads successful
`build-linux.yml` and `build-macos.yml` artifacts for the exact tagged commit.
Use this order:

1. Make sure the version in `CMakeLists.txt` is the version being released.
2. Push or merge the exact release commit to `main`.
3. Wait for both Linux and macOS build workflows to succeed for that commit.
4. Tag that same commit and push the tag:

       git tag -a v1.0.0 -m "Story of Alicia Launcher 1.0.0"
       git push origin v1.0.0

The tag must use the `.yml` workflow files already present in the repository;
GitHub Actions accepts both `.yml` and `.yaml`, but the release script looks up
the exact filenames `build-linux.yml` and `build-macos.yml`.
