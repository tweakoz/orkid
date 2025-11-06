////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/swift/orkid_handle.h>
#include <ork/swift/orkid_swift_bridge.h>
#include <string>

using namespace ork;
using namespace ork::swift;

// Forward declare core app functions (implemented in swift/macos/app_init.cpp)
extern "C" {
    void _coreappinit(int argc, char** argv);
    void _coreappexit();
    void _coreapppoll();
}

// Forward declare type registration functions from swext_* modules
void swext_timer_register_types();
void swext_math_register_types();
void swext_varmap_register_types();

// Forward declare codec registration (from swext_codec.cpp)
namespace ork::swift {
    void registerSwiftCodec();
}

namespace ork::swift {
    thread_local std::string g_last_error;
}

// ================================================================
// C API Implementation
// ================================================================

extern "C" {

// ================================================================
// Handle Management
// ================================================================

void orkid_handle_release(OrkidHandleBase* handle) {
    delete handle;  // Virtual dtor properly cleans up OrkidHandle<T>
}

int32_t orkid_handle_use_count(const OrkidHandleBase* handle) {
    return handle->useCount();
}

uint64_t orkid_handle_type_crc(const OrkidHandleBase* handle) {
    return handle->typeCRC();
}

const char* orkid_handle_type_name(const OrkidHandleBase* handle) {
    return handle->typeName();
}

// ================================================================
// Core Lifecycle - Register All Types Here!
// ================================================================

void orkid_swift_init(int argc, char** argv) {
    try {
        // Initialize Orkid core
        _coreappinit(argc, argv);

        // Register types from swext modules
        swext_timer_register_types();
        swext_math_register_types();
        swext_varmap_register_types();

        // Register codec (must come after type registration)
        ork::swift::registerSwiftCodec();

        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("Init failed: ") + e.what();
    }
}

void orkid_swift_exit() {
    try {
        _coreappexit();
    } catch (...) {
        // Swallow exceptions during shutdown
    }
}

void orkid_swift_poll() {
    _coreapppoll();
}

const char* orkid_get_last_error() {
    return g_last_error.c_str();
}

} // extern "C"
