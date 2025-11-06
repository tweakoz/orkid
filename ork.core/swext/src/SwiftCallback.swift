////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2025, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

import Foundation

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
