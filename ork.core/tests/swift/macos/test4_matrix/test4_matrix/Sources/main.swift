#!/usr/bin/env swift

import Foundation

// Import the C bridge
typealias OrkidHandleBase = OpaquePointer

// C function declarations
@_silgen_name("orkid_swift_init")
func orkid_swift_init(_ argc: Int32, _ argv: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?)

@_silgen_name("orkid_swift_exit")
func orkid_swift_exit()

@_silgen_name("orkid_get_last_error")
func orkid_get_last_error() -> UnsafePointer<CChar>?

@_silgen_name("orkid_handle_release")
func orkid_handle_release(_ handle: OrkidHandleBase)

@_silgen_name("orkid_handle_type_name")
func orkid_handle_type_name(_ handle: OrkidHandleBase) -> UnsafePointer<CChar>?

// vec4 functions (for testing transform)
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

// mat4 functions
@_silgen_name("orkid_fmtx4_create_identity")
func orkid_fmtx4_create_identity() -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_create_translation")
func orkid_fmtx4_create_translation(_ x: Float, _ y: Float, _ z: Float) -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_create_scale")
func orkid_fmtx4_create_scale(_ x: Float, _ y: Float, _ z: Float) -> OrkidHandleBase?

@_silgen_name("orkid_fmtx4_create_rotation_x")
func orkid_fmtx4_create_rotation_x(_ radians: Float) -> OrkidHandleBase?

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

@_silgen_name("orkid_fmtx4_transform_vec4")
func orkid_fmtx4_transform_vec4(_ mtx: OrkidHandleBase, _ vec: OrkidHandleBase) -> OrkidHandleBase?

// Helper to get last error as String
func getLastError() -> String {
    if let cstr = orkid_get_last_error() {
        return String(cString: cstr)
    }
    return ""
}

// Main test
print("=== Orkid Swift Math Test (mat4) ===\n")

// Initialize Orkid
print("Initializing Orkid...")
var args = CommandLine.unsafeArgv
orkid_swift_init(CommandLine.argc, args)

let error = getLastError()
if !error.isEmpty {
    print("ERROR during init: \(error)")
    exit(1)
}
print("Orkid initialized successfully\n")

// Test 1: Create identity matrix
print("Test 1: Creating identity matrix...")
guard let identity = orkid_fmtx4_create_identity() else {
    print("ERROR: Failed to create identity matrix: \(getLastError())")
    orkid_swift_exit()
    exit(1)
}
print("Identity matrix created successfully")
if let typeName = orkid_handle_type_name(identity) {
    print("  Type: \(String(cString: typeName))")
}

var tx: Float = 0, ty: Float = 0, tz: Float = 0
orkid_fmtx4_get_translation(identity, &tx, &ty, &tz)
print("  Translation: (\(tx), \(ty), \(tz))")
if tx == 0.0 && ty == 0.0 && tz == 0.0 {
    print("✓ Identity matrix has zero translation\n")
} else {
    print("✗ Identity matrix translation incorrect\n")
}

// Test 2: Create translation matrix
print("Test 2: Creating translation matrix...")
guard let trans = orkid_fmtx4_create_translation(10.0, 20.0, 30.0) else {
    print("ERROR: Failed to create translation matrix")
    orkid_handle_release(identity)
    orkid_swift_exit()
    exit(1)
}

orkid_fmtx4_get_translation(trans, &tx, &ty, &tz)
print("  Translation: (\(tx), \(ty), \(tz))")
if tx == 10.0 && ty == 20.0 && tz == 30.0 {
    print("✓ Translation matrix correct\n")
} else {
    print("✗ Translation matrix incorrect\n")
}

// Test 3: Set translation
print("Test 3: Modifying translation...")
orkid_fmtx4_set_translation(trans, 5.0, 15.0, 25.0)
orkid_fmtx4_get_translation(trans, &tx, &ty, &tz)
print("  New translation: (\(tx), \(ty), \(tz))")
if tx == 5.0 && ty == 15.0 && tz == 25.0 {
    print("✓ Set translation works\n")
} else {
    print("✗ Set translation failed\n")
}

// Test 4: Matrix multiplication
print("Test 4: Matrix multiplication...")
guard let scale = orkid_fmtx4_create_scale(2.0, 2.0, 2.0) else {
    print("ERROR: Failed to create scale matrix")
    orkid_handle_release(trans)
    orkid_handle_release(identity)
    orkid_swift_exit()
    exit(1)
}

// Multiply: scale * trans
guard let combined = orkid_fmtx4_multiply(scale, trans) else {
    print("ERROR: Failed to multiply matrices")
    orkid_handle_release(scale)
    orkid_handle_release(trans)
    orkid_handle_release(identity)
    orkid_swift_exit()
    exit(1)
}

orkid_fmtx4_get_translation(combined, &tx, &ty, &tz)
print("  Combined matrix translation: (\(tx), \(ty), \(tz))")
// After scale*trans, translation should be scaled
if abs(tx - 10.0) < 0.001 && abs(ty - 30.0) < 0.001 && abs(tz - 50.0) < 0.001 {
    print("✓ Matrix multiplication works (scale * translation)\n")
} else {
    print("✗ Matrix multiplication incorrect (expected (10, 30, 50), got (\(tx), \(ty), \(tz)))\n")
}

// Test 5: Transform vector
print("Test 5: Transforming vec4...")
guard let vec = orkid_fvec4_create(1.0, 1.0, 1.0, 1.0) else {
    print("ERROR: Failed to create vec4")
    orkid_handle_release(combined)
    orkid_handle_release(scale)
    orkid_handle_release(trans)
    orkid_handle_release(identity)
    orkid_swift_exit()
    exit(1)
}

guard let transformed = orkid_fmtx4_transform_vec4(scale, vec) else {
    print("ERROR: Failed to transform vec4")
    orkid_handle_release(vec)
    orkid_handle_release(combined)
    orkid_handle_release(scale)
    orkid_handle_release(trans)
    orkid_handle_release(identity)
    orkid_swift_exit()
    exit(1)
}

let tx_x = orkid_fvec4_get_x(transformed)
let tx_y = orkid_fvec4_get_y(transformed)
let tx_z = orkid_fvec4_get_z(transformed)
let tx_w = orkid_fvec4_get_w(transformed)
print("  Original: (1, 1, 1, 1)")
print("  Transformed by scale(2,2,2): (\(tx_x), \(tx_y), \(tx_z), \(tx_w))")
if abs(tx_x - 2.0) < 0.001 && abs(tx_y - 2.0) < 0.001 && abs(tx_z - 2.0) < 0.001 {
    print("✓ Vector transform works\n")
} else {
    print("✗ Vector transform incorrect\n")
}

// Test 6: Matrix inverse
print("Test 6: Matrix inverse...")
guard let scaleInv = orkid_fmtx4_inverse(scale) else {
    print("ERROR: Failed to invert matrix")
    orkid_handle_release(transformed)
    orkid_handle_release(vec)
    orkid_handle_release(combined)
    orkid_handle_release(scale)
    orkid_handle_release(trans)
    orkid_handle_release(identity)
    orkid_swift_exit()
    exit(1)
}

// Transform the scaled vector back with inverse
guard let unscaled = orkid_fmtx4_transform_vec4(scaleInv, transformed) else {
    print("ERROR: Failed to transform with inverse")
    orkid_handle_release(scaleInv)
    orkid_handle_release(transformed)
    orkid_handle_release(vec)
    orkid_handle_release(combined)
    orkid_handle_release(scale)
    orkid_handle_release(trans)
    orkid_handle_release(identity)
    orkid_swift_exit()
    exit(1)
}

let us_x = orkid_fvec4_get_x(unscaled)
let us_y = orkid_fvec4_get_y(unscaled)
let us_z = orkid_fvec4_get_z(unscaled)
print("  After scale(2) then inverse: (\(us_x), \(us_y), \(us_z))")
if abs(us_x - 1.0) < 0.001 && abs(us_y - 1.0) < 0.001 && abs(us_z - 1.0) < 0.001 {
    print("✓ Matrix inverse works (returns to original)\n")
} else {
    print("✗ Matrix inverse incorrect\n")
}

// Cleanup
print("Cleaning up...")
orkid_handle_release(unscaled)
orkid_handle_release(scaleInv)
orkid_handle_release(transformed)
orkid_handle_release(vec)
orkid_handle_release(combined)
orkid_handle_release(scale)
orkid_handle_release(trans)
orkid_handle_release(identity)
print("All handles released\n")

// Shutdown Orkid
print("Shutting down Orkid...")
orkid_swift_exit()
print("Orkid shutdown complete\n")

print("=== All Tests Complete ===")
