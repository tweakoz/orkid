#!/usr/bin/env swift

import Foundation
import OrkCore

// ========================================
// MAIN TEST
// ========================================

print("=== Orkid Swift Timer Test (OrkCore Module) ===\n")

// Initialize Orkid
print("Initializing Orkid...")
OrkCore.initialize()

if !OrkCore.lastError.isEmpty {
    print("ERROR during init: \(OrkCore.lastError)")
    exit(1)
}
print("Orkid initialized successfully\n")

// Create a Timer using OrkCore module
print("Creating Timer...")
let timer = OrkCore.Timer()
print("Timer created successfully")
print("  Type: \(timer.typeName)")
print("  Use count: \(timer.useCount)\n")

// Start the timer
print("Starting timer...")
timer.start()
print("Timer started\n")

// Sleep for a bit
print("Sleeping for 0.5 seconds...")
usleep(500_000)  // 500ms

// Check elapsed time
let elapsed = timer.secsSinceStart
print("Elapsed time: \(elapsed) seconds")
print("Timer description: \(timer)\n")

// Verify elapsed time is reasonable (between 0.4 and 0.6 seconds)
if elapsed >= 0.4 && elapsed <= 0.6 {
    print("✓ Timer elapsed time is correct\n")
} else {
    print("✗ Timer elapsed time is incorrect (expected ~0.5, got \(elapsed))\n")
}

// End the timer
print("Ending timer...")
timer.end()
print("Timer ended\n")

// Test measure() convenience method
print("Testing measure() convenience method...")
let measureTime = timer.measure {
    usleep(100_000)  // 100ms
}
print("  Measured time: \(measureTime) seconds")
if measureTime >= 0.09 && measureTime <= 0.15 {
    print("✓ Measure method works correctly\n")
} else {
    print("✗ Measure method incorrect (expected ~0.1, got \(measureTime))\n")
}

// Check use count before automatic cleanup
print("Use count before scope exit: \(timer.useCount)")
print("Timer will be automatically released via deinit when scope ends\n")

// Timer will be automatically released here when it goes out of scope

// Shutdown Orkid
print("Shutting down Orkid...")
OrkCore.exit()
print("Orkid shutdown complete\n")

print("=== Test Complete ===")
