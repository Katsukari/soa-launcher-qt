import Foundation
import Soa_Courier

final class Courier
	{

	let cdnBaseURL: String
	let onProgress: courier_progress_cb
	let onDone: courier_done_cb
	let ctx: UnsafeMutableRawPointer?

	private var task: Task<Void, Never>?

	init(cdnBaseURL: String,
	onProgress: @escaping courier_progress_cb,
	onDone: @escaping courier_done_cb,
	ctx: UnsafeMutableRawPointer?)
		{
		self.cdnBaseURL = cdnBaseURL.hasSuffix("/")
		? String(cdnBaseURL.dropLast())
		: cdnBaseURL
		self.onProgress = onProgress
		self.onDone = onDone
		self.ctx = ctx
	}

	func reportProgress(_ phase: courier_phase,
	_ message: String,
	_ percent: Int,
	_ received: UInt64,
	_ total: UInt64,
	_ throughput: UInt64)
		{
		message.withCString { c in
			onProgress(phase, c, Int32(percent), received, total, throughput, ctx)
		}
	}

	func reportDone(_ ok: Bool, _ message: String)
		{
		message.withCString { c in onDone(ok, c, ctx) }
	}

	func cancel()
		{
		task?.cancel()
	}

	func run(_ body: @escaping () async throws -> Void)
	{
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