import Foundation
#if canImport(FoundationNetworking)
import FoundationNetworking
#endif
import Soa_Courier

struct LauncherUpdateSelection
{
    let version: String
    let minimumVersion: String
    let message: String
    let packageKind: String
    let fileName: String
    let packageURL: URL
    let sha256: String
    let expectedSize: UInt64
    let required: Bool
}

struct LauncherServiceFailure: Error
{
    let code: soa_launcher_error
    let status: Int
    let detail: String

    init(_ code: soa_launcher_error, _ detail: String, status: Int = 0)
    {
        self.code = code
        self.status = status
        self.detail = detail
    }
}

final class LauncherAssetDelegate: NSObject, URLSessionDataDelegate, URLSessionTaskDelegate, @unchecked Sendable
{
    private let destination: URL
    private let expectedSize: UInt64
    private let expectedDigest: String
    private let progress: @Sendable (UInt64, UInt64) -> Void
    private let queue: OperationQueue
    private var session: URLSession?
    private var task: URLSessionDataTask?
    private var continuation: CheckedContinuation<Void, Error>?
    private var handle: FileHandle?
    private var terminalError: LauncherServiceFailure?
    private var received: UInt64 = 0
    private var hasher = SHA256()
    private var completed = false

    init(destination: URL,
         expectedSize: UInt64,
         expectedDigest: String,
         progress: @escaping @Sendable (UInt64, UInt64) -> Void)
    {
        self.destination = destination
        self.expectedSize = expectedSize
        self.expectedDigest = expectedDigest
        self.progress = progress
        self.queue = OperationQueue()
        self.queue.maxConcurrentOperationCount = 1
        self.queue.qualityOfService = .utility
    }

    func start(request: URLRequest, configuration: URLSessionConfiguration) async throws
    {
        try await withTaskCancellationHandler(operation: {
            try await withCheckedThrowingContinuation { continuation in
                self.continuation = continuation
                let session = URLSession(configuration: configuration, delegate: self, delegateQueue: queue)
                self.session = session
                let task = session.dataTask(with: request)
                self.task = task
                task.resume()
            }
        }, onCancel: {
            self.task?.cancel()
        })
    }

    func urlSession(_ session: URLSession,
                    task: URLSessionTask,
                    willPerformHTTPRedirection response: HTTPURLResponse,
                    newRequest request: URLRequest,
                    completionHandler: @escaping (URLRequest?) -> Void)
    {
        guard let url = request.url,
              url.scheme?.lowercased() == "https",
              LauncherUpdateService.trustedReleaseHost(url.host?.lowercased() ?? "") else {
            terminalError = LauncherServiceFailure(
                soa_launcher_error_unsafe_url,
                "The launcher update redirected to an untrusted URL")
            completionHandler(nil)
            return
        }
        completionHandler(request)
    }

    func urlSession(_ session: URLSession,
                    dataTask: URLSessionDataTask,
                    didReceive response: URLResponse,
                    completionHandler: @escaping (URLSession.ResponseDisposition) -> Void)
    {
        guard let http = response as? HTTPURLResponse else {
            terminalError = LauncherServiceFailure(
                soa_launcher_error_network,
                "The launcher update returned a non-HTTP response")
            completionHandler(.cancel)
            return
        }
        guard (200...299).contains(http.statusCode) else {
            terminalError = LauncherServiceFailure(
                soa_launcher_error_http,
                "HTTP \(http.statusCode)",
                status: http.statusCode)
            completionHandler(.cancel)
            return
        }
        if http.expectedContentLength >= 0
            && UInt64(http.expectedContentLength) != expectedSize {
            terminalError = LauncherServiceFailure(
                soa_launcher_error_size_mismatch,
                "The server reported an unexpected launcher update size",
                status: http.statusCode)
            completionHandler(.cancel)
            return
        }

        do {
            try FileManager.default.createDirectory(
                at: destination.deletingLastPathComponent(),
                withIntermediateDirectories: true)
            try? FileManager.default.removeItem(at: destination)
            guard FileManager.default.createFile(atPath: destination.path, contents: nil) else {
                throw LauncherServiceFailure(
                    soa_launcher_error_destination,
                    "The launcher could not create the temporary update file")
            }
            handle = try FileHandle(forWritingTo: destination)
            completionHandler(.allow)
        } catch let failure as LauncherServiceFailure {
            terminalError = failure
            completionHandler(.cancel)
        } catch {
            terminalError = LauncherServiceFailure(
                soa_launcher_error_destination,
                error.localizedDescription)
            completionHandler(.cancel)
        }
    }

    func urlSession(_ session: URLSession, dataTask: URLSessionDataTask, didReceive data: Data)
    {
        guard terminalError == nil else { return }
        let count = UInt64(data.count)
        if received > expectedSize || count > expectedSize - received {
            terminalError = LauncherServiceFailure(
                soa_launcher_error_size_mismatch,
                "The launcher update exceeded its expected size")
            dataTask.cancel()
            return
        }
        do {
            try handle?.write(contentsOf: data)
            hasher.update(data)
            received += count
            progress(received, expectedSize)
        } catch {
            terminalError = LauncherServiceFailure(
                soa_launcher_error_write,
                error.localizedDescription)
            dataTask.cancel()
        }
    }

    func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?)
    {
        guard !completed else { return }
        completed = true
        try? handle?.synchronize()
        try? handle?.close()
        handle = nil

        defer {
            self.task = nil
            self.session = nil
            session.finishTasksAndInvalidate()
            continuation = nil
        }

        if let terminalError {
            continuation?.resume(throwing: terminalError)
            return
        }
        if let error {
            if (error as NSError).code == NSURLErrorCancelled {
                continuation?.resume(throwing: CancellationError())
            } else {
                continuation?.resume(throwing: LauncherServiceFailure(
                    soa_launcher_error_network,
                    error.localizedDescription))
            }
            return
        }
        guard received == expectedSize else {
            continuation?.resume(throwing: LauncherServiceFailure(
                soa_launcher_error_size_mismatch,
                "The downloaded launcher update has an unexpected size"))
            return
        }
        let digest = hasher.finalizeHex().lowercased()
        guard digest == expectedDigest else {
            continuation?.resume(throwing: LauncherServiceFailure(
                soa_launcher_error_digest_mismatch,
                "The launcher update failed SHA-256 verification"))
            return
        }
        continuation?.resume()
    }
}

final class LauncherUpdateService: @unchecked Sendable
{
    private let repository: String
    private let currentVersion: String
    private let platform: String
    private let downloadDirectory: URL
    private let userAgent: String
    private let allowInsecureHTTP: Bool
    private let checkDone: soa_launcher_check_cb
    private let progress: soa_launcher_progress_cb
    private let downloadDone: soa_launcher_download_cb
    private let context: UnsafeMutableRawPointer?
    private let gate = NetworkCallbackGate()
    private let lock = NSLock()
    private var task: Task<Void, Never>?
    private var selection: LauncherUpdateSelection?
    private var stopped = false

    init(repository: String,
         currentVersion: String,
         platform: String,
         downloadDirectory: String,
         userAgent: String,
         allowInsecureHTTP: Bool,
         checkDone: @escaping soa_launcher_check_cb,
         progress: @escaping soa_launcher_progress_cb,
         downloadDone: @escaping soa_launcher_download_cb,
         context: UnsafeMutableRawPointer?)
    {
        self.repository = repository
        self.currentVersion = currentVersion
        self.platform = platform
        self.downloadDirectory = URL(fileURLWithPath: downloadDirectory, isDirectory: true)
        self.userAgent = userAgent
        self.allowInsecureHTTP = allowInsecureHTTP
        self.checkDone = checkDone
        self.progress = progress
        self.downloadDone = downloadDone
        self.context = context
    }

    func shutdown()
    {
        lock.lock()
        stopped = true
        let active = task
        task = nil
        selection = nil
        lock.unlock()
        active?.cancel()
        gate.shutdown()
    }

    func cancel()
    {
        lock.lock()
        let active = task
        lock.unlock()
        active?.cancel()
    }

    func check()
    {
        lock.lock()
        if stopped {
            lock.unlock()
            return
        }
        if task != nil {
            lock.unlock()
            reportCheck(result: soa_launcher_check_failed,
                        failure: LauncherServiceFailure(
                            soa_launcher_error_busy,
                            "A launcher update operation is already running"),
                        selection: nil)
            return
        }
        selection = nil
        let operation = Task { [weak self] in
            await self?.performCheck()
            self?.clearTask()
        }
        task = operation
        lock.unlock()
    }

    func download()
    {
        lock.lock()
        if stopped {
            lock.unlock()
            return
        }
        guard task == nil else {
            lock.unlock()
            reportDownload(result: soa_launcher_download_failed,
                           failure: LauncherServiceFailure(
                               soa_launcher_error_busy,
                               "A launcher update operation is already running"),
                           finalPath: "")
            return
        }
        guard let selected = selection else {
            lock.unlock()
            reportDownload(result: soa_launcher_download_failed,
                           failure: LauncherServiceFailure(
                               soa_launcher_error_invalid_release,
                               "No launcher update is ready to download"),
                           finalPath: "")
            return
        }
        let operation = Task { [weak self] in
            await self?.performDownload(selected)
            self?.clearTask()
        }
        task = operation
        lock.unlock()
    }

    private func clearTask()
    {
        lock.lock()
        task = nil
        lock.unlock()
    }


    private func storeSelection(_ value: LauncherUpdateSelection)
    {
        lock.lock()
        selection = value
        lock.unlock()
    }

    private func performCheck() async
    {
        guard repository.range(
            of: #"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$"#,
            options: .regularExpression) != nil,
              !currentVersion.isEmpty,
              ["linux-x86_64", "macos-arm64", "macos-x86_64"].contains(platform) else {
            reportCheck(result: soa_launcher_check_failed,
                        failure: LauncherServiceFailure(
                            soa_launcher_error_invalid_configuration,
                            "The launcher update configuration is invalid"),
                        selection: nil)
            return
        }

        guard let url = secureURL(
            "https://api.github.com/repos/\(repository)/releases/latest",
            allowInsecureHTTP: allowInsecureHTTP) else {
            reportCheck(result: soa_launcher_check_failed,
                        failure: LauncherServiceFailure(
                            soa_launcher_error_invalid_configuration,
                            "The GitHub release URL is invalid"),
                        selection: nil)
            return
        }

        var request = URLRequest(url: url)
        request.setValue(userAgent, forHTTPHeaderField: "User-Agent")
        request.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
        request.setValue("2022-11-28", forHTTPHeaderField: "X-GitHub-Api-Version")
        request.setValue("no-store", forHTTPHeaderField: "Cache-Control")

        do {
            let response = try await fetchBounded(
                request,
                timeoutMilliseconds: 12_000,
                maximumBytes: 4 * 1024 * 1024,
                allowInsecureHTTP: allowInsecureHTTP,
                redirectValidator: { source, destination in
                    source.scheme?.lowercased() == destination.scheme?.lowercased()
                    && destination.host?.lowercased() == "api.github.com"
                })
            guard (200...299).contains(response.status) else {
                throw LauncherServiceFailure(
                    soa_launcher_error_http,
                    "HTTP \(response.status)",
                    status: response.status)
            }
            guard let selected = try parseRelease(response.data) else {
                reportCheck(result: soa_launcher_check_no_update, failure: nil, selection: nil)
                return
            }
            storeSelection(selected)
            reportCheck(result: soa_launcher_check_update_available,
                        failure: nil,
                        selection: selected)
        } catch is CancellationError {
            reportCheck(result: soa_launcher_check_cancelled,
                        failure: LauncherServiceFailure(
                            soa_launcher_error_cancelled,
                            "Cancelled"),
                        selection: nil)
        } catch let failure as LauncherServiceFailure {
            reportCheck(result: soa_launcher_check_failed, failure: failure, selection: nil)
        } catch let failure as NetworkFailure {
            let code: soa_launcher_error = failure.message.contains("allowed size")
                ? soa_launcher_error_response_too_large
                : soa_launcher_error_network
            reportCheck(result: soa_launcher_check_failed,
                        failure: LauncherServiceFailure(code, failure.message, status: failure.status),
                        selection: nil)
        } catch {
            reportCheck(result: soa_launcher_check_failed,
                        failure: LauncherServiceFailure(
                            soa_launcher_error_network,
                            error.localizedDescription),
                        selection: nil)
        }
    }

    private func parseRelease(_ data: Data) throws -> LauncherUpdateSelection?
    {
        guard let release = try JSONSerialization.jsonObject(with: data) as? [String: Any],
              release["draft"] as? Bool != true,
              release["prerelease"] as? Bool != true else {
            throw LauncherServiceFailure(
                soa_launcher_error_invalid_release,
                "GitHub returned invalid launcher release information")
        }

        let tag = normalizedVersion((release["tag_name"] as? String) ?? (release["name"] as? String) ?? "")
        guard validVersion(tag) else {
            throw LauncherServiceFailure(
                soa_launcher_error_invalid_release,
                "The latest launcher release has an invalid version tag")
        }
        if compareVersions(tag, currentVersion) <= 0 {
            return nil
        }

        let body = release["body"] as? String ?? ""
        let minimum = normalizedVersion(releaseDirective(body, key: "minimum-version"))
        if !minimum.isEmpty && !validVersion(minimum) {
            throw LauncherServiceFailure(
                soa_launcher_error_invalid_release,
                "The launcher release has an invalid minimum version")
        }

        guard let assets = release["assets"] as? [[String: Any]],
              let asset = selectAsset(assets) else {
            throw LauncherServiceFailure(
                soa_launcher_error_missing_asset,
                "No compatible launcher package was attached to the release")
        }

        let name = safeFileName(asset["name"] as? String ?? "")
        let urlText = asset["browser_download_url"] as? String ?? ""
        guard !name.isEmpty,
              let url = secureURL(urlText, allowInsecureHTTP: allowInsecureHTTP),
              Self.trustedReleaseHost(url.host?.lowercased() ?? "") else {
            throw LauncherServiceFailure(
                soa_launcher_error_unsafe_url,
                "The launcher release contains an invalid package URL")
        }

        var digest = (asset["digest"] as? String ?? "")
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .lowercased()
        if digest.hasPrefix("sha256:") { digest.removeFirst(7) }
        guard digest.range(of: #"^[0-9a-f]{64}$"#, options: .regularExpression) != nil else {
            throw LauncherServiceFailure(
                soa_launcher_error_missing_digest,
                "The launcher release package has no valid SHA-256 digest")
        }

        let expectedSize: UInt64
        if let number = asset["size"] as? NSNumber, number.int64Value > 0 {
            expectedSize = UInt64(number.int64Value)
        } else if let value = asset["size"] as? String,
                  let parsed = UInt64(value), parsed > 0 {
            expectedSize = parsed
        } else {
            throw LauncherServiceFailure(
                soa_launcher_error_invalid_size,
                "The launcher release package has no valid size")
        }
        guard expectedSize <= 4 * 1024 * 1024 * 1024 else {
            throw LauncherServiceFailure(
                soa_launcher_error_invalid_size,
                "The launcher release package exceeds the maximum allowed size")
        }

        return LauncherUpdateSelection(
            version: tag,
            minimumVersion: minimum,
            message: releaseSummary(body),
            packageKind: platform.hasPrefix("linux-") ? "appimage" : "dmg",
            fileName: name,
            packageURL: url,
            sha256: digest,
            expectedSize: expectedSize,
            required: requiredDirective(body)
                || (!minimum.isEmpty && compareVersions(currentVersion, minimum) < 0))
    }

    private func performDownload(_ selected: LauncherUpdateSelection) async
    {
        let finalURL = downloadDirectory.appendingPathComponent(selected.fileName)
        let partialURL = finalURL.appendingPathExtension("part")
        do {
            try FileManager.default.createDirectory(
                at: downloadDirectory,
                withIntermediateDirectories: true)
            var request = URLRequest(url: selected.packageURL)
            request.setValue(userAgent, forHTTPHeaderField: "User-Agent")
            request.setValue("application/octet-stream", forHTTPHeaderField: "Accept")
            request.setValue("identity", forHTTPHeaderField: "Accept-Encoding")
            request.setValue("no-store", forHTTPHeaderField: "Cache-Control")

            let configuration = URLSessionConfiguration.ephemeral
            configuration.timeoutIntervalForRequest = 30
            configuration.timeoutIntervalForResource = 60 * 60
            configuration.requestCachePolicy = .reloadIgnoringLocalAndRemoteCacheData
            configuration.urlCache = nil

            let delegate = LauncherAssetDelegate(
                destination: partialURL,
                expectedSize: selected.expectedSize,
                expectedDigest: selected.sha256,
                progress: { [weak self] received, total in
                    self?.reportProgress(received: received, total: total)
                })
            reportProgress(received: 0, total: selected.expectedSize)
            try await delegate.start(request: request, configuration: configuration)
            try? FileManager.default.removeItem(at: finalURL)
            do {
                try FileManager.default.moveItem(at: partialURL, to: finalURL)
            } catch {
                throw LauncherServiceFailure(
                    soa_launcher_error_finalize,
                    error.localizedDescription)
            }
            reportDownload(result: soa_launcher_download_completed,
                           failure: nil,
                           finalPath: finalURL.path)
        } catch is CancellationError {
            try? FileManager.default.removeItem(at: partialURL)
            reportDownload(result: soa_launcher_download_cancelled,
                           failure: LauncherServiceFailure(
                               soa_launcher_error_cancelled,
                               "Cancelled"),
                           finalPath: "")
        } catch let failure as LauncherServiceFailure {
            try? FileManager.default.removeItem(at: partialURL)
            reportDownload(result: soa_launcher_download_failed,
                           failure: failure,
                           finalPath: "")
        } catch {
            try? FileManager.default.removeItem(at: partialURL)
            reportDownload(result: soa_launcher_download_failed,
                           failure: LauncherServiceFailure(
                               soa_launcher_error_network,
                               error.localizedDescription),
                           finalPath: "")
        }
    }

    private func reportCheck(result: soa_launcher_check_result,
                             failure: LauncherServiceFailure?,
                             selection: LauncherUpdateSelection?)
    {
        let callback = checkDone
        let contextAddress = context.map { UInt(bitPattern: $0) } ?? 0
        let empty = ""
        let version = selection?.version ?? empty
        let minimum = selection?.minimumVersion ?? empty
        let message = selection?.message ?? empty
        let kind = selection?.packageKind ?? empty
        let name = selection?.fileName ?? empty
        let url = selection?.packageURL.absoluteString ?? empty
        let digest = selection?.sha256 ?? empty
        let detail = failure?.detail ?? empty
        let expectedSize = selection?.expectedSize ?? 0
        let required = selection?.required ?? false
        let errorCode = failure?.code ?? soa_launcher_error_none
        let status = failure?.status ?? 0

        gate.submit {
            detail.withCString { detailPointer in
                version.withCString { versionPointer in
                    minimum.withCString { minimumPointer in
                        message.withCString { messagePointer in
                            kind.withCString { kindPointer in
                                name.withCString { namePointer in
                                    url.withCString { urlPointer in
                                        digest.withCString { digestPointer in
                                            callback(result, errorCode, Int32(status), detailPointer,
                                                     versionPointer, minimumPointer, messagePointer,
                                                     kindPointer, namePointer, urlPointer, digestPointer,
                                                     expectedSize, required,
                                                     contextAddress == 0 ? nil : UnsafeMutableRawPointer(bitPattern: contextAddress))
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private func reportProgress(received: UInt64, total: UInt64)
    {
        let callback = progress
        let contextAddress = context.map { UInt(bitPattern: $0) } ?? 0
        gate.submit {
            callback(received, total,
                     contextAddress == 0 ? nil : UnsafeMutableRawPointer(bitPattern: contextAddress))
        }
    }

    private func reportDownload(result: soa_launcher_download_result,
                                failure: LauncherServiceFailure?,
                                finalPath: String)
    {
        let callback = downloadDone
        let contextAddress = context.map { UInt(bitPattern: $0) } ?? 0
        let detail = failure?.detail ?? ""
        let errorCode = failure?.code ?? soa_launcher_error_none
        let status = failure?.status ?? 0
        gate.submit {
            detail.withCString { detailPointer in
                finalPath.withCString { pathPointer in
                    callback(result, errorCode, Int32(status), detailPointer, pathPointer,
                             contextAddress == 0 ? nil : UnsafeMutableRawPointer(bitPattern: contextAddress))
                }
            }
        }
    }

    private func selectAsset(_ assets: [[String: Any]]) -> [String: Any]?
    {
        var selected: [String: Any]?
        var best = -1
        for asset in assets {
            if let state = asset["state"] as? String, state != "uploaded" { continue }
            let score = assetScore(asset["name"] as? String ?? "")
            if score > best {
                selected = asset
                best = score
            }
        }
        return selected
    }

    private func assetScore(_ name: String) -> Int
    {
        let lower = name.lowercased()
        var score = 0
        if platform == "linux-x86_64" {
            guard lower.hasSuffix(".appimage"),
                  !lower.contains("arm64"),
                  !lower.contains("aarch64") else { return -1 }
            score = 100
            if lower.contains("x86_64") || lower.contains("amd64") { score += 100 }
        } else if platform == "macos-arm64" {
            guard lower.hasSuffix(".dmg"),
                  !lower.contains("x86_64"),
                  !lower.contains("amd64"),
                  !lower.contains("intel") else { return -1 }
            score = 100
            if lower.contains("arm64") || lower.contains("aarch64") || lower.contains("apple-silicon") {
                score += 100
            } else if lower.contains("universal") {
                score += 60
            }
        } else if platform == "macos-x86_64" {
            guard lower.hasSuffix(".dmg"),
                  !lower.contains("arm64"),
                  !lower.contains("aarch64"),
                  !lower.contains("apple-silicon") else { return -1 }
            score = 100
            if lower.contains("x86_64") || lower.contains("amd64") || lower.contains("intel") {
                score += 100
            } else if lower.contains("universal") {
                score += 60
            }
        } else {
            return -1
        }
        if lower.contains("story_of_alicia")
            || lower.contains("story-of-alicia")
            || lower.contains("soa-launcher") {
            score += 20
        }
        return score
    }

    private func normalizedVersion(_ value: String) -> String
    {
        var output = value.trimmingCharacters(in: .whitespacesAndNewlines)
        if output.lowercased().hasPrefix("v") { output.removeFirst() }
        return output
    }

    private func validVersion(_ value: String) -> Bool
    {
        parseVersion(value) != nil
    }

    private func parseVersion(_ value: String) -> ([UInt64], [String])?
    {
        let normalized = normalizedVersion(value)
        let withoutBuild = normalized.split(separator: "+", maxSplits: 1).first.map(String.init) ?? normalized
        let pieces = withoutBuild.split(separator: "-", maxSplits: 1, omittingEmptySubsequences: false)
        let core = pieces.first.map(String.init) ?? ""
        let components = core.split(separator: ".", omittingEmptySubsequences: false)
        guard !components.isEmpty else { return nil }
        var numbers: [UInt64] = []
        for component in components {
            guard !component.isEmpty,
                  component.allSatisfy({ $0.isNumber }),
                  let number = UInt64(component) else { return nil }
            numbers.append(number)
        }
        let prerelease = pieces.count > 1
            ? pieces[1].split(separator: ".", omittingEmptySubsequences: false).map(String.init)
            : []
        if prerelease.contains(where: { $0.isEmpty }) { return nil }
        return (numbers, prerelease)
    }

    private func compareVersions(_ left: String, _ right: String) -> Int
    {
        guard let lhs = parseVersion(left), let rhs = parseVersion(right) else {
            return left.caseInsensitiveCompare(right) == .orderedAscending ? -1
                : left.caseInsensitiveCompare(right) == .orderedDescending ? 1 : 0
        }
        let count = max(lhs.0.count, rhs.0.count)
        for index in 0..<count {
            let l = index < lhs.0.count ? lhs.0[index] : 0
            let r = index < rhs.0.count ? rhs.0[index] : 0
            if l < r { return -1 }
            if l > r { return 1 }
        }
        if lhs.1.isEmpty && rhs.1.isEmpty { return 0 }
        if lhs.1.isEmpty { return 1 }
        if rhs.1.isEmpty { return -1 }
        let preCount = min(lhs.1.count, rhs.1.count)
        for index in 0..<preCount {
            let l = lhs.1[index]
            let r = rhs.1[index]
            let ln = UInt64(l)
            let rn = UInt64(r)
            if let ln, let rn {
                if ln < rn { return -1 }
                if ln > rn { return 1 }
            } else if ln != nil {
                return -1
            } else if rn != nil {
                return 1
            } else {
                let comparison = l.caseInsensitiveCompare(r)
                if comparison == .orderedAscending { return -1 }
                if comparison == .orderedDescending { return 1 }
            }
        }
        if lhs.1.count < rhs.1.count { return -1 }
        if lhs.1.count > rhs.1.count { return 1 }
        return 0
    }

    private func releaseDirective(_ body: String, key: String) -> String
    {
        let escaped = NSRegularExpression.escapedPattern(for: key)
        guard let expression = try? NSRegularExpression(
            pattern: "<!--\\s*soa-launcher-\(escaped)\\s*:\\s*([^>]*?)\\s*-->",
            options: [.caseInsensitive]) else { return "" }
        let range = NSRange(body.startIndex..<body.endIndex, in: body)
        guard let match = expression.firstMatch(in: body, range: range),
              let valueRange = Range(match.range(at: 1), in: body) else { return "" }
        return String(body[valueRange]).trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private func requiredDirective(_ body: String) -> Bool
    {
        let value = releaseDirective(body, key: "update").lowercased()
        return ["required", "mandatory", "true", "yes"].contains(value)
            || body.range(of: "[launcher-update-required]", options: .caseInsensitive) != nil
    }

    private func releaseSummary(_ body: String) -> String
    {
        var output = body
        output = output.replacingOccurrences(
            of: #"<!--\s*soa-launcher-[^>]*-->"#,
            with: "",
            options: [.regularExpression, .caseInsensitive])
        output = output.replacingOccurrences(
            of: "[launcher-update-required]",
            with: "",
            options: .caseInsensitive)
        output = output.replacingOccurrences(of: #"<[^>]+>"#, with: " ", options: .regularExpression)
        output = output.replacingOccurrences(
            of: #"\[([^\]]+)\]\([^\)]+\)"#,
            with: "$1",
            options: .regularExpression)
        for raw in output.components(separatedBy: .newlines) {
            var line = raw.trimmingCharacters(in: .whitespacesAndNewlines)
            line = line.replacingOccurrences(
                of: #"^[#>*_`~\-\s]+"#,
                with: "",
                options: .regularExpression)
            line = line.split(whereSeparator: { $0.isWhitespace }).joined(separator: " ")
            if line.isEmpty { continue }
            if line.count > 220 {
                return String(line.prefix(217)).trimmingCharacters(in: .whitespaces) + "..."
            }
            return line
        }
        return ""
    }

    private func safeFileName(_ value: String) -> String
    {
        var output = URL(fileURLWithPath: value).lastPathComponent
        output = output.replacingOccurrences(
            of: #"[^A-Za-z0-9._ -]"#,
            with: "_",
            options: .regularExpression)
        while output.hasPrefix(".") { output.removeFirst() }
        return String(output.prefix(180))
    }

    static func trustedReleaseHost(_ host: String) -> Bool
    {
        let allowed = [
            "github.com",
            "api.github.com",
            "objects.githubusercontent.com",
            "github-releases.githubusercontent.com",
            "release-assets.githubusercontent.com"
        ]
        return allowed.contains(host)
            || host.hasSuffix(".githubusercontent.com")
    }
}
