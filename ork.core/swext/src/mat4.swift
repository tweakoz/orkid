// mat4 - 4x4 floating point matrix (facade over C++ fmtx4)
import Foundation

/// Swift wrapper for ork::fmtx4 - INSTANTIABLE from Swift
public final class mat4: OrkidObject {

    // MARK: - Public Initialization (Swift can create!)

    public init() {
        super.init(handle: orkid_fmtx4_create_identity()!)
    }

    public static func identity() -> mat4 {
        return mat4()
    }

    public static func translation(x: Float, y: Float, z: Float) -> mat4 {
        return mat4(handle: orkid_fmtx4_create_translation(x, y, z)!)
    }

    public static func scale(x: Float, y: Float, z: Float) -> mat4 {
        return mat4(handle: orkid_fmtx4_create_scale(x, y, z)!)
    }

    public static func rotationX(_ radians: Float) -> mat4 {
        return mat4(handle: orkid_fmtx4_create_rotation_x(radians)!)
    }

    public static func rotationY(_ radians: Float) -> mat4 {
        return mat4(handle: orkid_fmtx4_create_rotation_y(radians)!)
    }

    public static func rotationZ(_ radians: Float) -> mat4 {
        return mat4(handle: orkid_fmtx4_create_rotation_z(radians)!)
    }

    // MARK: - Internal init for wrapping C++-returned handles
    internal override init(handle: OpaquePointer, owned: Bool = true) {
        super.init(handle: handle, owned: owned)
    }

    // MARK: - Translation

    public var translation: (x: Float, y: Float, z: Float) {
        get {
            var x: Float = 0, y: Float = 0, z: Float = 0
            orkid_fmtx4_get_translation(handle, &x, &y, &z)
            return (x, y, z)
        }
        set {
            orkid_fmtx4_set_translation(handle, newValue.x, newValue.y, newValue.z)
        }
    }

    // MARK: - Operations

    public func multiply(_ other: mat4) -> mat4 {
        return mat4(handle: orkid_fmtx4_multiply(handle, other.handle)!)
    }

    public func inverse() -> mat4 {
        return mat4(handle: orkid_fmtx4_inverse(handle)!)
    }

    public func transpose() -> mat4 {
        return mat4(handle: orkid_fmtx4_transpose(handle)!)
    }

    public func transform(_ vec: vec4) -> vec4 {
        return vec4(handle: orkid_fmtx4_transform_vec4(handle, vec.handle)!)
    }

    // MARK: - Operators

    public static func * (lhs: mat4, rhs: mat4) -> mat4 {
        return lhs.multiply(rhs)
    }

    public static func * (lhs: mat4, rhs: vec4) -> vec4 {
        return lhs.transform(rhs)
    }
}

// MARK: - CustomStringConvertible
extension mat4: CustomStringConvertible {
    public var description: String {
        let t = translation
        return "mat4(translation: (\(t.x), \(t.y), \(t.z)))"
    }
}
