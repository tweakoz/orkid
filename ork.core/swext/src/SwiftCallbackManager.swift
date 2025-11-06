////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2025, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

import Foundation

/// Protocol for type-erased callback invocation
protocol SwiftCallbackInvocable: AnyObject {
    func invoke(argsHandle: OpaquePointer?)
}

/// Global registry for callback lookup by ID
class SwiftCallbackManager {
    static let shared = SwiftCallbackManager()

    private var callbacks: [UInt64: SwiftCallbackInvocable] = [:]
    private let lock = NSLock()

    private init() {
        // Register our Swift callback invoker with C++
        orkid_register_swift_callback_invoker(orkid_swift_invoke_callback)
    }

    func register(id: UInt64, callback: SwiftCallbackInvocable) {
        lock.lock()
        defer { lock.unlock() }
        callbacks[id] = callback
    }

    func unregister(id: UInt64) {
        lock.lock()
        defer { lock.unlock() }
        callbacks.removeValue(forKey: id)
    }

    func invoke(id: UInt64, argsHandle: OpaquePointer?) {
        lock.lock()
        let callback = callbacks[id]  // Creates strong reference via ARC
        lock.unlock()                  // Safe to unlock - callback stays alive

        // Invoke through protocol - callback object retained by local variable
        callback?.invoke(argsHandle: argsHandle)
    }
}

/// C bridge function - called from C++ to invoke Swift callbacks
@_cdecl("orkid_swift_invoke_callback")
func orkid_swift_invoke_callback(callback_id: UInt64, args: OpaquePointer?) {
    SwiftCallbackManager.shared.invoke(id: callback_id, argsHandle: args)
}
