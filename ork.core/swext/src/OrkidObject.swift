// OrkidObject - Base class for all Orkid objects using handle pattern
import Foundation

/// Base class for all Orkid objects (uses handle pattern with automatic cleanup)
open class OrkidObject {
    internal let handle: OpaquePointer
    private var ownsHandle: Bool

    internal init(handle: OpaquePointer, owned: Bool = true) {
        self.handle = handle
        self.ownsHandle = owned
    }

    deinit {
        if ownsHandle {
            orkid_handle_release(handle)
        }
    }

    /// Get the type name of the underlying C++ object
    public var typeName: String {
        if let cstr = orkid_handle_type_name(handle) {
            return String(cString: cstr)
        }
        return ""
    }

    /// Get the reference count of the underlying shared_ptr
    public var useCount: Int {
        return Int(orkid_handle_use_count(handle))
    }

    /// Access the underlying C++ handle for interop
    public var cppHandle: OpaquePointer {
        return handle
    }
}
