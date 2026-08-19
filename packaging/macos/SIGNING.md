# macOS signing and release guide

`.github/workflows/build-macos.yml` builds one universal application and DMG
containing both arm64 and x86_64 slices. Intel and Apple Silicon therefore use
the same download, manifest, and three-version update history.

Pull requests can build without credentials. A public release needs all Apple
signing and notarization secrets below. The workflow signs the nested Mach-O
files, app, and DMG; submits the app and DMG to Apple; staples the tickets; and
verifies the result before uploading the artifact.

Never commit a `.p12`, `.p8`, private update key, password, or base64-encoded
secret to the repository.

## 1. Create the Developer ID Application certificate

An active Apple Developer Program membership is required. Apple only permits
the Account Holder to create Developer ID certificates.

1. On a Mac, open Keychain Access and choose **Certificate Assistant → Request
   a Certificate From a Certificate Authority**. Save the CSR to disk. See
   [Create a certificate signing request](https://developer.apple.com/help/account/certificates/create-a-certificate-signing-request/).
2. In [Certificates, Identifiers & Profiles](https://developer.apple.com/account/resources/certificates/list),
   create a **Developer ID Application** certificate and upload the CSR. Do not
   choose Developer ID Installer; this project publishes a DMG, not a PKG. See
   [Create Developer ID certificates](https://developer.apple.com/help/account/certificates/create-developer-id-certificates/).
3. Download the `.cer` and open it so it is installed in Keychain Access.
4. Under **My Certificates**, expand the certificate and confirm its private key
   is nested below it. Export both together as a password-protected `.p12`.
   The export password must not be empty because the workflow treats an empty
   secret as missing.
5. Find the exact signing identity:

       security find-identity -v -p codesigning

   Copy the complete quoted value, for example:

       Developer ID Application: Example Studio (ABCDE12345)

6. Encode the `.p12` as one line and copy it to the clipboard:

       base64 -i DeveloperIDApplication.p12 | tr -d '\n' | pbcopy

## 2. Create the App Store Connect notarization key

1. Open **App Store Connect → Users and Access → Integrations → Team Keys** and
   generate an API key that your team can use for notarization. Record its Key
   ID and the Issuer ID. Apple only allows the `.p8` file to be downloaded once.
   See [App Store Connect API](https://developer.apple.com/help/app-store-connect/get-started/app-store-connect-api/).
2. Encode the downloaded key as one line:

       base64 -i AuthKey_KEYID.p8 | tr -d '\n' | pbcopy

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
| `APPLE_DEVELOPER_IDENTITY` | Full `Developer ID Application: … (TEAMID)` identity |
| `APPLE_NOTARY_KEY_P8_BASE64` | One-line base64 value of the `.p8` |
| `APPLE_NOTARY_KEY_ID` | App Store Connect API Key ID |
| `APPLE_NOTARY_ISSUER_ID` | App Store Connect Issuer ID |

These must be repository or organization Actions secrets available to this
repository. The workflow does not attach a GitHub Environment, so secrets stored
only inside an Environment will not be visible.

## 4. Keep the launcher update-signing key configured

Apple signing proves the DMG came from the Developer ID holder. Launcher
self-update metadata uses a separate Ed25519 key. Official Linux and macOS
artifacts both require the repository secret `SOA_UPDATE_SIGNING_KEY_B64` to
match `packaging/update-public-key.hex` (and, when set, the repository variable
`SOA_UPDATE_PUBLIC_KEY_HEX`). If that secret already exists and builds pass, do
not rotate it.

For a first release, generate the key once on a trusted Mac and keep a secure
offline backup:

    brew install openssl@3
    UPDATE_OPENSSL="$(brew --prefix openssl@3)/bin/openssl"
    umask 077
    "$UPDATE_OPENSSL" genpkey -algorithm Ed25519 -out soa-update-ed25519.pem
    "$UPDATE_OPENSSL" pkey -in soa-update-ed25519.pem -pubout -outform DER \
      | tail -c 32 | xxd -p -c 64
    base64 -i soa-update-ed25519.pem | tr -d '\n' | pbcopy

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

After downloading the DMG on a Mac, it can be checked again with:

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


