import Foundation
#if canImport(FoundationNetworking)
import FoundationNetworking   // URLSession lives here on Linux (not in Foundation)
#endif
import SoaBridge

// Story Of Alicia download engine (Swift)

private struct MD5 {
	private var a0: UInt32 = 0x6745_2301
	private var b0: UInt32 = 0xefcd_ab89
	private var c0: UInt32 = 0x98ba_dcfe
	private var d0: UInt32 = 0x1032_5476

	private var buffer = [UInt8]()
	private var totalLength: UInt64 = 0

	private static let s: [UInt32] = [
		7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
		5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
		4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
		6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
	]

	private static let k: [UInt32] = [
		0xd76a_a478, 0xe8c7_b756, 0x2420_70db, 0xc1bd_ceee,
		0xf57c_0faf, 0x4787_c62a, 0xa830_4613, 0xfd46_9501,
		0x6980_98d8, 0x8b44_f7af, 0xffff_5bb1, 0x895c_d7be,
		0x6b90_1122, 0xfd98_7193, 0xa679_438e, 0x49b4_0821,
		0xf61e_2562, 0xc040_b340, 0x265e_5a51, 0xe9b6_c7aa,
		0xd62f_105d, 0x0244_1453, 0xd8a1_e681, 0xe7d3_fbc8,
		0x21e1_cde6, 0xc337_07d6, 0xf4d5_0d87, 0x455a_14ed,
		0xa9e3_e905, 0xfcef_a3f8, 0x676f_02d9, 0x8d2a_4c8a,
		0xfffa_3942, 0x8771_f681, 0x6d9d_6122, 0xfde5_380c,
		0xa4be_ea44, 0x4bde_cfa9, 0xf6bb_4b60, 0xbebf_bc70,
		0x289b_7ec6, 0xeaa1_27fa, 0xd4ef_3085, 0x0488_1d05,
		0xd9d4_d039, 0xe6db_99e5, 0x1fa2_7cf8, 0xc4ac_5665,
		0xf429_2244, 0x432a_ff97, 0xab94_23a7, 0xfc93_a039,
		0x655b_59c3, 0x8f0c_cc92, 0xffef_f47d, 0x8584_5dd1,
		0x6fa8_7e4f, 0xfe2c_e6e0, 0xa301_4314, 0x4e08_11a1,
		0xf753_7e82, 0xbd3a_f235, 0x2ad7_d2bb, 0xeb86_d391
	]

	@inline(__always)
	private static func rotl(_ x: UInt32, _ c: UInt32) -> UInt32 {
		(x << c) | (x >> (32 - c))
	}

	mutating func update(_ data: Data) {
		totalLength = totalLength &+ UInt64(data.count)
		buffer.append(contentsOf: data)

		// process complete 64-byte blocks
		var offset = 0
		while buffer.count - offset >= 64 {
			processBlock(Array(buffer[offset..<offset + 64]))
			offset += 64
		}
		if offset > 0 {
			buffer.removeFirst(offset)
		}
	}

	private mutating func processBlock(_ block: [UInt8]) {
		var m = [UInt32](repeating: 0, count: 16)
		for i in 0..<16 {
			let j = i * 4
			m[i] = UInt32(block[j])
			| (UInt32(block[j + 1]) << 8)
			| (UInt32(block[j + 2]) << 16)
			| (UInt32(block[j + 3]) << 24)
		}

		var a = a0, b = b0, c = c0, d = d0

		for i in 0..<64 {
			var f: UInt32
			var g: Int
			switch i {
			case 0..<16:
				f = (b & c) | (~b & d); g = i
			case 16..<32:
				f = (d & b) | (~d & c); g = (5 * i + 1) % 16
			case 32..<48:
				f = b ^ c ^ d;          g = (3 * i + 5) % 16
			default:
				f = c ^ (b | ~d);       g = (7 * i) % 16
			}
			f = f &+ a &+ MD5.k[i] &+ m[g]
			a = d
			d = c
			c = b
			b = b &+ MD5.rotl(f, MD5.s[i])
		}

		a0 = a0 &+ a
		b0 = b0 &+ b
		c0 = c0 &+ c
		d0 = d0 &+ d
	}

	mutating func finalizeHex() -> String {
		// padding: 0x80 then zeros to 56 mod 64, then 64-bit little-endian length
		let bitLength = totalLength &* 8
		buffer.append(0x80)
		while buffer.count % 64 != 56 {
			buffer.append(0)
		}
		for i in 0..<8 {
			buffer.append(UInt8((bitLength >> (8 * UInt64(i))) & 0xff))
		}
		var offset = 0
		while offset < buffer.count {
			processBlock(Array(buffer[offset..<offset + 64]))
			offset += 64
		}

		var digest = [UInt8]()
		for word in [a0, b0, c0, d0] {
			digest.append(UInt8(word & 0xff))
			digest.append(UInt8((word >> 8) & 0xff))
			digest.append(UInt8((word >> 16) & 0xff))
			digest.append(UInt8((word >> 24) & 0xff))
		}
		return digest.map { String(format: "%02x", $0) }.joined()
	}
}


private func log(_ level: Int32, _ message: String) {
	message.withCString { soa_log(level, $0) }
}


private struct ManifestEntry: Codable {
	let path: String
	let hash: String
	let size: Int
}

private struct Manifest: Codable {
	let files: [ManifestEntry]
}


// These DLLs may exist as "<name>.bak" when DXVK is installed; if so, the
// backup is the file to hash for the integrity comparison.
private let dxvkBackedUpDlls: Set<String> = ["d3dx9_31.dll", "d3dx9_42.dll"]


private final class Downloader {

	let cdnBaseURL: String
	let onProgress: soa_progress_cb
	let onDone: soa_done_cb
	let ctx: UnsafeMutableRawPointer?

	// The in-flight operation, so cancel() can tear it down.
	private var task: Task<Void, Never>?

	init(cdnBaseURL: String,
	onProgress: @escaping soa_progress_cb,
	onDone: @escaping soa_done_cb,
	ctx: UnsafeMutableRawPointer?)
		{
		self.cdnBaseURL = cdnBaseURL.hasSuffix("/")
		? String(cdnBaseURL.dropLast())
		: cdnBaseURL
		self.onProgress = onProgress
		self.onDone = onDone
		self.ctx = ctx
	}

	private func reportProgress(_ message: String,
	_ percent: Int,
	_ received: UInt64,
	_ total: UInt64,
	_ throughput: UInt64)
		{
		message.withCString { c in
			onProgress(c, Int32(percent), received, total, throughput, ctx)
		}
	}

	private func reportDone(_ ok: Bool, _ message: String) {
		message.withCString { c in onDone(ok, c, ctx) }
	}

	private func fetchRemoteVersion() async throws -> String {
		let url = URL(string: "\(cdnBaseURL)/version")!
		let (data, response) = try await URLSession.shared.data(from: url)
		guard let http = response as? HTTPURLResponse, http.statusCode == 200 else {
			throw Err("Remote returned bad status retrieving version")
		}
		return String(decoding: data, as: UTF8.self)
		.trimmingCharacters(in: .whitespacesAndNewlines)
	}

	private func fetchManifest(version: String) async throws -> Manifest {
		let url = URL(string: "\(cdnBaseURL)/\(version)/manifest.json")!
		let (data, response) = try await URLSession.shared.data(from: url)
		guard let http = response as? HTTPURLResponse, http.statusCode == 200 else {
			throw Err("Remote returned bad status retrieving manifest")
		}
		do {
			return try JSONDecoder().decode(Manifest.self, from: data)
		} catch {
			throw Err("Invalid manifest JSON: \(error)")
		}
	}

	private func md5OfFile(at path: String) -> String? {
		guard let handle = FileHandle(forReadingAtPath: path) else { return nil }
		defer { try? handle.close() }

		var hasher = MD5()
		while true {
			let chunk = handle.readData(ofLength: 1 << 16)   // 64 KiB
			if chunk.isEmpty { break }
			hasher.update(chunk)
		}
		return hasher.finalizeHex()
	}

	private func localComparePath(installDir: String, entry: ManifestEntry) -> String {
		let direct = (installDir as NSString).appendingPathComponent(entry.path)
		if dxvkBackedUpDlls.contains(where: { entry.path.caseInsensitiveCompare($0) == .orderedSame }) {
			let bak = direct + ".bak"
			if FileManager.default.fileExists(atPath: bak) { return bak }
		}
		return direct
	}

	private func downloadFile(from url: URL,
	toTemp tempPath: String,
	onBytes: (Int) throws -> Void) async throws -> String
		{
		try Task.checkCancellation()

		let (data, response) = try await URLSession.shared.data(from: url)
		guard let http = response as? HTTPURLResponse, http.statusCode == 200 else {
			throw Err("Remote returned bad status for \(url.lastPathComponent)")
		}

		try Task.checkCancellation()

		// write the file
		try data.write(to: URL(fileURLWithPath: tempPath))

		// hash it
		var hasher = MD5()
		hasher.update(data)

		// report the whole file's bytes for progress
		try onBytes(data.count)

		return hasher.finalizeHex()
	}

	private func tempPath(for dest: String) -> String { dest + ".download" }

	private func writeVersionJson(installPath: String, version: String) throws {
		let path = (installPath as NSString).appendingPathComponent("version.json")
		let obj = ["version": version]
		let data = try JSONSerialization.data(withJSONObject: obj, options: [.prettyPrinted])
		try data.write(to: URL(fileURLWithPath: path))
	}

	private func readLocalVersion(installPath: String) -> String? {
		let path = (installPath as NSString).appendingPathComponent("version.json")
		guard let data = FileManager.default.contents(atPath: path),
		let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any]
		else { return nil }
		return (obj["version"] as? String) ?? (obj["latest"] as? String)
	}

	func startIntegrityCheck(installPath: String) {
		run { [self] in
			reportProgress("Requesting game version...", 0, 0, 0, 0)
			let version = try await fetchRemoteVersion()
			reportProgress("Requesting manifest...", 0, 0, 0, 0)
			let manifest = try await fetchManifest(version: version)
			guard !manifest.files.isEmpty else { throw Err("Manifest has no files") }

			var bad = 0
			for (i, entry) in manifest.files.enumerated() {
				try Task.checkCancellation()
				let local = localComparePath(installDir: installPath, entry: entry)
				let pct = Int(Double(i + 1) / Double(manifest.files.count) * 100)
				reportProgress("Verifying (\(i + 1)/\(manifest.files.count))", pct, 0, 0, 0)
				let hash = md5OfFile(at: local)
				if hash == nil || hash != entry.hash { bad += 1 }
			}
			reportDone(true, "\(bad)")   // count comes back as the message
		}
	}

	func startUpdateCheck(installPath: String) {
		run { [self] in
			let remote = try await fetchRemoteVersion()
			let local = readLocalVersion(installPath: installPath)
			if local == remote {
				reportDone(true, "up-to-date")
			} else {
				reportDone(true, "update-available")
			}
		}
	}

	func startUpdate(installPath: String) {
		run { [self] in
			reportProgress("Requesting game version...", 0, 0, 0, 0)
			let version = try await fetchRemoteVersion()
			reportProgress("Requesting manifest...", 0, 0, 0, 0)
			let manifest = try await fetchManifest(version: version)
			guard !manifest.files.isEmpty else { throw Err("Manifest has no files") }

			// Ensure the install dir exists.
			try FileManager.default.createDirectory(
				atPath: installPath, withIntermediateDirectories: true)

			// Hash-the-diff: figure out which files actually need downloading.
			var needed: [ManifestEntry] = []
			for (i, entry) in manifest.files.enumerated() {
				try Task.checkCancellation()
				let local = localComparePath(installDir: installPath, entry: entry)
				let pct = Int(Double(i + 1) / Double(manifest.files.count) * 100)
				reportProgress("Checking (\(i + 1)/\(manifest.files.count))", pct, 0, 0, 0)
				let hash = md5OfFile(at: local)
				if hash == nil || hash != entry.hash { needed.append(entry) }
			}

			if needed.isEmpty {
				try writeVersionJson(installPath: installPath, version: version)
				reportProgress("Already up to date.", 100, 1, 1, 0)
				reportDone(true, "Already up to date.")
				return
			}

			let totalBytes = UInt64(needed.reduce(0) { $0 + $1.size })
			var receivedBytes: UInt64 = 0

			// throughput measurement (>1s windows, like the Rust)
			var windowStart = Date()
			var windowBytes: UInt64 = 0
			var throughput: UInt64 = 0

			let contentsURL = "\(cdnBaseURL)/\(version)"

			for (i, entry) in needed.enumerated() {
				try Task.checkCancellation()

				let dest = (installPath as NSString).appendingPathComponent(entry.path)
				let temp = tempPath(for: dest)

				// make parent dirs
				let parent = (temp as NSString).deletingLastPathComponent
				try FileManager.default.createDirectory(
					atPath: parent, withIntermediateDirectories: true)

				guard let url = URL(string: "\(contentsURL)/\(entry.path)") else {
					throw Err("Bad URL for \(entry.path)")
				}

				let actualHash = try await downloadFile(from: url, toTemp: temp) { n in
					receivedBytes += UInt64(n)
					windowBytes += UInt64(n)

					let now = Date()
					let elapsed = now.timeIntervalSince(windowStart)
					if elapsed > 1.0 && windowBytes > 0 {
						throughput = UInt64(Double(windowBytes) / elapsed)
						windowBytes = 0
						windowStart = now
					}

					let pct = totalBytes > 0
					? Int(Double(receivedBytes) / Double(totalBytes) * 100)
					: 0
					reportProgress("Downloading (\(i + 1)/\(needed.count))",
						pct, receivedBytes, totalBytes, throughput)
				}

				// verify before committing
				if actualHash != entry.hash {
					try? FileManager.default.removeItem(atPath: temp)
					throw Err("Hash mismatch for \(entry.path)")
				}

				// atomic replace: remove existing, move temp into place
				if FileManager.default.fileExists(atPath: dest) {
					try FileManager.default.removeItem(atPath: dest)
				}
				try FileManager.default.moveItem(atPath: temp, toPath: dest)
			}

			try writeVersionJson(installPath: installPath, version: version)
			reportProgress("Update complete.", 100, totalBytes, totalBytes, 0)
			reportDone(true, "Updated \(needed.count) file(s).")
		}
	}

	func cancel() {
		task?.cancel()
	}

	// Wraps an async operation
	private func run(_ body: @escaping () async throws -> Void) {
		task?.cancel()
		task = Task {
			do {
				try await body()
			} catch is CancellationError {
				reportDone(false, "Cancelled.")
			} catch let e as Err {
				log(4, e.message)
				reportDone(false, e.message)
			} catch {
				log(4, "\(error)")
				reportDone(false, "\(error)")
			}
		}
	}
}

// Small typed error so messages are clean across the bridge.
private struct Err: Error { let message: String; init(_ m: String) { message = m } }

@_cdecl("soa_downloader_create")
public func soa_downloader_create(_ cdn: UnsafePointer<CChar>?,
_ onProgress: soa_progress_cb?,
_ onDone: soa_done_cb?,
_ ctx: UnsafeMutableRawPointer?) -> UnsafeMutableRawPointer?
	{
	guard let cdn, let onProgress, let onDone else { return nil }
	let d = Downloader(cdnBaseURL: String(cString: cdn),
		onProgress: onProgress,
		onDone: onDone,
		ctx: ctx)
	return Unmanaged.passRetained(d).toOpaque()
}

@_cdecl("soa_downloader_destroy")
public func soa_downloader_destroy(_ ptr: UnsafeMutableRawPointer?) {
	guard let ptr else { return }
	Unmanaged<Downloader>.fromOpaque(ptr).release()
}

@_cdecl("soa_integrity_check")
public func soa_integrity_check(_ ptr: UnsafeMutableRawPointer?,
_ installPath: UnsafePointer<CChar>?)
	{
	guard let ptr, let installPath else { return }
	let d = Unmanaged<Downloader>.fromOpaque(ptr).takeUnretainedValue()
	d.startIntegrityCheck(installPath: String(cString: installPath))
}

@_cdecl("soa_update_check")
public func soa_update_check(_ ptr: UnsafeMutableRawPointer?,
_ installPath: UnsafePointer<CChar>?)
	{
	guard let ptr, let installPath else { return }
	let d = Unmanaged<Downloader>.fromOpaque(ptr).takeUnretainedValue()
	d.startUpdateCheck(installPath: String(cString: installPath))
}

@_cdecl("soa_update")
public func soa_update(_ ptr: UnsafeMutableRawPointer?,
_ installPath: UnsafePointer<CChar>?)
	{
	guard let ptr, let installPath else { return }
	let d = Unmanaged<Downloader>.fromOpaque(ptr).takeUnretainedValue()
	d.startUpdate(installPath: String(cString: installPath))
}

@_cdecl("soa_cancel")
public func soa_cancel(_ ptr: UnsafeMutableRawPointer?) {
	guard let ptr else { return }
	let d = Unmanaged<Downloader>.fromOpaque(ptr).takeUnretainedValue()
	d.cancel()
}

// Keep the ping
@_cdecl("soa_ping")
public func soa_ping() {
	log(2, "ping from swift!")
}