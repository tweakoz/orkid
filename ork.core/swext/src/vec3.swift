// vec3 - 3D floating point vector (facade over C++ fvec3)
import Foundation

/// Swift wrapper for ork::fvec3 - INSTANTIABLE from Swift
public final class vec3: OrkidObject {

    // MARK: - Public Initialization (Swift can create!)

    public init(_ x: Float = 0, _ y: Float = 0, _ z: Float = 0) {
        super.init(handle: orkid_fvec3_create(x, y, z)!)
    }

    // MARK: - Internal init for wrapping C++-returned handles
    internal override init(handle: OpaquePointer, owned: Bool = true) {
        super.init(handle: handle, owned: owned)
    }

    // MARK: - Properties

    public var x: Float {
        get { orkid_fvec3_get_x(handle) }
        set { orkid_fvec3_set_x(handle, newValue) }
    }

    public var y: Float {
        get { orkid_fvec3_get_y(handle) }
        set { orkid_fvec3_set_y(handle, newValue) }
    }

    public var z: Float {
        get { orkid_fvec3_get_z(handle) }
        set { orkid_fvec3_set_z(handle, newValue) }
    }

    // MARK: - Methods

    public var length: Float {
        return orkid_fvec3_length(handle)
    }

    public func normalized() -> vec3 {
        return vec3(handle: orkid_fvec3_normalized(handle)!)
    }
}

// MARK: - CustomStringConvertible
extension vec3: CustomStringConvertible {
    public var description: String {
        return "vec3(x: \(x), y: \(y), z: \(z))"
    }
}
