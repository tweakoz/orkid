#!/usr/bin/env swift

import Foundation
import OrkCore

// ========================================
// MAIN TEST
// ========================================

print("=== Orkid Swift Math Test (vec3 OrkCore Module) ===\n")

// Initialize Orkid
print("Initializing Orkid...")
_ = Orkid.shared

if !Orkid.lastError.isEmpty {
    print("ERROR during init: \(Orkid.lastError)")
    exit(1)
}
print("Orkid initialized successfully\n")

// Test 1: Create a vec3
print("Test 1: Creating vec3(3, 4, 0)...")
let vec = vec3(3.0, 4.0, 0.0)
print("vec3 created successfully")
print("  Type: \(vec.typeName)")
print("  Use count: \(vec.useCount)")
print("  Description: \(vec)\n")

// Test 2: Get components
print("Test 2: Reading components...")
print("  x = \(vec.x)")
print("  y = \(vec.y)")
print("  z = \(vec.z)")

if vec.x == 3.0 && vec.y == 4.0 && vec.z == 0.0 {
    print("✓ Components correct\n")
} else {
    print("✗ Components incorrect!\n")
}

// Test 3: Calculate length (should be 5.0 for 3,4,0 triangle)
print("Test 3: Calculating length...")
let length = vec.length
print("  length = \(length)")

if abs(length - 5.0) < 0.001 {
    print("✓ Length correct (3-4-5 triangle)\n")
} else {
    print("✗ Length incorrect (expected 5.0, got \(length))\n")
}

// Test 4: Set components
print("Test 4: Modifying components...")
vec.x = 1.0
vec.y = 0.0
vec.z = 0.0

print("  After setting to (1, 0, 0):")
print("    Description: \(vec)")
print("    x = \(vec.x)")
print("    y = \(vec.y)")
print("    z = \(vec.z)")

if vec.x == 1.0 && vec.y == 0.0 && vec.z == 0.0 {
    print("✓ Setters work correctly\n")
} else {
    print("✗ Setters failed\n")
}

// Test 5: Normalized vector
print("Test 5: Creating normalized vector...")
let normalized = vec.normalized()
print("  Normalized: \(normalized)")
print("  Normalized length = \(normalized.length)")

if abs(normalized.length - 1.0) < 0.001 {
    print("✓ Normalized vector has unit length\n")
} else {
    print("✗ Normalized vector incorrect (expected 1.0, got \(normalized.length))\n")
}

// Test 6: Another length test with different vector
print("Test 6: Testing with different vector...")
let vec2 = vec3(1.0, 1.0, 1.0)
print("  \(vec2)")

let length2 = vec2.length
let expectedLength = Float(sqrt(3.0))
print("  length = \(length2)")
print("  Expected: \(expectedLength)")

if abs(length2 - expectedLength) < 0.001 {
    print("✓ Length calculation correct\n")
} else {
    print("✗ Length calculation incorrect\n")
}

// Test 7: Demonstrate automatic cleanup
print("Test 7: Automatic memory management...")
print("  vec use count: \(vec.useCount)")
print("  vec2 use count: \(vec2.useCount)")
print("  normalized use count: \(normalized.useCount)")
print("  All vectors will be automatically released via deinit\n")

// All vec3 objects automatically released when going out of scope

// Shutdown Orkid
print("Shutting down Orkid...")
Orkid.exit()
print("Orkid shutdown complete\n")

print("=== All Tests Complete ===")
