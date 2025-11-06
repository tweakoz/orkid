// vec4 - 4D floating point vector (facade over C++ fvec4)
import Foundation

/// Swift wrapper for ork::fvec4 - INSTANTIABLE from Swift
public final class vec4: OrkidObject {

    // MARK: - Public Initialization (Swift can create!)

    public init(x: Float = 0, y: Float = 0, z: Float = 0, w: Float = 0) {
        super.init(handle: orkid_fvec4_create(x, y, z, w)!)
    }

    public init(_ vec3: vec3, w: Float = 0) {
        super.init(handle: orkid_fvec4_create(vec3.x, vec3.y, vec3.z, w)!)
    }

    // MARK: - Internal init for wrapping C++-returned handles
    internal override init(handle: OpaquePointer, owned: Bool = true) {
        super.init(handle: handle, owned: owned)
    }

    // MARK: - Properties

    public var x: Float {
        get { orkid_fvec4_get_x(handle) }
        set { orkid_fvec4_set_x(handle, newValue) }
    }

    public var y: Float {
        get { orkid_fvec4_get_y(handle) }
        set { orkid_fvec4_set_y(handle, newValue) }
    }

    public var z: Float {
        get { orkid_fvec4_get_z(handle) }
        set { orkid_fvec4_set_z(handle, newValue) }
    }

    public var w: Float {
        get { orkid_fvec4_get_w(handle) }
        set { orkid_fvec4_set_w(handle, newValue) }
    }

    // MARK: - Methods

    public var length: Float {
        return orkid_fvec4_length(handle)
    }

    public func normalized() -> vec4 {
        return vec4(handle: orkid_fvec4_normalized(handle)!)
    }

    public func dot(_ other: vec4) -> Float {
        return orkid_fvec4_dot(handle, other.handle)
    }

    /// Convert to vec3 (drops w component)
    public var xyz: vec3 {
        return vec3(x: x, y: y, z: z)
    }
}

// MARK: - CustomStringConvertible
extension vec4: CustomStringConvertible {
    public var description: String {
        return "vec4(x: \(x), y: \(y), z: \(z), w: \(w))"
    }
}
