# macOS signing and notarization

The GitHub Actions workflow builds separate arm64 and x86_64 applications.
Pull requests can build without signing credentials. When signing secrets are
available, the workflow signs the app's Mach-O files and nested bundles, signs
the outer application with the hardened runtime, creates and signs the DMG,
submits it to Apple notarization, staples the ticket, and verifies the result.

Configure these GitHub Actions secrets:

- `APPLE_DEVELOPER_ID_P12_BASE64`
- `APPLE_DEVELOPER_ID_P12_PASSWORD`
- `APPLE_DEVELOPER_IDENTITY`
- `APPLE_NOTARY_KEY_P8_BASE64`
- `APPLE_NOTARY_KEY_ID`
- `APPLE_NOTARY_ISSUER_ID`

`APPLE_DEVELOPER_IDENTITY` must be the complete Developer ID Application identity, including the Team ID.

Export the Developer ID Application certificate and private key as a password-protected `.p12`, then base64-encode it as a single line. Create an App Store Connect API key with access to notarization and base64-encode the `.p8` key as a single line.

The workflow intentionally does not modify the application after signing. A
signed but non-notarized DMG is still produced when the certificate is
configured without notarization credentials, but it is not treated as a
completed public macOS release.
