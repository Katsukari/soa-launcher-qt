import Foundation
#if canImport(FoundationNetworking)
import FoundationNetworking
#endif

extension Courier
{

	func fetchRemoteVersion() async throws -> String
	{
		let url = URL(string: "\(cdnBaseURL)/version")!
		let (data, response) = try await URLSession.shared.data(from: url)

		guard let http = response as? HTTPURLResponse, http.statusCode == 200
		else
		{
			throw Err("Remote returned bad status retrieving version")
		}

		return String(decoding: data, as: UTF8.self)
		.trimmingCharacters(in: .whitespacesAndNewlines)
	}

	func fetchManifest(version: String) async throws -> Manifest
	{
		let url = URL(string: "\(cdnBaseURL)/\(version)/manifest.json")!
		let (data, response) = try await URLSession.shared.data(from: url)

		guard let http = response as? HTTPURLResponse, http.statusCode == 200
		else
		{
			throw Err("Remote returned bad status retrieving manifest")
		}

		do
		{
			return try JSONDecoder().decode(Manifest.self, from: data)
		}
		catch
		{
			throw Err("Invalid manifest JSON: \(error)")
		}
	}

	func md5OfFile(at path: String) -> String?
	{
		guard let handle = FileHandle(forReadingAtPath: path) else { return nil }
		defer { try? handle.close() }

		var hasher = MD5()

		while true
		{
			let chunk = handle.readData(ofLength: 1 << 16)
			if chunk.isEmpty { break }
			hasher.update(chunk)
		}
		return hasher.finalizeHex()
	}

	func localComparePath(installDir: String, entry: ManifestEntry) -> String
	{
		let direct = (installDir as NSString).appendingPathComponent(entry.path)

		if dxvkBackedUpDlls.contains(where: { entry.path.caseInsensitiveCompare($0) == .orderedSame })
		{
			let bak = direct + ".bak"
			if FileManager.default.fileExists(atPath: bak) { return bak }
		}
		return direct
	}

	func downloadFile(from url: URL,
	toTemp tempPath: String,
	onBytes: (Int) throws -> Void) async throws -> String
	{
		try Task.checkCancellation()

		let (data, response) = try await URLSession.shared.data(from: url)

		guard let http = response as? HTTPURLResponse, http.statusCode == 200
		else
		{
			throw Err("Remote returned bad status for \(url.lastPathComponent)")
		}

		try Task.checkCancellation()

		try data.write(to: URL(fileURLWithPath: tempPath))

		var hasher = MD5()
		hasher.update(data)

		try onBytes(data.count)

		return hasher.finalizeHex()
	}

	func tempPath(for dest: String) -> String
	{
		dest + ".download"
	}

	func writeVersionJson(installPath: String, version: String) throws
	{
		let path = (installPath as NSString).appendingPathComponent("version.json")
		let obj = ["version": version]
		let data = try JSONSerialization.data(withJSONObject: obj, options: [.prettyPrinted])
		try data.write(to: URL(fileURLWithPath: path))
	}

	func readLocalVersion(installPath: String) -> String?
	{
		let path = (installPath as NSString).appendingPathComponent("version.json")
		guard let data = FileManager.default.contents(atPath: path),
		let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
		else { return nil }
		return (obj["version"] as? String) ?? (obj["latest"] as? String)
	}
}