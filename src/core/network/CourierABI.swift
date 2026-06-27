import Foundation
import Soa_Courier

@_cdecl("courier_create")
public func courier_create(_ cdn: UnsafePointer<CChar>?,
_ onProgress: courier_progress_cb?,
_ onDone: courier_done_cb?,
_ ctx: UnsafeMutableRawPointer?) -> UnsafeMutableRawPointer?
{
	guard let cdn, let onProgress, let onDone else { return nil }
	let d = Courier(cdnBaseURL: String(cString: cdn),
		onProgress: onProgress,
		onDone: onDone,
		ctx: ctx)
	return Unmanaged.passRetained(d).toOpaque()
}

@_cdecl("courier_destroy")
public func courier_destroy(_ ptr: UnsafeMutableRawPointer?)
{
	guard let ptr else { return }
	Unmanaged<Courier>.fromOpaque(ptr).release()
}

@_cdecl("courier_integrity_check")
public func courier_integrity_check(_ ptr: UnsafeMutableRawPointer?,
_ installPath: UnsafePointer<CChar>?)
{
	guard let ptr, let installPath else { return }
	let d = Unmanaged<Courier>.fromOpaque(ptr).takeUnretainedValue()
	d.startIntegrityCheck(installPath: String(cString: installPath))
}

@_cdecl("courier_update_check")
public func courier_update_check(_ ptr: UnsafeMutableRawPointer?,
_ installPath: UnsafePointer<CChar>?)
{
	guard let ptr, let installPath else { return }
	let d = Unmanaged<Courier>.fromOpaque(ptr).takeUnretainedValue()
	d.startUpdateCheck(installPath: String(cString: installPath))
}

@_cdecl("courier_update")
public func courier_update(_ ptr: UnsafeMutableRawPointer?,
_ installPath: UnsafePointer<CChar>?)
{
	guard let ptr, let installPath else { return }
	let d = Unmanaged<Courier>.fromOpaque(ptr).takeUnretainedValue()
	d.startUpdate(installPath: String(cString: installPath))
}

@_cdecl("courier_cancel")
public func courier_cancel(_ ptr: UnsafeMutableRawPointer?)
{
	guard let ptr else { return }
	let d = Unmanaged<Courier>.fromOpaque(ptr).takeUnretainedValue()
	d.cancel()
}