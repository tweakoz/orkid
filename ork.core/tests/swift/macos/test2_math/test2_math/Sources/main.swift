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

// vec3 functions
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

// Helper to get last error as String
func getLastError() -> String {
    if let cstr = orkid_get_last_error() {
        return String(cString: cstr)
    }
    return ""
}

// Main test
print("=== Orkid Swift Math Test (vec3) ===\n")

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

// Test 1: Create a vec3
print("Test 1: Creating vec3(3, 4, 0)...")
guard let vec = orkid_fvec3_create(3.0, 4.0, 0.0) else {
    print("ERROR: Failed to create vec3: \(getLastError())")
    orkid_swift_exit()
    exit(1)
}
print("vec3 created successfully")
if let typeName = orkid_handle_type_name(vec) {
    print("  Type: \(String(cString: typeName))")
}
print("  Use count: \(orkid_handle_use_count(vec))\n")

// Test 2: Get components
print("Test 2: Reading components...")
let x = orkid_fvec3_get_x(vec)
let y = orkid_fvec3_get_y(vec)
let z = orkid_fvec3_get_z(vec)
print("  x = \(x)")
print("  y = \(y)")
print("  z = \(z)")

if x == 3.0 && y == 4.0 && z == 0.0 {
    print("✓ Components correct\n")
} else {
    print("✗ Components incorrect!\n")
}

// Test 3: Calculate length (should be 5.0 for 3,4,0 triangle)
print("Test 3: Calculating length...")
let length = orkid_fvec3_length(vec)
print("  length = \(length)")

if abs(length - 5.0) < 0.001 {
    print("✓ Length correct (3-4-5 triangle)\n")
} else {
    print("✗ Length incorrect (expected 5.0, got \(length))\n")
}

// Test 4: Set components
print("Test 4: Modifying components...")
orkid_fvec3_set_x(vec, 1.0)
orkid_fvec3_set_y(vec, 0.0)
orkid_fvec3_set_z(vec, 0.0)

let newX = orkid_fvec3_get_x(vec)
let newY = orkid_fvec3_get_y(vec)
let newZ = orkid_fvec3_get_z(vec)
print("  After setting to (1, 0, 0):")
print("    x = \(newX)")
print("    y = \(newY)")
print("    z = \(newZ)")

if newX == 1.0 && newY == 0.0 && newZ == 0.0 {
    print("✓ Setters work correctly\n")
} else {
    print("✗ Setters failed\n")
}

// Test 5: Normalized vector
print("Test 5: Creating normalized vector...")
guard let normalized = orkid_fvec3_normalized(vec) else {
    print("ERROR: Failed to normalize: \(getLastError())")
    orkid_handle_release(vec)
    orkid_swift_exit()
    exit(1)
}

let normLength = orkid_fvec3_length(normalized)
print("  Normalized length = \(normLength)")

if abs(normLength - 1.0) < 0.001 {
    print("✓ Normalized vector has unit length\n")
} else {
    print("✗ Normalized vector incorrect (expected 1.0, got \(normLength))\n")
}

// Test 6: Another length test with different vector
print("Test 6: Testing with different vector...")
guard let vec2 = orkid_fvec3_create(1.0, 1.0, 1.0) else {
    print("ERROR: Failed to create vec2")
    orkid_handle_release(normalized)
    orkid_handle_release(vec)
    orkid_swift_exit()
    exit(1)
}

let length2 = orkid_fvec3_length(vec2)
let expectedLength = Float(sqrt(3.0))
print("  vec3(1, 1, 1) length = \(length2)")
print("  Expected: \(expectedLength)")

if abs(length2 - expectedLength) < 0.001 {
    print("✓ Length calculation correct\n")
} else {
    print("✗ Length calculation incorrect\n")
}

// Cleanup
print("Cleaning up...")
orkid_handle_release(vec2)
orkid_handle_release(normalized)
orkid_handle_release(vec)
print("All handles released\n")

// Shutdown Orkid
print("Shutting down Orkid...")
orkid_swift_exit()
print("Orkid shutdown complete\n")

print("=== All Tests Complete ===")
