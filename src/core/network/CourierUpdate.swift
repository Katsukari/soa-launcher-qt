import Foundation
import Soa_Courier

private struct PlannedReplacement
{
    let entry: ValidatedManifestEntry
    let stagedRelativePath: String
    let targetRelativePath: String
}

extension Courier
{
    private var stagingDirectoryName: String { ".soa-update-staging" }
    private var backupDirectoryName: String { ".soa-update-backup" }
    private var journalFileName: String { ".soa-update-journal.json" }
    private var stagingManifestFileName: String { ".soa-update-staging.json" }
    private var managedManifestFileName: String { ".soa-managed-manifest.json" }

    private func removeIfPresent(_ url: URL) throws
    {
        if FileManager.default.fileExists(atPath: url.path) {
            try FileManager.default.removeItem(at: url)
        }
    }

    private func createParent(of url: URL) throws
    {
        try FileManager.default.createDirectory(
            at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
    }

    private func metadataBackupURL(backupRoot: URL, name: String) -> URL
    {
        backupRoot.appendingPathComponent("__metadata", isDirectory: true)
            .appendingPathComponent(name)
    }

    private func restoreMetadata(installRoot: URL, backupRoot: URL,
                                 name: String, existedBefore: Bool) throws
    {
        let destination = installRoot.appendingPathComponent(name)
        let backup = metadataBackupURL(backupRoot: backupRoot, name: name)
        if FileManager.default.fileExists(atPath: backup.path) {
            try removeIfPresent(destination)
            try createParent(of: destination)
            try FileManager.default.moveItem(at: backup, to: destination)
        } else if !existedBefore {
            try removeIfPresent(destination)
        }
    }

    private func prepareInternalDirectory(installRoot: URL, name: String, preserve: Bool) throws -> URL
    {
        let raw = installRoot.appendingPathComponent(name, isDirectory: true)
        let manager = FileManager.default
        if manager.fileExists(atPath: raw.path) {
            let attributes = try manager.attributesOfItem(atPath: raw.path)
            guard attributes[.type] as? FileAttributeType == .typeDirectory else {
                throw Err("Launcher update workspace is not a directory: \(name)")
            }
            if !preserve {
                try manager.removeItem(at: raw)
                try manager.createDirectory(at: raw, withIntermediateDirectories: true)
            }
        } else {
            try manager.createDirectory(at: raw, withIntermediateDirectories: true)
        }

        let resolved = raw.resolvingSymlinksInPath().standardizedFileURL
        let rootPrefix = installRoot.path.hasSuffix("/") ? installRoot.path : installRoot.path + "/"
        guard resolved.path.hasPrefix(rootPrefix) else {
            throw Err("Launcher update workspace escapes the game installation directory")
        }
        return resolved
    }

    private func readStagingManifest(installRoot: URL) -> StagingManifest?
    {
        let url = installRoot.appendingPathComponent(stagingManifestFileName)
        guard let data = try? Data(contentsOf: url), data.count <= 64 * 1024 * 1024 else {
            return nil
        }
        return try? JSONDecoder().decode(StagingManifest.self, from: data)
    }

    func recoverInterruptedUpdate(installRoot: URL) throws
    {
        let manager = FileManager.default
        let journalURL = installRoot.appendingPathComponent(journalFileName)
        let backupRootRaw = installRoot.appendingPathComponent(backupDirectoryName, isDirectory: true)
        guard manager.fileExists(atPath: journalURL.path) else {
            // A staging directory without a commit journal contains resumable downloads.
            // Only a backup directory is stale when there is no active journal.
            try? removeIfPresent(backupRootRaw)
            return
        }

        let data = try Data(contentsOf: journalURL)
        guard data.count <= 4 * 1024 * 1024 else {
            throw Err("Update recovery journal is unexpectedly large")
        }
        let journal = try JSONDecoder().decode(UpdateJournal.self, from: data)
        guard (journal.schemaVersion == 1 || journal.schemaVersion == 2),
              journal.replacementPaths.count == journal.replacementHadOriginal.count else {
            throw Err("Update recovery journal is invalid")
        }
        if let staged = journal.replacementStagedPaths,
           staged.count != journal.replacementPaths.count {
            throw Err("Update recovery journal has invalid staging metadata")
        }

        let backupRoot = try prepareInternalDirectory(
            installRoot: installRoot, name: backupDirectoryName, preserve: true)
        var stagingRoot: URL?
        if journal.replacementStagedPaths != nil {
            stagingRoot = try prepareInternalDirectory(
                installRoot: installRoot, name: stagingDirectoryName, preserve: true)
        }
        log(3, "Recovering an interrupted game update")

        for index in journal.replacementPaths.indices.reversed() {
            let relative = try normalizedManifestPath(journal.replacementPaths[index])
            let destination = try safeDestination(root: installRoot, relativePath: relative)
            let backup = try safeDestination(root: backupRoot, relativePath: relative)
            let backupExists = manager.fileExists(atPath: backup.path)
            let replacementReachedDestination = backupExists || !journal.replacementHadOriginal[index]

            if replacementReachedDestination, manager.fileExists(atPath: destination.path),
               let stagedPaths = journal.replacementStagedPaths, let stagingRoot {
                let stagedRelative = try normalizedManifestPath(stagedPaths[index])
                let staged = try safeDestination(root: stagingRoot, relativePath: stagedRelative)
                try createParent(of: staged)
                if manager.fileExists(atPath: staged.path) {
                    try removeIfPresent(destination)
                } else {
                    try manager.moveItem(at: destination, to: staged)
                }
            }

            if backupExists {
                try removeIfPresent(destination)
                try createParent(of: destination)
                try manager.moveItem(at: backup, to: destination)
            } else if !journal.replacementHadOriginal[index] {
                try removeIfPresent(destination)
            }
        }

        for raw in journal.obsoletePaths.reversed() {
            let relative = try normalizedManifestPath(raw)
            let destination = try safeDestination(root: installRoot, relativePath: relative)
            let backup = try safeDestination(root: backupRoot, relativePath: relative)
            if manager.fileExists(atPath: backup.path) {
                try removeIfPresent(destination)
                try createParent(of: destination)
                try manager.moveItem(at: backup, to: destination)
            }
        }

        try restoreMetadata(
            installRoot: installRoot, backupRoot: backupRoot,
            name: "version.json", existedBefore: journal.versionMetadataExisted)
        try restoreMetadata(
            installRoot: installRoot, backupRoot: backupRoot,
            name: managedManifestFileName, existedBefore: journal.managedMetadataExisted)

        try? removeIfPresent(backupRoot)
        try removeIfPresent(journalURL)
    }

    private func backupMetadataIfPresent(installRoot: URL, backupRoot: URL, name: String) throws
    {
        let source = installRoot.appendingPathComponent(name)
        guard FileManager.default.fileExists(atPath: source.path) else { return }
        let backup = metadataBackupURL(backupRoot: backupRoot, name: name)
        try createParent(of: backup)
        try FileManager.default.moveItem(at: source, to: backup)
    }

    private func availableDiskSpace(at root: URL) -> UInt64?
    {
        guard let attributes = try? FileManager.default.attributesOfFileSystem(forPath: root.path),
              let value = attributes[.systemFreeSize] as? NSNumber else { return nil }
        return value.uint64Value
    }

    func startUpdateCheck(installPath: String) -> UInt64
    {
        run { [self] operationID in
            let installRoot = try canonicalInstallRoot(installPath)
            try recoverInterruptedUpdate(installRoot: installRoot)
            let remote = try await fetchRemoteVersion()
            let local = readLocalVersion(installPath: installPath)
            reportDone(operationID, true, local == remote ? "up-to-date" : "update-available")
        }
    }

    func startUpdate(installPath: String) -> UInt64
    {
        run { [self] operationID in
            let fileManager = FileManager.default
            let installRoot = try canonicalInstallRoot(installPath, create: true)
            try recoverInterruptedUpdate(installRoot: installRoot)

            reportProgress(operationID, courier_phase_preparing,
                           "Requesting game version...", 0, 0, 0, 0, 0, 0)
            let version = try await fetchRemoteVersion()
            reportProgress(operationID, courier_phase_preparing,
                           "Requesting manifest...", 0, 0, 0, 0, 0, 0)
            let manifest = try await fetchManifest(version: version)

            var needed: [PlannedReplacement] = []
            var targetPaths: [String] = []
            var targetPathKeys: Set<String> = []
            targetPaths.reserveCapacity(manifest.count)

            for (index, entry) in manifest.enumerated() {
                try Task.checkCancellation()
                let targetRelative = try actualManagedRelativePath(installRoot: installRoot, entry: entry)
                let destination = try safeDestination(root: installRoot, relativePath: targetRelative)
                let targetKey = targetRelative.folding(
                    options: [.caseInsensitive, .diacriticInsensitive, .widthInsensitive],
                    locale: Locale(identifier: "en_US_POSIX"))
                guard targetPathKeys.insert(targetKey).inserted else {
                    throw Err("Manifest entries resolve to the same destination: \(targetRelative)")
                }
                targetPaths.append(targetRelative)

                let percent = Int(Double(index + 1) / Double(manifest.count) * 100)
                reportProgress(operationID, courier_phase_checking,
                               "Checking (\(index + 1)/\(manifest.count))", percent,
                               0, 0, 0, index + 1, manifest.count)
                let hash = try md5OfFile(at: destination.path) { _ in
                    self.reportProgress(operationID, courier_phase_checking,
                                        "Checking (\(index + 1)/\(manifest.count))", percent,
                                        0, 0, 0, index + 1, manifest.count)
                }
                if hash == nil || hash?.caseInsensitiveCompare(entry.manifest.hash) != .orderedSame {
                    needed.append(PlannedReplacement(
                        entry: entry,
                        stagedRelativePath: entry.relativePath,
                        targetRelativePath: targetRelative))
                }
            }

            let priorManaged = readManagedManifest(installRoot: installRoot)
            let targetKeys = Set(targetPaths.map {
                $0.folding(options: [.caseInsensitive, .diacriticInsensitive, .widthInsensitive],
                           locale: Locale(identifier: "en_US_POSIX"))
            })
            var obsolete: [String] = []
            for raw in priorManaged?.files ?? [] {
                do {
                    let normalized = try normalizedManifestPath(raw)
                    let key = normalized.folding(
                        options: [.caseInsensitive, .diacriticInsensitive, .widthInsensitive],
                        locale: Locale(identifier: "en_US_POSIX"))
                    if !targetKeys.contains(key) {
                        obsolete.append(normalized)
                    }
                } catch {
                    log(3, "Ignoring unsafe path in local managed manifest: \(raw)")
                }
            }

            let totalBytes = try needed.reduce(UInt64(0)) { partial, planned in
                let size = UInt64(planned.entry.manifest.size)
                let (sum, overflow) = partial.addingReportingOverflow(size)
                if overflow { throw Err("Manifest total download size is too large") }
                return sum
            }

            let stagingRootRaw = installRoot.appendingPathComponent(stagingDirectoryName, isDirectory: true)
            let stagingManifestURL = installRoot.appendingPathComponent(stagingManifestFileName)
            let expectedStagingManifest = StagingManifest(
                schemaVersion: 1,
                releaseVersion: version,
                files: manifest.map { entry in
                    StagingManifestEntry(
                        path: entry.relativePath,
                        hash: entry.manifest.hash.lowercased(),
                        size: entry.manifest.size)
                })

            let canResume = readStagingManifest(installRoot: installRoot) == expectedStagingManifest
            if !canResume {
                try removeIfPresent(stagingRootRaw)
                try removeIfPresent(stagingManifestURL)
            } else if !needed.isEmpty {
                log(2, "Resuming verified or partial files from the previous download attempt")
            }

            let stagingRoot = try prepareInternalDirectory(
                installRoot: installRoot, name: stagingDirectoryName, preserve: true)
            let backupRoot = try prepareInternalDirectory(
                installRoot: installRoot, name: backupDirectoryName, preserve: false)
            try atomicWriteJSON(expectedStagingManifest, to: stagingManifestURL)

            var resumableBytes: UInt64 = 0
            for planned in needed {
                let staged = try safeDestination(root: stagingRoot, relativePath: planned.stagedRelativePath)
                let storedSize = (try? fileManager.attributesOfItem(atPath: staged.path)[.size] as? NSNumber)?.uint64Value ?? 0
                resumableBytes += min(storedSize, UInt64(planned.entry.manifest.size))
            }
            let remainingBytes = totalBytes > resumableBytes ? totalBytes - resumableBytes : 0
            if remainingBytes > 0, let free = availableDiskSpace(at: installRoot),
               free < remainingBytes + 64 * 1024 * 1024 {
                throw Err("Not enough free disk space to finish this update")
            }

            var completedBytes: UInt64 = 0
            var windowStart = Date()
            var windowBytes: UInt64 = 0
            var throughput: UInt64 = 0
            var lastProgressReport = Date.distantPast

            for (index, planned) in needed.enumerated() {
                try Task.checkCancellation()
                let staged = try safeDestination(root: stagingRoot, relativePath: planned.stagedRelativePath)
                try createParent(of: staged)
                let url = try urlForContent(
                    base: cdnBaseURL, version: version, relativePath: planned.entry.relativePath)
                let expectedSize = UInt64(planned.entry.manifest.size)
                var performedCleanRedownload = false
                var transferredBytesThisRun: UInt64 = 0

                while true {
                    try Task.checkCancellation()
                    var storedSize = (try? fileManager.attributesOfItem(atPath: staged.path)[.size] as? NSNumber)?.uint64Value ?? 0
                    if storedSize > expectedSize {
                        try removeIfPresent(staged)
                        storedSize = 0
                    }

                    if storedSize < expectedSize {
                        let receivedBeforeRequest = completedBytes + storedSize
                        reportProgress(
                            operationID, courier_phase_downloading,
                            storedSize > 0
                                ? "Resuming (\(index + 1)/\(needed.count))"
                                : "Downloading (\(index + 1)/\(needed.count))",
                            totalBytes > 0
                                ? Int(Double(receivedBeforeRequest) / Double(totalBytes) * 100)
                                : 0,
                            receivedBeforeRequest, totalBytes, throughput, index + 1, needed.count)

                        let resumedThisRequest = storedSize > 0
                        try await streamingDownload(
                            from: url,
                            to: staged,
                            expectedSize: planned.entry.manifest.size)
                        { byteCount, fileReceived in
                            let transferred = UInt64(byteCount)
                            transferredBytesThisRun += transferred
                            windowBytes += transferred
                            let now = Date()
                            let elapsed = now.timeIntervalSince(windowStart)
                            if elapsed >= 1.0 && windowBytes > 0 {
                                throughput = UInt64(Double(windowBytes) / elapsed)
                                windowBytes = 0
                                windowStart = now
                            }
                            let received = completedBytes + fileReceived
                            let percent = totalBytes > 0
                                ? Int(Double(received) / Double(totalBytes) * 100)
                                : 0
                            if now.timeIntervalSince(lastProgressReport) >= 0.1 || received >= totalBytes {
                                lastProgressReport = now
                                self.reportProgress(
                                    operationID, courier_phase_downloading,
                                    resumedThisRequest
                                        ? "Resuming (\(index + 1)/\(needed.count))"
                                        : "Downloading (\(index + 1)/\(needed.count))",
                                    percent, received, totalBytes, throughput,
                                    index + 1, needed.count)
                            }
                        }
                    }

                    reportProgress(
                        operationID, courier_phase_verifying,
                        "Verifying (\(index + 1)/\(needed.count))",
                        totalBytes > 0
                            ? Int(Double(completedBytes) / Double(totalBytes) * 100)
                            : 0,
                        completedBytes, totalBytes, throughput, index + 1, needed.count)

                    let actualHash = try md5OfFile(at: staged.path, progress: { _ in
                        self.reportProgress(
                            operationID, courier_phase_verifying,
                            "Verifying (\(index + 1)/\(needed.count))",
                            totalBytes > 0
                                ? Int(Double(completedBytes) / Double(totalBytes) * 100)
                                : 0,
                            completedBytes, totalBytes, throughput, index + 1, needed.count)
                    })
                    if actualHash?.caseInsensitiveCompare(planned.entry.manifest.hash) == .orderedSame {
                        break
                    }

                    try removeIfPresent(staged)
                    if performedCleanRedownload {
                        throw Err("Hash mismatch for \(planned.entry.relativePath)")
                    }
                    performedCleanRedownload = true
                    log(3, "Saved partial data for \(planned.entry.relativePath) was invalid; retrying that file from the beginning")
                }

                let safePath = logSafe(planned.entry.relativePath)
                if transferredBytesThisRun > 0 {
                    log(1, "Downloaded \(expectedSize) bytes of \(safePath) (\(index + 1)/\(needed.count))")
                } else {
                    log(1, "Reused \(expectedSize) verified bytes of \(safePath) (\(index + 1)/\(needed.count))")
                }

                completedBytes += expectedSize
                reportProgress(
                    operationID, courier_phase_verifying,
                    "Verified (\(index + 1)/\(needed.count))",
                    totalBytes > 0 ? Int(Double(completedBytes) / Double(totalBytes) * 100) : 100,
                    completedBytes, totalBytes, throughput, index + 1, needed.count)
            }

            let replacementPaths = needed.map(\.targetRelativePath)
            let replacementHadOriginal = try replacementPaths.map {
                let destination = try safeDestination(root: installRoot, relativePath: $0)
                return fileManager.fileExists(atPath: destination.path)
            }
            let journal = UpdateJournal(
                schemaVersion: 2,
                replacementPaths: replacementPaths,
                replacementHadOriginal: replacementHadOriginal,
                replacementStagedPaths: needed.map(\.stagedRelativePath),
                obsoletePaths: obsolete,
                versionMetadataExisted: fileManager.fileExists(atPath: installRoot.appendingPathComponent("version.json").path),
                managedMetadataExisted: fileManager.fileExists(atPath: installRoot.appendingPathComponent(managedManifestFileName).path))
            let journalURL = installRoot.appendingPathComponent(journalFileName)
            try atomicWriteJSON(journal, to: journalURL)

            do {
                try backupMetadataIfPresent(installRoot: installRoot, backupRoot: backupRoot, name: "version.json")
                try backupMetadataIfPresent(installRoot: installRoot, backupRoot: backupRoot, name: managedManifestFileName)

                for planned in needed {
                    try Task.checkCancellation()
                    let staged = try safeDestination(root: stagingRoot, relativePath: planned.stagedRelativePath)
                    let destination = try safeDestination(root: installRoot, relativePath: planned.targetRelativePath)
                    let backup = try safeDestination(root: backupRoot, relativePath: planned.targetRelativePath)
                    try ensureNoSymlinkEscape(root: installRoot, relativePath: planned.targetRelativePath, includeLeaf: true)
                    try createParent(of: destination)

                    if fileManager.fileExists(atPath: destination.path) {
                        try createParent(of: backup)
                        try fileManager.moveItem(at: destination, to: backup)
                    }
                    try fileManager.moveItem(at: staged, to: destination)
                }

                for relative in obsolete {
                    try Task.checkCancellation()
                    let destination = try safeDestination(root: installRoot, relativePath: relative)
                    guard fileManager.fileExists(atPath: destination.path) else { continue }
                    let backup = try safeDestination(root: backupRoot, relativePath: relative)
                    try createParent(of: backup)
                    try fileManager.moveItem(at: destination, to: backup)
                }

                try writeVersionJSON(installRoot: installRoot, version: version)
                try atomicWriteJSON(
                    ManagedManifest(schemaVersion: 1, releaseVersion: version, files: targetPaths.sorted()),
                    to: installRoot.appendingPathComponent(managedManifestFileName))

                try removeIfPresent(journalURL)
                try? removeIfPresent(stagingRoot)
                try? removeIfPresent(stagingManifestURL)
                try? removeIfPresent(backupRoot)
            } catch {
                do {
                    try recoverInterruptedUpdate(installRoot: installRoot)
                } catch let recoveryError {
                    log(4, "Update failed and rollback also failed: \(recoveryError)")
                }
                throw error
            }

            let message: String
            if needed.isEmpty && obsolete.isEmpty {
                message = "Already up to date."
            } else {
                message = "Updated \(needed.count) file(s) and removed \(obsolete.count) obsolete file(s)."
            }
            reportProgress(operationID, courier_phase_downloading, message, 100,
                           totalBytes, totalBytes, 0, needed.count, needed.count)
            reportDone(operationID, true, message)
        }
    }
}
