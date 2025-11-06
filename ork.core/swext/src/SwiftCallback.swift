////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2025, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

import Foundation

/// Shared argument decoder - used by all callback types
private func decodeCallbackArgument(handle: OpaquePointer) -> Any {
    // Try int
    var intVal: Int32 = 0
    if orkid_try_decode_int(handle, &intVal) { return Int(intVal) }

    // Try float
    var floatVal: Float = 0
    if orkid_try_decode_float(handle, &floatVal) { return floatVal }

    // Try double
    var doubleVal: Double = 0
    if orkid_try_decode_double(handle, &doubleVal) { return doubleVal }

    // Try string
    if let strPtr = orkid_try_decode_string(handle) {
        return String(cString: strPtr)
    }

    // Fall back to OrkidObject (use registry to get correct subclass)
    return OrkidObjectRegistry.shared.wrap(handle: handle)
}

/// SwiftCallback - Simple callback with no arguments
public final class SwiftCallback: OrkidObject {
    private let closure: () -> Void
    private let callbackId: UInt64  // Cache ID for deinit

    public init(closure: @escaping () -> Void) {
        self.closure = closure
        let handle = orkid_swiftcallback_create()!
        self.callbackId = orkid_swiftcallback_get_id(handle)  // Cache before super.init
        super.init(handle: handle, owned: true)

        // Ensure manager is initialized (triggers function pointer registration)
        _ = SwiftCallbackManager.shared

        // Register in global callback registry
        SwiftCallbackManager.shared.register(id: callbackId, callback: self)
    }

    deinit {
        SwiftCallbackManager.shared.unregister(id: callbackId)  // CRITICAL: Prevent memory leak
    }

    internal func invoke(argsHandle: OpaquePointer?) {
        closure()
    }
}

// Conform to protocol for type-erased storage
extension SwiftCallback: SwiftCallbackInvocable {}

/// SwiftCallback1 - Callback with 1 argument (Any: primitives or OrkidObject)
public final class SwiftCallback1: OrkidObject {
    private let closure: (Any) -> Void
    private let callbackId: UInt64

    public init(closure: @escaping (Any) -> Void) {
        self.closure = closure
        let handle = orkid_swiftcallback_create()!
        self.callbackId = orkid_swiftcallback_get_id(handle)
        super.init(handle: handle, owned: true)

        _ = SwiftCallbackManager.shared
        SwiftCallbackManager.shared.register(id: callbackId, callback: self)
    }

    deinit {
        SwiftCallbackManager.shared.unregister(id: callbackId)
    }

    internal func invoke(argsHandle: OpaquePointer?) {
        guard let handle = argsHandle else {
            print("Warning: SwiftCallback1 invoked without argument")
            return
        }

        let arg = decodeCallbackArgument(handle: handle)
        closure(arg)
    }
}

extension SwiftCallback1: SwiftCallbackInvocable {}

/// SwiftCallback2 - Callback with 2 arguments
public final class SwiftCallback2: OrkidObject {
    private let closure: (Any, Any) -> Void
    private let callbackId: UInt64

    public init(closure: @escaping (Any, Any) -> Void) {
        self.closure = closure
        let handle = orkid_swiftcallback_create()!
        self.callbackId = orkid_swiftcallback_get_id(handle)
        super.init(handle: handle, owned: true)

        _ = SwiftCallbackManager.shared
        SwiftCallbackManager.shared.register(id: callbackId, callback: self)
    }

    deinit {
        SwiftCallbackManager.shared.unregister(id: callbackId)
    }

    internal func invoke(argsHandle: OpaquePointer?) {
        // Single-arg invoke not supported for SwiftCallback2
        fatalError("SwiftCallback2 requires invoke2arg, not invoke")
    }

    internal func invoke2arg(arg1Handle: OpaquePointer?, arg2Handle: OpaquePointer?) {
        guard let h1 = arg1Handle, let h2 = arg2Handle else { return }
        let arg1 = decodeCallbackArgument(handle: h1)
        let arg2 = decodeCallbackArgument(handle: h2)
        closure(arg1, arg2)
    }
}

extension SwiftCallback2: SwiftCallbackInvocable {}

/// SwiftCallback3 - Callback with 3 arguments
public final class SwiftCallback3: OrkidObject {
    private let closure: (Any, Any, Any) -> Void
    private let callbackId: UInt64

    public init(closure: @escaping (Any, Any, Any) -> Void) {
        self.closure = closure
        let handle = orkid_swiftcallback_create()!
        self.callbackId = orkid_swiftcallback_get_id(handle)
        super.init(handle: handle, owned: true)

        _ = SwiftCallbackManager.shared
        SwiftCallbackManager.shared.register(id: callbackId, callback: self)
    }

    deinit {
        SwiftCallbackManager.shared.unregister(id: callbackId)
    }

    internal func invoke(argsHandle: OpaquePointer?) {
        // Single-arg invoke not supported for SwiftCallback3
        fatalError("SwiftCallback3 requires invoke3arg, not invoke")
    }

    internal func invoke3arg(arg1Handle: OpaquePointer?, arg2Handle: OpaquePointer?, arg3Handle: OpaquePointer?) {
        guard let h1 = arg1Handle, let h2 = arg2Handle, let h3 = arg3Handle else { return }
        let arg1 = decodeCallbackArgument(handle: h1)
        let arg2 = decodeCallbackArgument(handle: h2)
        let arg3 = decodeCallbackArgument(handle: h3)
        closure(arg1, arg2, arg3)
    }
}

extension SwiftCallback3: SwiftCallbackInvocable {}
