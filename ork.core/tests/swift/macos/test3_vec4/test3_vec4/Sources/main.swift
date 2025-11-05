#!/usr/bin/env swift

import Foundation

// Import the C bridge (direct function calls)
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

@_silgen_name("orkid_handle_use_count")
func orkid_handle_use_count(_ handle: OrkidHandleBase) -> Int32

@_silgen_name("orkid_handle_type_name")
func orkid_handle_type_name(_ handle: OrkidHandleBase) -> UnsafePointer<CChar>?

// vec4 functions
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

// Helper to get last error as String
func getLastError() -> String {
    if let cstr = orkid_get_last_error() {
        return String(cString: cstr)
    }
    return ""
}

// Main test
print("=== Orkid Swift Math Test (vec4) ===\n")

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

// Test 1: Create a vec4
print("Test 1: Creating vec4(1, 2, 3, 4)...")
guard let vec = orkid_fvec4_create(1.0, 2.0, 3.0, 4.0) else {
    print("ERROR: Failed to create vec4: \(getLastError())")
    orkid_swift_exit()
    exit(1)
}
print("vec4 created successfully")
if let typeName = orkid_handle_type_name(vec) {
    print("  Type: \(String(cString: typeName))")
}
print("  Use count: \(orkid_handle_use_count(vec))\n")

// Test 2: Get components
print("Test 2: Reading components...")
let x = orkid_fvec4_get_x(vec)
let y = orkid_fvec4_get_y(vec)
let z = orkid_fvec4_get_z(vec)
let w = orkid_fvec4_get_w(vec)
print("  x = \(x)")
print("  y = \(y)")
print("  z = \(z)")
print("  w = \(w)")

if x == 1.0 && y == 2.0 && z == 3.0 && w == 4.0 {
    print("✓ Components correct\n")
} else {
    print("✗ Components incorrect!\n")
}

// Test 3: Calculate length
// NOTE: vec4.magnitude() only uses x,y,z (ignores w) - designed for homogeneous coordinates
print("Test 3: Calculating length...")
let length = orkid_fvec4_length(vec)
let expectedLength = Float(sqrt(1.0 + 4.0 + 9.0))  // sqrt(14) - w is ignored
print("  length = \(length)")
print("  Expected: \(expectedLength) (NOTE: w component ignored)")

if abs(length - expectedLength) < 0.001 {
    print("✓ Length correct (sqrt(x²+y²+z²) = sqrt(14))\n")
} else {
    print("✗ Length incorrect (expected \(expectedLength), got \(length))\n")
}

// Test 4: Set components
print("Test 4: Modifying components...")
orkid_fvec4_set_x(vec, 3.0)
orkid_fvec4_set_y(vec, 4.0)
orkid_fvec4_set_z(vec, 0.0)
orkid_fvec4_set_w(vec, 1.0)

let newX = orkid_fvec4_get_x(vec)
let newY = orkid_fvec4_get_y(vec)
let newZ = orkid_fvec4_get_z(vec)
let newW = orkid_fvec4_get_w(vec)
print("  After setting to (3, 4, 0, 1):")
print("    x = \(newX)")
print("    y = \(newY)")
print("    z = \(newZ)")
print("    w = \(newW)")

if newX == 3.0 && newY == 4.0 && newZ == 0.0 && newW == 1.0 {
    print("✓ Setters work correctly\n")
} else {
    print("✗ Setters failed\n")
}

// Test 5: Normalized vector
print("Test 5: Creating normalized vector...")
guard let normalized = orkid_fvec4_normalized(vec) else {
    print("ERROR: Failed to normalize: \(getLastError())")
    orkid_handle_release(vec)
    orkid_swift_exit()
    exit(1)
}

let normLength = orkid_fvec4_length(normalized)
print("  Normalized (3,4,0,1) length = \(normLength)")

if abs(normLength - 1.0) < 0.001 {
    print("✓ Normalized vector has unit length (3D: 3-4-5 triangle)\n")
} else {
    print("✗ Normalized vector incorrect (expected 1.0, got \(normLength))\n")
}

// Test 6: Dot product
print("Test 6: Testing dot product...")
guard let vecA = orkid_fvec4_create(1.0, 0.0, 0.0, 0.0) else {
    print("ERROR: Failed to create vecA")
    orkid_handle_release(normalized)
    orkid_handle_release(vec)
    orkid_swift_exit()
    exit(1)
}

guard let vecB = orkid_fvec4_create(0.0, 1.0, 0.0, 0.0) else {
    print("ERROR: Failed to create vecB")
    orkid_handle_release(vecA)
    orkid_handle_release(normalized)
    orkid_handle_release(vec)
    orkid_swift_exit()
    exit(1)
}

let dotOrthogonal = orkid_fvec4_dot(vecA, vecB)
print("  dot((1,0,0,0), (0,1,0,0)) = \(dotOrthogonal)")

if abs(dotOrthogonal - 0.0) < 0.001 {
    print("✓ Orthogonal vectors have zero dot product\n")
} else {
    print("✗ Dot product incorrect for orthogonal vectors\n")
}

// Test parallel vectors
guard let vecC = orkid_fvec4_create(2.0, 0.0, 0.0, 0.0) else {
    print("ERROR: Failed to create vecC")
    orkid_handle_release(vecB)
    orkid_handle_release(vecA)
    orkid_handle_release(normalized)
    orkid_handle_release(vec)
    orkid_swift_exit()
    exit(1)
}

let dotParallel = orkid_fvec4_dot(vecA, vecC)
print("  dot((1,0,0,0), (2,0,0,0)) = \(dotParallel)")

if abs(dotParallel - 2.0) < 0.001 {
    print("✓ Parallel vectors dot product correct\n")
} else {
    print("✗ Dot product incorrect for parallel vectors\n")
}

// Cleanup
print("Cleaning up...")
orkid_handle_release(vecC)
orkid_handle_release(vecB)
orkid_handle_release(vecA)
orkid_handle_release(normalized)
orkid_handle_release(vec)
print("All handles released\n")

// Shutdown Orkid
print("Shutting down Orkid...")
orkid_swift_exit()
print("Orkid shutdown complete\n")

print("=== All Tests Complete ===")
