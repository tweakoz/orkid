#!/usr/bin/env swift

import Foundation
import OrkCore

// ========================================
// MAIN TEST
// ========================================

print("=== Orkid Swift Math Test (vec4 OrkCore Module) ===\n")

// Initialize Orkid
print("Initializing Orkid...")
OrkCore.initialize()

if !OrkCore.lastError.isEmpty {
    print("ERROR during init: \(OrkCore.lastError)")
    exit(1)
}
print("Orkid initialized successfully\n")

// Test 1: Create a vec4
print("Test 1: Creating vec4(1, 2, 3, 4)...")
let vec = vec4(1.0, 2.0, 3.0, 4.0)
print("vec4 created successfully")
print("  Type: \(vec.typeName)")
print("  Use count: \(vec.useCount)")
print("  Description: \(vec)\n")

// Test 2: Get components
print("Test 2: Reading components...")
print("  \(vec)")
if vec.x == 1.0 && vec.y == 2.0 && vec.z == 3.0 && vec.w == 4.0 {
    print("✓ Components correct\n")
} else {
    print("✗ Components incorrect!\n")
}

// Test 3: Calculate length
print("Test 3: Calculating length...")
let length = vec.length
let sum = 1.0*1.0 + 2.0*2.0 + 3.0*3.0 + 4.0*4.0
let expectedLength = Float(sqrt(sum))
print("  length = \(length)")
print("  expected = \(expectedLength)")

if abs(length - expectedLength) < 0.001 {
    print("✓ Length correct\n")
} else {
    print("✗ Length incorrect\n")
}

// Test 4: Set components
print("Test 4: Modifying components...")
vec.x = 10.0
vec.y = 20.0
vec.z = 30.0
vec.w = 40.0

print("  After modification: \(vec)")
if vec.x == 10.0 && vec.y == 20.0 && vec.z == 30.0 && vec.w == 40.0 {
    print("✓ Setters work correctly\n")
} else {
    print("✗ Setters failed\n")
}

// Test 5: Normalized vector
print("Test 5: Creating normalized vector...")
vec.x = 1.0
vec.y = 0.0
vec.z = 0.0
vec.w = 0.0

let normalized = vec.normalized()
print("  Original: \(vec)")
print("  Normalized: \(normalized)")
print("  Normalized length = \(normalized.length)")

if abs(normalized.length - 1.0) < 0.001 {
    print("✓ Normalized vector has unit length\n")
} else {
    print("✗ Normalized vector incorrect\n")
}

// Test 6: Dot product
print("Test 6: Testing dot product...")
let a = vec4(1.0, 0.0, 0.0, 0.0)
let b = vec4(0.0, 1.0, 0.0, 0.0)
let dotAB = a.dot(b)
print("  \(a) · \(b) = \(dotAB)")

if abs(dotAB) < 0.001 {
    print("✓ Dot product correct (perpendicular vectors)\n")
} else {
    print("✗ Dot product incorrect (expected 0)\n")
}

let c = vec4(1.0, 2.0, 3.0, 4.0)
let d = vec4(2.0, 3.0, 4.0, 5.0)
let dotCD = c.dot(d)
let expectedDot: Float = 40.0  // 1*2 + 2*3 + 3*4 + 4*5
print("  \(c) · \(d) = \(dotCD)")
print("  Expected: \(expectedDot)")

if abs(dotCD - expectedDot) < 0.001 {
    print("✓ Dot product calculation correct\n")
} else {
    print("✗ Dot product calculation incorrect\n")
}

// Test 7: vec3 conversion
print("Test 7: Testing xyz property (vec4 → vec3)...")
let vec4Value = vec4(5.0, 6.0, 7.0, 8.0)
let vec3Value = vec4Value.xyz
print("  vec4: \(vec4Value)")
print("  xyz (vec3): \(vec3Value)")

if vec3Value.x == 5.0 && vec3Value.y == 6.0 && vec3Value.z == 7.0 {
    print("✓ xyz property works correctly\n")
} else {
    print("✗ xyz property failed\n")
}

// Test 8: Create vec4 from vec3
print("Test 8: Creating vec4 from vec3...")
let vec3Base = vec3(1.0, 2.0, 3.0)
let vec4Fromvec3 = vec4(vec3Base, 10.0)
print("  vec3: \(vec3Base)")
print("  vec4: \(vec4Fromvec3)")

if vec4Fromvec3.x == 1.0 && vec4Fromvec3.y == 2.0 && vec4Fromvec3.z == 3.0 && vec4Fromvec3.w == 10.0 {
    print("✓ vec4 from vec3 works correctly\n")
} else {
    print("✗ vec4 from vec3 failed\n")
}

// Automatic cleanup demonstration
print("All vectors will be automatically released via deinit\n")

// Shutdown Orkid
print("Shutting down Orkid...")
OrkCore.exit()
print("Orkid shutdown complete\n")

print("=== All Tests Complete ===")
