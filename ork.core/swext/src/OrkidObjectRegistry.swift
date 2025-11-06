// OrkidObjectRegistry - Registry-based factory for creating correct Swift wrapper classes
import Foundation

/// Registry for creating the correct Swift wrapper class based on C++ type name
/// Similar to C++ SwiftCodecImpl with encoder/decoder maps
class OrkidObjectRegistry {
    typealias FactoryFunction = (OpaquePointer, Bool) -> OrkidObject

    private var factories: [String: FactoryFunction] = [:]

    static let shared = OrkidObjectRegistry()

    private init() {
        // Register all known types
        registerAllTypes()
    }

    /// Register a factory function for a C++ type name
    func register(typeName: String, factory: @escaping FactoryFunction) {
        factories[typeName] = factory
    }

    /// Create the appropriate Swift wrapper class for a handle
    /// Always creates a shared reference (increments refcount)
    func wrap(handle: OpaquePointer) -> OrkidObject {
        // Create a new handle sharing the same std::shared_ptr (increments refcount)
        let sharedHandle = orkid_handle_retain(handle)

        guard let cstr = orkid_handle_type_name(sharedHandle) else {
            return OrkidObject(handle: sharedHandle, owned: true)
        }

        let typeName = String(cString: cstr)

        if let factory = factories[typeName] {
            return factory(sharedHandle, true)  // Always owned: true (we retained it)
        }

        // Unknown type - return base OrkidObject
        return OrkidObject(handle: sharedHandle, owned: true)
    }

    /// Register all known Orkid types (called during init)
    private func registerAllTypes() {
        register(typeName: "ork::fvec3") { handle, owned in
            return vec3(handle: handle, owned: owned)
        }
        register(typeName: "ork::fvec4") { handle, owned in
            return vec4(handle: handle, owned: owned)
        }
        register(typeName: "ork::fmtx4") { handle, owned in
            return mat4(handle: handle, owned: owned)
        }
        register(typeName: "ork::Timer") { handle, owned in
            return Timer(handle: handle, owned: owned)
        }
        register(typeName: "ork::varmap::VarMap") { handle, owned in
            return VarMap(handle: handle, owned: owned)
        }
    }
}
