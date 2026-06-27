import Foundation
import Soa_Courier

@_cdecl("soa_ping")
public func soa_ping()
{
	log(2, "ping from swift!")
}