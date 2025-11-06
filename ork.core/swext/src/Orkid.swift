// OrkCore Module - Engine lifecycle management
import Foundation

private var _initialized = false

/// Initialize Orkid engine (must be called on main thread)
public func initialize() {
    dispatchPrecondition(condition: .onQueue(.main))

    guard !_initialized else {
        print("Warning: initialize() called multiple times")
        return
    }

    // Initialize C++ layer (type registration happens here in C++)
    let args = CommandLine.unsafeArgv
    orkid_swift_init(CommandLine.argc, args)
    _initialized = true
}

/// Poll engine (must be called on main thread)
public func poll() {
    dispatchPrecondition(condition: .onQueue(.main))
    orkid_swift_poll()
}

/// Exit engine (must be called on main thread)
public func exit() {
    dispatchPrecondition(condition: .onQueue(.main))
    orkid_swift_exit()
}

/// Get the last error message from C++ layer
public var lastError: String {
    if let cstr = orkid_get_last_error() {
        return String(cString: cstr)
    }
    return ""
}
