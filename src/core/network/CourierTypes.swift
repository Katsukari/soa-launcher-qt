import Foundation
import Soa_Courier

func log(_ level: Int32, _ message: String)
{
	message.withCString { soa_log(level, $0) }
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

let dxvkBackedUpDlls: Set<String> = ["d3dx9_31.dll", "d3dx9_42.dll"]

struct Err: Error
{
	let message: String
	init(_ m: String)
	{
		message = m
	}
}