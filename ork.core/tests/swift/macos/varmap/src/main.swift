#!/usr/bin/env swift

import Foundation
import OrkCore

// ========================================
// MAIN TEST
// ========================================

print("=== Orkid Swift VarMap Test (OrkCore Module) ===\n")

// Initialize Orkid
print("Initializing Orkid...")
OrkCore.initialize()

if !OrkCore.lastError.isEmpty {
    print("ERROR during init: \(OrkCore.lastError)")
    exit(1)
}
print("Orkid initialized successfully\n")

// Test 1: Create empty VarMap
print("Test 1: Creating empty VarMap...")
let vmap = VarMap()
print("VarMap created successfully")
print("  Type: \(vmap.typeName)")
print("  Count: \(vmap.count)")
print("  Is empty: \(vmap.isEmpty)")

if vmap.isEmpty {
    print("✓ VarMap is empty\n")
} else {
    print("✗ VarMap should be empty\n")
}

// Test 2: Store and retrieve Timer
print("Test 2: Storing Timer in VarMap...")
let timer = OrkCore.Timer()
timer.start()

vmap["myTimer"] = timer
print("  Stored timer with key 'myTimer'")
print("  Contains 'myTimer': \(vmap.contains("myTimer"))")
print("  Count: \(vmap.count)")

if vmap.contains("myTimer") && vmap.count == 1 {
    print("✓ Timer stored successfully\n")
} else {
    print("✗ Timer not stored correctly\n")
}

// Test 3: Retrieve Timer
print("Test 3: Retrieving Timer from VarMap...")
if let retrieved = vmap["myTimer"] as? OrkidObject {
    print("  Retrieved object type: \(retrieved.typeName)")
    if retrieved.typeName == "ork::Timer" {
        print("✓ Timer retrieved successfully\n")
    } else {
        print("✗ Wrong type retrieved\n")
    }
} else {
    print("✗ Failed to retrieve timer\n")
}

// Test 4: Store and retrieve vec3
print("Test 4: Storing vec3 in VarMap...")
let position = vec3(10.0, 20.0, 30.0)
vmap["position"] = position
print("  Stored vec3 with key 'position'")
print("  Count: \(vmap.count)")

if vmap.contains("position") && vmap.count == 2 {
    print("✓ vec3 stored successfully\n")
} else {
    print("✗ vec3 not stored correctly\n")
}

// Test 5: Retrieve vec3 (note: returns OrkidObject, would need casting in production)
print("Test 5: Retrieving vec3 from VarMap...")
if let retrieved = vmap["position"] as? OrkidObject {
    print("  Retrieved object type: \(retrieved.typeName)")
    if retrieved.typeName == "ork::fvec3" {
        print("✓ vec3 retrieved successfully\n")
    } else {
        print("✗ Wrong type retrieved\n")
    }
} else {
    print("✗ Failed to retrieve vec3\n")
}

// Test 6: Store vec4 and mat4
print("Test 6: Storing vec4 and mat4...")
vmap["direction"] = vec4(0, 1, 0, 0)
vmap["transform"] = mat4.identity()
print("  Count: \(vmap.count)")

if vmap.count == 4 {
    print("✓ Multiple types stored\n")
} else {
    print("✗ Count incorrect (expected 4, got \(vmap.count))\n")
}

// Test 7: Keys iteration
print("Test 7: Listing all keys...")
let allKeys = vmap.keys.sorted()
print("  Keys: \(allKeys)")

if allKeys == ["direction", "myTimer", "position", "transform"] {
    print("✓ Keys correct\n")
} else {
    print("✗ Keys incorrect\n")
}

// Test 8: Remove key
print("Test 8: Removing 'myTimer'...")
vmap.remove("myTimer")
print("  Contains 'myTimer': \(vmap.contains("myTimer"))")
print("  Count: \(vmap.count)")

if !vmap.contains("myTimer") && vmap.count == 3 {
    print("✓ Key removed successfully\n")
} else {
    print("✗ Key removal failed\n")
}

// Test 9: Subscript removal (nil assignment)
print("Test 9: Removing 'position' via subscript...")
vmap["position"] = nil
print("  Contains 'position': \(vmap.contains("position"))")
print("  Count: \(vmap.count)")

if !vmap.contains("position") && vmap.count == 2 {
    print("✓ Subscript removal works\n")
} else {
    print("✗ Subscript removal failed\n")
}

// Test 10: Clear all
print("Test 10: Clearing VarMap...")
vmap.clear()
print("  Count: \(vmap.count)")
print("  Is empty: \(vmap.isEmpty)")

if vmap.isEmpty {
    print("✓ VarMap cleared\n")
} else {
    print("✗ VarMap not cleared\n")
}

// Test 11: Clone
print("Test 11: Cloning VarMap...")
vmap["a"] = vec3(1, 2, 3)
vmap["b"] = vec3(4, 5, 6)
let cloned = vmap.clone()
print("  Original count: \(vmap.count)")
print("  Clone count: \(cloned.count)")
print("  Clone contains 'a': \(cloned.contains("a"))")

if cloned.count == vmap.count && cloned.contains("a") && cloned.contains("b") {
    print("✓ Clone works\n")
} else {
    print("✗ Clone failed\n")
}

// Test 12: For-in iteration
print("Test 12: For-in iteration...")
print("  Iterating over VarMap:")
for (key, value) in vmap {
    if let obj = value as? OrkidObject {
        print("    \(key): \(obj.typeName)")
    } else {
        print("    \(key): nil")
    }
}
print("✓ For-in iteration works\n")

// Automatic cleanup
print("All objects will be automatically released via deinit\n")

// Shutdown Orkid
print("Shutting down Orkid...")
OrkCore.exit()
print("Orkid shutdown complete\n")

print("=== All Tests Complete ===")
