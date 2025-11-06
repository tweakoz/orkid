import Foundation
import OrkCore

////////////////////////////////////////////////////////////////
// Test: SwiftCallback (no args)
// Phase 8: Simple Callbacks - Full Round-Trip Test
//
// Tests Swift → C++ storage → C++ retrieval → Swift invocation:
// 1. Create SwiftCallback in Swift
// 2. Store in VarMap (C++ storage)
// 3. Invoke via VarMap.invokeNamedCallback() (C++ → Swift)
// 4. Verify closure executed
////////////////////////////////////////////////////////////////

print("=== SwiftCallback Round-Trip Tests ===\n")

// Initialize Orkid (required for all tests)
OrkCore.initialize()

// Test 1: Basic callback invocation via VarMap
print("Test 1: Basic callback invocation")
let vmap = VarMap()
var callbackInvoked = false
let callback = SwiftCallback {
    callbackInvoked = true
}
vmap["test_callback"] = callback
vmap.invokeNamedCallback("test_callback")

if callbackInvoked {
    print("✓ Callback invoked successfully via VarMap")
} else {
    print("✗ Callback failed to invoke")
}

// Test 2: Multiple callbacks with different keys
print("\nTest 2: Multiple callbacks")
var count1 = 0
var count2 = 0
let callback1 = SwiftCallback {
    count1 += 1
}
let callback2 = SwiftCallback {
    count2 += 1
}
vmap["callback1"] = callback1
vmap["callback2"] = callback2

vmap.invokeNamedCallback("callback1")
vmap.invokeNamedCallback("callback2")
vmap.invokeNamedCallback("callback1")

if count1 == 2 && count2 == 1 {
    print("✓ Multiple callbacks work independently")
    print("  callback1 invoked \(count1) times")
    print("  callback2 invoked \(count2) times")
} else {
    print("✗ Multiple callbacks failed (count1=\(count1), count2=\(count2))")
}

// Test 3: Callback with captured variables
print("\nTest 3: Callback with captured variables")
var capturedValue = 42
let capturingCallback = SwiftCallback {
    capturedValue += 10
}
vmap["capturing"] = capturingCallback
vmap.invokeNamedCallback("capturing")

if capturedValue == 52 {
    print("✓ Callback captured and modified variable (value=\(capturedValue))")
} else {
    print("✗ Callback capture failed (value=\(capturedValue))")
}

// Test 4: Verify callback persists in VarMap
print("\nTest 4: Callback persistence")
var persistTest = false
do {
    let tempCallback = SwiftCallback {
        persistTest = true
    }
    vmap["persist"] = tempCallback
    // tempCallback local var goes out of scope, but VarMap holds reference
}
vmap.invokeNamedCallback("persist")
if persistTest {
    print("✓ Callback persisted in VarMap after local scope exit")
} else {
    print("✗ Callback lost after scope exit")
}

// Test 5: Callback cleanup when removed from VarMap
print("\nTest 5: Callback cleanup")
var cleanupTest = 0
let cleanupCallback = SwiftCallback {
    cleanupTest += 1
}
vmap["cleanup"] = cleanupCallback
vmap.invokeNamedCallback("cleanup")
print("  Invoked once: cleanupTest=\(cleanupTest)")

// Remove from VarMap - callback should be unregistered
vmap.remove("cleanup")
// Note: Can't easily test that it's NOT invoked without error handling

print("✓ Callback removed from VarMap")

// Test 6: Type verification
print("\nTest 6: Type verification")
let typedCallback = SwiftCallback {
    print("Typed callback")
}
vmap["typed"] = typedCallback
if let retrieved = vmap["typed"] as? OrkidObject {
    if retrieved.typeName.contains("SwiftCallbackHolder") {
        print("✓ Callback has correct type: \(retrieved.typeName)")
    } else {
        print("✗ Unexpected type: \(retrieved.typeName)")
    }
} else {
    print("✗ Failed to retrieve callback from VarMap")
}

// Test 7: Raw closure syntax (auto-wrapping)
print("\nTest 7: Raw closure syntax")
var rawClosureInvoked = false
vmap["rawClosure"] = {
    rawClosureInvoked = true
}
vmap.invokeNamedCallback("rawClosure")

if rawClosureInvoked {
    print("✓ Raw closure auto-wrapped and invoked successfully")
} else {
    print("✗ Raw closure failed to invoke")
}

// Test 8: Callback with typed argument (Int) -> Void
print("\nTest 8: Callback with typed argument (Int) -> Void")
var receivedInt: Int?
vmap["withInt"] = { (val: Int) in  // Clean typed syntax!
    print("  Received Int: \(val)")
    receivedInt = val
}
vmap.invokeNamedCallback("withInt", 123)

if receivedInt == 123 {
    print("✓ Callback received correct int value (123)")
} else {
    print("✗ Callback with int failed (got \(receivedInt ?? 0))")
}

// Test 9: Callback with typed argument (String) -> Void
print("\nTest 9: Callback with typed argument (String) -> Void")
var receivedString: String?
vmap["withString"] = { (val: String) in  // Clean typed syntax!
    print("  Received String: \"\(val)\"")
    receivedString = val
}
vmap.invokeNamedCallback("withString", "Hello Swift!")

if receivedString == "Hello Swift!" {
    print("✓ Callback received correct string value")
} else {
    print("✗ Callback with string failed")
}

// Test 10: Callback with OrkidObject argument
print("\nTest 10: Callback with typed argument (OrkidObject) -> Void")
var receivedTimer: Bool = false
vmap["withTimer"] = { (obj: OrkidObject) in  // Handles any OrkidObject subclass
    print("  Received OrkidObject: \(obj.typeName)")
    if obj.typeName == "ork::Timer" {
        receivedTimer = true
    }
}
let testTimer = Timer()
vmap.invokeNamedCallback("withTimer", testTimer)

if receivedTimer {
    print("✓ Callback received Timer (OrkidObject)")
} else {
    print("✗ Callback with Timer failed")
}

// Test 11: Callback with OrkidObject argument
print("\nTest 11: Callback with typed argument (OrkidObject) -> Void")
var receivedVec3: Bool = false
vmap["withVec3"] = { (obj: OrkidObject) in  // Handles any OrkidObject subclass
    print("  Received vec3: \(obj.typeName)")
    // cast obj to vec3
    let v3 =  obj as! vec3
    print("  Received vec3: \(v3.x), \(v3.y), \(v3.z)")
    receivedVec3 = true
}
vmap.invokeNamedCallback("withVec3", vec3(1.0, 2.0, 3.0))

if receivedVec3 {
    print("✓ Callback received Vec3 (OrkidObject)")
} else {
    print("✗ Callback with Vec3 failed")
}

// Test 12: Callback with 2 arguments
print("\nTest 12: Callback with 2 arguments (Int, String) -> Void")
var received2args = false
var receivedInt2: Int?
var receivedString2: String?
vmap["with2Args"] = { (val1: Any, val2: Any) in
    receivedInt2 = val1 as? Int
    receivedString2 = val2 as? String
    received2args = true
}
vmap.invokeNamedCallback("with2Args", 42, "hello")

if received2args && receivedInt2 == 42 && receivedString2 == "hello" {
    print("✓ Callback received 2 arguments correctly (42, \"hello\")")
} else {
    print("✗ Callback with 2 arguments failed")
}

// Test 13: Callback with 3 arguments
print("\nTest 13: Callback with 3 arguments (Int, Float, vec3) -> Void")
var received3args = false
var receivedInt3: Int?
var receivedFloat3: Float?
var receivedVec3_3: vec3?
vmap["with3Args"] = { (val1: Any, val2: Any, val3: Any) in
    receivedInt3 = val1 as? Int
    receivedFloat3 = val2 as? Float
    receivedVec3_3 = val3 as? vec3
    received3args = true
}
let testVec3 = vec3(10.0, 20.0, 30.0)
vmap.invokeNamedCallback("with3Args", 99, Float(3.14), testVec3)

if received3args && receivedInt3 == 99 && receivedFloat3 == 3.14 && receivedVec3_3 != nil {
    print("✓ Callback received 3 arguments correctly (99, 3.14, vec3)")
    if let v = receivedVec3_3 {
        print("  vec3: (\(v.x), \(v.y), \(v.z))")
    }
} else {
    print("✗ Callback with 3 arguments failed")
}

print("\n=== All Tests Complete ===")

// Clean shutdown
OrkCore.exit()
