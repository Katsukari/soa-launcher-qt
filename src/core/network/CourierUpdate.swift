import Foundation
import Soa_Courier
extension Courier
	{

	func startUpdateCheck(installPath: String)
		{
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

	func startUpdate(installPath: String)
		{
		run { [self] in
			reportProgress(courier_phase_preparing, "Requesting game version...", 0, 0, 0, 0)
			let version = try await fetchRemoteVersion()
			reportProgress(courier_phase_preparing, "Requesting manifest...", 0, 0, 0, 0)
			let manifest = try await fetchManifest(version: version)
			guard !manifest.files.isEmpty else { throw Err("Manifest has no files") }

			try FileManager.default.createDirectory(
				atPath: installPath, withIntermediateDirectories: true)

			var needed: [ManifestEntry] = []
			for (i, entry) in manifest.files.enumerated() {
				try Task.checkCancellation()
				let local = localComparePath(installDir: installPath, entry: entry)
				let pct = Int(Double(i + 1) / Double(manifest.files.count) * 100)
				reportProgress(courier_phase_checking, "Checking (\(i + 1)/\(manifest.files.count))", pct, 0, 0, 0)
				let hash = md5OfFile(at: local)
				if hash == nil || hash != entry.hash { needed.append(entry) }
			}

			if needed.isEmpty {
				try writeVersionJson(installPath: installPath, version: version)
				reportProgress(courier_phase_checking, "Already up to date.", 100, 1, 1, 0)
				reportDone(true, "Already up to date.")
				return
			}

			let totalBytes = UInt64(needed.reduce(0) { $0 + $1.size })
			var receivedBytes: UInt64 = 0

			var windowStart = Date()
			var windowBytes: UInt64 = 0
			var throughput: UInt64 = 0

			let contentsURL = "\(cdnBaseURL)/\(version)"

			for (i, entry) in needed.enumerated() {
				try Task.checkCancellation()

				let dest = (installPath as NSString).appendingPathComponent(entry.path)
				let temp = tempPath(for: dest)

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
					reportProgress(courier_phase_downloading, "Downloading (\(i + 1)/\(needed.count))",
						pct, receivedBytes, totalBytes, throughput)
				}

				if actualHash != entry.hash {
					try? FileManager.default.removeItem(atPath: temp)
					throw Err("Hash mismatch for \(entry.path)")
				}

				if FileManager.default.fileExists(atPath: dest) {
					try FileManager.default.removeItem(atPath: dest)
				}
				try FileManager.default.moveItem(atPath: temp, toPath: dest)
			}

			try writeVersionJson(installPath: installPath, version: version)
			reportProgress(courier_phase_downloading, "Update complete.", 100, totalBytes, totalBytes, 0)
			reportDone(true, "Updated \(needed.count) file(s).")
		}
	}
}