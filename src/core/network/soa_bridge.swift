import Foundation
import SoaBridge
@_cdecl("soa_ping")
public func soa_ping()
{
	"ping from swift!".withCString
	{
		cstr in
		soa_log(2, cstr)   // call back into C++ logging (level 2 = info)
	}
}