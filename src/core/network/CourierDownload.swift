import Foundation
#if canImport(FoundationNetworking)
import FoundationNetworking
#endif

extension Courier
{
    private func fetchData(url: URL, maximumBytes: Int, description: String) async throws -> Data
    {
        var attempt = 0
        while true {
            try Task.checkCancellation()
            attempt += 1
            do {
                let session = URLSession(configuration: sessionConfiguration)
                defer { session.invalidateAndCancel() }
                let (data, response) = try await session.data(from: url)
                guard let http = response as? HTTPURLResponse else {
                    throw Err("Remote returned a non-HTTP response for \(description)")
                }
                guard http.statusCode == 200 else {
                    throw Err(
                        "Remote returned HTTP \(http.statusCode) while retrieving \(description)",
                        retryable: http.statusCode == 408 || http.statusCode == 429 || http.statusCode >= 500)
                }
                guard data.count <= maximumBytes else {
                    throw Err("Remote \(description) exceeds the allowed size")
                }
                return data
            } catch is CancellationError {
                throw CancellationError()
            } catch let error as Err {
                if !error.retryable || attempt >= 3 { throw error }
                log(3, "Retrying \(description) request after transient failure: \(error.message)")
            } catch {
                if attempt >= 3 {
                    throw Err("Failed to retrieve \(description): \(error)")
                }
                log(3, "Retrying \(description) request after transient failure: \(error)")
            }
            try await Task.sleep(for: .seconds(pow(2.0, Double(attempt - 1))))
        }
    }

    func fetchRemoteVersion() async throws -> String
    {
        guard let url = URL(string: "\(cdnBaseURL)/version") else {
            throw Err("Invalid remote version URL")
        }
        let data = try await fetchData(url: url, maximumBytes: 4096, description: "game version")
        let version = String(decoding: data, as: UTF8.self)
            .trimmingCharacters(in: .whitespacesAndNewlines)

        guard !version.isEmpty,
              version.count <= 128,
              version.range(of: #"^[A-Za-z0-9._-]+$"#, options: .regularExpression) != nil else {
            throw Err("Remote returned an invalid game version")
        }
        return version
    }

    func fetchManifest(version: String) async throws -> [ValidatedManifestEntry]
    {
        guard let url = URL(string: "\(cdnBaseURL)/\(version)/manifest.json") else {
            throw Err("Invalid remote manifest URL")
        }
        let data = try await fetchData(url: url, maximumBytes: 32 * 1024 * 1024, description: "game manifest")
        do {
            return try validateManifest(JSONDecoder().decode(Manifest.self, from: data))
        } catch let error as Err {
            throw error
        } catch {
            throw Err("Invalid manifest JSON: \(error)")
        }
    }

    func md5OfFile(at path: String, progress: ((UInt64) -> Void)? = nil) throws -> String?
    {
        guard let handle = FileHandle(forReadingAtPath: path) else { return nil }
        defer { try? handle.close() }

        var hasher = MD5()
        var processed: UInt64 = 0
        var nextReport: UInt64 = 8 * 1024 * 1024
        while true {
            try Task.checkCancellation()
            let chunk = handle.readData(ofLength: 1 << 16)
            if chunk.isEmpty { break }
            hasher.update(chunk)
            processed += UInt64(chunk.count)
            if processed >= nextReport {
                progress?(processed)
                nextReport = processed + 8 * 1024 * 1024
            }
        }
        progress?(processed)
        return hasher.finalizeHex()
    }

    func tempPath(for destination: URL) -> URL
    {
        destination.appendingPathExtension("download")
    }

    func atomicWriteJSON<T: Encodable>(_ value: T, to url: URL) throws
    {
        let data = try JSONEncoder.pretty.encode(value)
        try data.write(to: url, options: .atomic)
    }

    func writeVersionJSON(installRoot: URL, version: String) throws
    {
        struct VersionFile: Codable { let version: String }
        try atomicWriteJSON(VersionFile(version: version), to: installRoot.appendingPathComponent("version.json"))
    }

    func readLocalVersion(installPath: String) -> String?
    {
        let path = (installPath as NSString).appendingPathComponent("version.json")
        guard let data = FileManager.default.contents(atPath: path),
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            return nil
        }
        return (object["version"] as? String) ?? (object["latest"] as? String)
    }

    func readManagedManifest(installRoot: URL) -> ManagedManifest?
    {
        let url = installRoot.appendingPathComponent(".soa-managed-manifest.json")
        guard let data = try? Data(contentsOf: url), data.count <= 16 * 1024 * 1024 else { return nil }
        return try? JSONDecoder().decode(ManagedManifest.self, from: data)
    }
}

private extension JSONEncoder
{
    static var pretty: JSONEncoder
    {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        return encoder
    }
}
