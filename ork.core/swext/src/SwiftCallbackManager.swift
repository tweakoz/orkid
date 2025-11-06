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
        // Register our Swift callback invokers with C++
        orkid_register_swift_callback_invoker(orkid_swift_invoke_callback)
        orkid_register_swift_callback_invoker_2arg(orkid_swift_invoke_callback_2arg)
        orkid_register_swift_callback_invoker_3arg(orkid_swift_invoke_callback_3arg)
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

    func invoke2arg(id: UInt64, arg1Handle: OpaquePointer?, arg2Handle: OpaquePointer?) {
        lock.lock()
        let callback = callbacks[id]
        lock.unlock()

        // Downcast to SwiftCallback2
        if let callback2 = callback as? SwiftCallback2 {
            callback2.invoke2arg(arg1Handle: arg1Handle, arg2Handle: arg2Handle)
        }
    }

    func invoke3arg(id: UInt64, arg1Handle: OpaquePointer?, arg2Handle: OpaquePointer?, arg3Handle: OpaquePointer?) {
        lock.lock()
        let callback = callbacks[id]
        lock.unlock()

        // Downcast to SwiftCallback3
        if let callback3 = callback as? SwiftCallback3 {
            callback3.invoke3arg(arg1Handle: arg1Handle, arg2Handle: arg2Handle, arg3Handle: arg3Handle)
        }
    }
}

/// C bridge functions - called from C++ to invoke Swift callbacks
@_cdecl("orkid_swift_invoke_callback")
func orkid_swift_invoke_callback(callback_id: UInt64, args: OpaquePointer?) {
    SwiftCallbackManager.shared.invoke(id: callback_id, argsHandle: args)
}

@_cdecl("orkid_swift_invoke_callback_2arg")
func orkid_swift_invoke_callback_2arg(callback_id: UInt64, arg1: OpaquePointer?, arg2: OpaquePointer?) {
    SwiftCallbackManager.shared.invoke2arg(id: callback_id, arg1Handle: arg1, arg2Handle: arg2)
}

@_cdecl("orkid_swift_invoke_callback_3arg")
func orkid_swift_invoke_callback_3arg(callback_id: UInt64, arg1: OpaquePointer?, arg2: OpaquePointer?, arg3: OpaquePointer?) {
    SwiftCallbackManager.shared.invoke3arg(id: callback_id, arg1Handle: arg1, arg2Handle: arg2, arg3Handle: arg3)
}
