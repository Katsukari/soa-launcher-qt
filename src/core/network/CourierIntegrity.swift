import Foundation
import Soa_Courier
extension Courier
	{

	func startIntegrityCheck(installPath: String)
		{
		run { [self] in
			reportProgress(courier_phase_preparing, "Requesting game version...", 0, 0, 0, 0)
			let version = try await fetchRemoteVersion()
			reportProgress(courier_phase_preparing, "Requesting manifest...", 0, 0, 0, 0)
			let manifest = try await fetchManifest(version: version)
			guard !manifest.files.isEmpty else { throw Err("Manifest has no files") }

			var bad = 0
			for (i, entry) in manifest.files.enumerated() {
				try Task.checkCancellation()
				let local = localComparePath(installDir: installPath, entry: entry)
				let pct = Int(Double(i + 1) / Double(manifest.files.count) * 100)
				reportProgress(courier_phase_verifying, "Verifying (\(i + 1)/\(manifest.files.count))", pct, 0, 0, 0)
				let hash = md5OfFile(at: local)
				if hash == nil || hash != entry.hash { bad += 1 }
			}
			reportDone(true, "\(bad)")
		}
	}
}