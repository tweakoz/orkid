// C Bridge imports - low-level C functions
import Foundation

// Opaque handle type
typealias OrkidHandleBase = OpaquePointer

// Core lifecycle
@_silgen_name("orkid_swift_init")
func orkid_swift_init(_ argc: Int32, _ argv: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?)

@_silgen_name("orkid_swift_exit")
func orkid_swift_exit()

@_silgen_name("orkid_get_last_error")
func orkid_get_last_error() -> UnsafePointer<CChar>?

// Handle management
@_silgen_name("orkid_handle_release")
func orkid_handle_release(_ handle: OrkidHandleBase)

@_silgen_name("orkid_handle_use_count")
func orkid_handle_use_count(_ handle: OrkidHandleBase) -> Int32

@_silgen_name("orkid_handle_type_name")
func orkid_handle_type_name(_ handle: OrkidHandleBase) -> UnsafePointer<CChar>?

// Timer
@_silgen_name("orkid_timer_create")
func orkid_timer_create() -> OrkidHandleBase?

@_silgen_name("orkid_timer_start")
func orkid_timer_start(_ timer: OrkidHandleBase)

@_silgen_name("orkid_timer_end")
func orkid_timer_end(_ timer: OrkidHandleBase)

@_silgen_name("orkid_timer_secs_since_start")
func orkid_timer_secs_since_start(_ timer: OrkidHandleBase) -> Float

@_silgen_name("orkid_timer_get_sync_time")
func orkid_timer_get_sync_time() -> Float

// vec3
@_silgen_name("orkid_fvec3_create")
func orkid_fvec3_create(_ x: Float, _ y: Float, _ z: Float) -> OrkidHandleBase?

@_silgen_name("orkid_fvec3_get_x")
func orkid_fvec3_get_x(_ handle: OrkidHandleBase) -> Float

@_silgen_name("orkid_fvec3_get_y")
func orkid_fvec3_get_y(_ handle: OrkidHandleBase) -> Float

@_silgen_name("orkid_fvec3_get_z")
func orkid_fvec3_get_z(_ handle: OrkidHandleBase) -> Float

@_silgen_name("orkid_fvec3_set_x")
func orkid_fvec3_set_x(_ handle: OrkidHandleBase, _ value: Float)

@_silgen_name("orkid_fvec3_set_y")
func orkid_fvec3_set_y(_ handle: OrkidHandleBase, _ value: Float)

@_silgen_name("orkid_fvec3_set_z")
func orkid_fvec3_set_z(_ handle: OrkidHandleBase, _ value: Float)

@_silgen_name("orkid_fvec3_length")
func orkid_fvec3_length(_ handle: OrkidHandleBase) -> Float

@_silgen_name("orkid_fvec3_normalized")
func orkid_fvec3_normalized(_ handle: OrkidHandleBase) -> OrkidHandleBase?

// vec4
@_silgen_name("orkid_fvec4_create")
func orkid_fvec4_create(_ x: Float, _ y: Float, _ z: Float, _ w: Float) -> OrkidHandleBase?

@_silgen_name("orkid_fvec4_get_x")
func orkid_fvec4_get_x(_ handle: OrkidHandleBase) -> Float

@_silgen_name("orkid_fvec4_get_y")
func orkid_fvec4_get_y(_ handle: OrkidHandleBase) -> Float

@_silgen_name("orkid_fvec4_get_z")
func orkid_fvec4_get_z(_ handle: OrkidHandleBase) -> Float

@_silgen_name("orkid_fvec4_get_w")
func orkid_fvec4_get_w(_ handle: OrkidHandleBase) -> Float

@_silgen_name("orkid_fvec4_set_x")
func orkid_fvec4_set_x(_ handle: OrkidHandleBase, _ value: Float)

@_silgen_name("orkid_fvec4_set_y")
func orkid_fvec4_set_y(_ handle: OrkidHandleBase, _ value: Float)

@_silgen_name("orkid_fvec4_set_z")
func orkid_fvec4_set_z(_ handle: OrkidHandleBase, _ value: Float)

@_silgen_name("orkid_fvec4_set_w")
func orkid_fvec4_set_w(_ handle: OrkidHandleBase, _ value: Float)

@_silgen_name("orkid_fvec4_length")
func orkid_fvec4_length(_ handle: OrkidHandleBase) -> Float

@_silgen_name("orkid_fvec4_normalized")
func orkid_fvec4_normalized(_ handle: OrkidHandleBase) -> OrkidHandleBase?

@_silgen_name("orkid_fvec4_dot")
func orkid_fvec4_dot(_ a: OrkidHandleBase, _ b: OrkidHandleBase) -> Float

// mat4
@_silgen_name("orkid_fmtx4_create_identity")
func orkid_fmtx4_create_identity() -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_create_translation")
func orkid_fmtx4_create_translation(_ x: Float, _ y: Float, _ z: Float) -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_create_scale")
func orkid_fmtx4_create_scale(_ x: Float, _ y: Float, _ z: Float) -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_create_rotation_x")
func orkid_fmtx4_create_rotation_x(_ radians: Float) -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_create_rotation_y")
func orkid_fmtx4_create_rotation_y(_ radians: Float) -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_create_rotation_z")
func orkid_fmtx4_create_rotation_z(_ radians: Float) -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_get_translation")
func orkid_fmtx4_get_translation(_ handle: OrkidHandleBase, _ out_x: UnsafeMutablePointer<Float>?, _ out_y: UnsafeMutablePointer<Float>?, _ out_z: UnsafeMutablePointer<Float>?)

@_silgen_name("orkid_fmtx4_set_translation")
func orkid_fmtx4_set_translation(_ handle: OrkidHandleBase, _ x: Float, _ y: Float, _ z: Float)

@_silgen_name("orkid_fmtx4_multiply")
func orkid_fmtx4_multiply(_ a: OrkidHandleBase, _ b: OrkidHandleBase) -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_inverse")
func orkid_fmtx4_inverse(_ handle: OrkidHandleBase) -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_transpose")
func orkid_fmtx4_transpose(_ handle: OrkidHandleBase) -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_transform_vec4")
func orkid_fmtx4_transform_vec4(_ mtx: OrkidHandleBase, _ vec: OrkidHandleBase) -> OrkidHandleBase?

// VarMap
@_silgen_name("orkid_varmap_create")
func orkid_varmap_create() -> OrkidHandleBase?

@_silgen_name("orkid_varmap_get")
func orkid_varmap_get(_ vmap: OrkidHandleBase, _ key: UnsafePointer<CChar>) -> OrkidHandleBase?

@_silgen_name("orkid_varmap_set")
func orkid_varmap_set(_ vmap: OrkidHandleBase, _ key: UnsafePointer<CChar>, _ value: OrkidHandleBase)

@_silgen_name("orkid_varmap_remove")
func orkid_varmap_remove(_ vmap: OrkidHandleBase, _ key: UnsafePointer<CChar>)

@_silgen_name("orkid_varmap_contains")
func orkid_varmap_contains(_ vmap: OrkidHandleBase, _ key: UnsafePointer<CChar>) -> Bool

@_silgen_name("orkid_varmap_keys")
func orkid_varmap_keys(_ vmap: OrkidHandleBase, _ out_count: UnsafeMutablePointer<Int32>) -> UnsafePointer<UnsafePointer<CChar>?>?

@_silgen_name("orkid_varmap_free_keys")
func orkid_varmap_free_keys(_ keys: UnsafePointer<UnsafePointer<CChar>?>, _ count: Int32)

@_silgen_name("orkid_varmap_size")
func orkid_varmap_size(_ vmap: OrkidHandleBase) -> Int32

@_silgen_name("orkid_varmap_clear")
func orkid_varmap_clear(_ vmap: OrkidHandleBase)

@_silgen_name("orkid_varmap_clone")
func orkid_varmap_clone(_ vmap: OrkidHandleBase) -> OrkidHandleBase?

@_silgen_name("orkid_varmap_invoke_callback")
func orkid_varmap_invoke_callback(_ vmap: OrkidHandleBase, _ key: UnsafePointer<CChar>)

// SwiftCallback
@_silgen_name("orkid_swiftcallback_create")
func orkid_swiftcallback_create() -> OrkidHandleBase?

@_silgen_name("orkid_swiftcallback_get_id")
func orkid_swiftcallback_get_id(_ handle: OrkidHandleBase) -> UInt64

@_silgen_name("orkid_register_swift_callback_invoker")
func orkid_register_swift_callback_invoker(_ invoker: @convention(c) (UInt64, OpaquePointer?) -> Void)

// Helper to get last error
func getLastError() -> String {
    if let cstr = orkid_get_last_error() {
        return String(cString: cstr)
    }
    return ""
}
