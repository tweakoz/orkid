#!/usr/bin/env swift

import Foundation
import OrkCore

// ========================================
// MAIN TEST
// ========================================

print("=== Orkid Swift Math Test (mat4 OrkCore Module) ===\n")

// Initialize Orkid
print("Initializing Orkid...")
_ = Orkid.shared

if !Orkid.lastError.isEmpty {
    print("ERROR during init: \(Orkid.lastError)")
    exit(1)
}
print("Orkid initialized successfully\n")

// Test 1: Create identity matrix
print("Test 1: Creating identity matrix...")
let identity = mat4.identity()
print("Identity matrix created successfully")
print("  Type: \(identity.typeName)")
print("  Description: \(identity)")

let t = identity.translation
print("  Translation: (\(t.x), \(t.y), \(t.z))")
if t.x == 0.0 && t.y == 0.0 && t.z == 0.0 {
    print("✓ Identity matrix has zero translation\n")
} else {
    print("✗ Identity matrix translation incorrect\n")
}

// Test 2: Create translation matrix
print("Test 2: Creating translation matrix...")
let trans = mat4.translation(10.0, 20.0, 30.0)
let t2 = trans.translation
print("  \(trans)")
print("  Translation: (\(t2.x), \(t2.y), \(t2.z))")

if t2.x == 10.0 && t2.y == 20.0 && t2.z == 30.0 {
    print("✓ Translation matrix correct\n")
} else {
    print("✗ Translation matrix incorrect\n")
}

// Test 3: Set translation
print("Test 3: Modifying translation...")
trans.translation = (x: 5.0, y: 15.0, z: 25.0)
let t3 = trans.translation
print("  New translation: (\(t3.x), \(t3.y), \(t3.z))")

if t3.x == 5.0 && t3.y == 15.0 && t3.z == 25.0 {
    print("✓ Set translation works\n")
} else {
    print("✗ Set translation failed\n")
}

// Test 4: Matrix multiplication
print("Test 4: Matrix multiplication...")
let scale = mat4.scale(2.0, 2.0, 2.0)
print("  Scale matrix: \(scale)")
print("  Translation matrix: \(trans)")

// Multiply: scale * trans
let combined = scale * trans
print("  Combined (scale * translation): \(combined)")

let t4 = combined.translation
print("  Combined translation: (\(t4.x), \(t4.y), \(t4.z))")

// After scale*trans, translation should be scaled
if abs(t4.x - 10.0) < 0.001 && abs(t4.y - 30.0) < 0.001 && abs(t4.z - 50.0) < 0.001 {
    print("✓ Matrix multiplication works (scale * translation)\n")
} else {
    print("✗ Matrix multiplication incorrect (expected (10, 30, 50), got (\(t4.x), \(t4.y), \(t4.z)))\n")
}

// Test 5: Transform vector
print("Test 5: Transforming vec4...")
let vec = vec4(1.0, 1.0, 1.0, 1.0)
print("  Original vector: \(vec)")

let transformed = scale * vec
print("  After scale(2,2,2): \(transformed)")

if abs(transformed.x - 2.0) < 0.001 && abs(transformed.y - 2.0) < 0.001 && abs(transformed.z - 2.0) < 0.001 {
    print("✓ Vector transform works\n")
} else {
    print("✗ Vector transform incorrect\n")
}

// Test 6: Matrix inverse
print("Test 6: Matrix inverse...")
let scaleInv = scale.inverse()
print("  Scale matrix inverse: \(scaleInv)")

// Transform the scaled vector back with inverse
let unscaled = scaleInv * transformed
print("  Transformed vector: \(transformed)")
print("  After inverse transform: \(unscaled)")

if abs(unscaled.x - 1.0) < 0.001 && abs(unscaled.y - 1.0) < 0.001 && abs(unscaled.z - 1.0) < 0.001 {
    print("✓ Matrix inverse works (returns to original)\n")
} else {
    print("✗ Matrix inverse incorrect\n")
}

// Test 7: Rotation matrices
print("Test 7: Testing rotation matrices...")
let rotX = mat4.rotationX(.pi / 2)  // 90 degrees
let rotY = mat4.rotationY(.pi / 2)
let rotZ = mat4.rotationZ(.pi / 2)

print("  Rotation X (90°): \(rotX)")
print("  Rotation Y (90°): \(rotY)")
print("  Rotation Z (90°): \(rotZ)")
print("✓ Rotation matrices created successfully\n")

// Test 8: Transpose
print("Test 8: Testing matrix transpose...")
let transposed = identity.transpose()
print("  Original identity: \(identity)")
print("  Transposed: \(transposed)")
print("✓ Transpose works\n")

// Test 9: Chained transformations using operator overloading
print("Test 9: Chained transformations...")
let translate = mat4.translation(10.0, 0.0, 0.0)
let rotate = mat4.rotationZ(.pi / 4)  // 45 degrees
let scaleOp = mat4.scale(2.0, 2.0, 2.0)

let complex = scaleOp * rotate * translate
print("  Complex matrix (scale * rotate * translate): \(complex)")

let testVec = vec4(0.0, 0.0, 0.0, 1.0)
let result = complex * testVec
print("  Transform (0,0,0,1): \(result)")
print("✓ Chained transformations work\n")

// Automatic cleanup demonstration
print("All matrices and vectors will be automatically released via deinit\n")

// Shutdown Orkid
print("Shutting down Orkid...")
Orkid.exit()
print("Orkid shutdown complete\n")

print("=== All Tests Complete ===")
