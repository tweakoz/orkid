// Orkid - Main singleton for engine lifecycle management
import Foundation

public final class Orkid {

    public static let shared = Orkid()

    private init() {
        // Initialize C++ layer (type registration happens here in C++)
        let args = CommandLine.unsafeArgv
        orkid_swift_init(CommandLine.argc, args)
    }

    public static func poll() {
        orkid_swift_poll()
    }

    public static func exit() {
        orkid_swift_exit()
    }

    /// Get the last error message from C++ layer
    public static var lastError: String {
        if let cstr = orkid_get_last_error() {
            return String(cString: cstr)
        }
        return ""
    }
}
