import Foundation
import Soa_Courier

func log(_ level: Int32, _ message: String)
{
    message.withCString { soa_log(level, $0) }
}

/// Prevent manifest-controlled names from inserting fake lines or terminal
/// control characters into launcher diagnostics.
func logSafe(_ value: String) -> String
{
    value.unicodeScalars.map { scalar in
        CharacterSet.controlCharacters.contains(scalar) ? "?" : String(scalar)
    }.joined()
}

struct ManifestEntry: Codable
{
    let path: String
    let hash: String
    let size: Int
}

struct Manifest: Codable
{
    let files: [ManifestEntry]
}

struct ValidatedManifestEntry
{
    let manifest: ManifestEntry
    let relativePath: String
    let collisionKey: String
}

struct ManagedManifest: Codable
{
    let schemaVersion: Int
    let releaseVersion: String
    let files: [String]
}

struct StagingManifestEntry: Codable, Equatable
{
    let path: String
    let hash: String
    let size: Int
}

struct StagingManifest: Codable, Equatable
{
    let schemaVersion: Int
    let releaseVersion: String
    let files: [StagingManifestEntry]
}

struct UpdateJournal: Codable
{
    let schemaVersion: Int
    let replacementPaths: [String]
    let replacementHadOriginal: [Bool]
    let replacementStagedPaths: [String]?
    let obsoletePaths: [String]
    let versionMetadataExisted: Bool
    let managedMetadataExisted: Bool
}

let dxvkBackedUpDlls: Set<String> = ["d3dx9_31.dll", "d3dx9_42.dll"]

struct Err: Error
{
    let message: String
    let retryable: Bool

    init(_ message: String, retryable: Bool = false)
    {
        self.message = message
        self.retryable = retryable
    }
}
