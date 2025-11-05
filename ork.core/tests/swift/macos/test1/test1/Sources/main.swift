#!/usr/bin/env swift

import Foundation

// Import the C bridge (we'll need to configure search paths)
// For now, we'll declare the C functions directly as a proof of concept

// Opaque handle type
typealias OrkidHandleBase = OpaquePointer

// C function declarations
@_silgen_name("orkid_swift_init")
func orkid_swift_init(_ argc: Int32, _ argv: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?)

@_silgen_name("orkid_swift_exit")
func orkid_swift_exit()

@_silgen_name("orkid_get_last_error")
func orkid_get_last_error() -> UnsafePointer<CChar>?

@_silgen_name("orkid_timer_create")
func orkid_timer_create() -> OrkidHandleBase?

@_silgen_name("orkid_timer_start")
func orkid_timer_start(_ handle: OrkidHandleBase)

@_silgen_name("orkid_timer_end")
func orkid_timer_end(_ handle: OrkidHandleBase)

@_silgen_name("orkid_timer_secs_since_start")
func orkid_timer_secs_since_start(_ handle: OrkidHandleBase) -> Float

@_silgen_name("orkid_handle_release")
func orkid_handle_release(_ handle: OrkidHandleBase)

@_silgen_name("orkid_handle_use_count")
func orkid_handle_use_count(_ handle: OrkidHandleBase) -> Int32

// Helper to get last error as String
func getLastError() -> String {
    if let cstr = orkid_get_last_error() {
        return String(cString: cstr)
    }
    return ""
}

// Main test
print("=== Orkid Swift Bridge Test 1 ===\n")

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

// Create a Timer
print("Creating Timer...")
guard let timer = orkid_timer_create() else {
    print("ERROR: Failed to create timer: \(getLastError())")
    orkid_swift_exit()
    exit(1)
}
print("Timer created successfully")
print("  Use count: \(orkid_handle_use_count(timer))\n")

// Start the timer
print("Starting timer...")
orkid_timer_start(timer)
print("Timer started\n")

// Sleep for a bit
print("Sleeping for 0.5 seconds...")
usleep(500_000)  // 500ms

// Check elapsed time
let elapsed = orkid_timer_secs_since_start(timer)
print("Elapsed time: \(elapsed) seconds\n")

// Verify elapsed time is reasonable (between 0.4 and 0.6 seconds)
if elapsed >= 0.4 && elapsed <= 0.6 {
    print("✓ Timer elapsed time is correct\n")
} else {
    print("✗ Timer elapsed time is incorrect (expected ~0.5, got \(elapsed))\n")
}

// End the timer
print("Ending timer...")
orkid_timer_end(timer)
print("Timer ended\n")

// Release the timer
print("Releasing timer...")
print("  Use count before release: \(orkid_handle_use_count(timer))")
orkid_handle_release(timer)
print("Timer released\n")

// Shutdown Orkid
print("Shutting down Orkid...")
orkid_swift_exit()
print("Orkid shutdown complete\n")

print("=== Test Complete ===")
