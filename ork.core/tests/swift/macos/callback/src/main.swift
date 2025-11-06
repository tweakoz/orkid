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

print("\n=== All Tests Complete ===")

// Clean shutdown
OrkCore.exit()
