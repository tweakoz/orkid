////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/swift/orkid_handle.h>
#include <ork/swift/orkid_swift_bridge.h>
#include <ork/kernel/timer.h>
#include <string>

using namespace ork;
using namespace ork::swift;

// Forward declare core app functions (implemented in swift/macos/app_init.cpp)
extern "C" {
    void _coreappinit(int argc, char** argv);
    void _coreappexit();
    void _coreapppoll();
}

static thread_local std::string g_last_error;

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

        // Register ONLY Timer for now (Phase 2 - minimal)
        TypeRegistry::registerType<Timer>("ork::Timer");

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

// ================================================================
// Timer Functions
// ================================================================

OrkidHandleBase* orkid_timer_create() {
    try {
        auto handle = OrkidHandle<Timer>::makeShared();
        g_last_error.clear();
        return handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("Timer creation failed: ") + e.what();
        return nullptr;
    }
}

void orkid_timer_start(OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null timer handle in orkid_timer_start";
        return;
    }

    auto typed = handle->typedHandle<Timer>();
    if (!typed) {
        g_last_error = "Invalid timer handle type in orkid_timer_start";
        return;
    }

    try {
        typed->get()->Start();
        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("Timer start failed: ") + e.what();
    }
}

void orkid_timer_end(OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null timer handle in orkid_timer_end";
        return;
    }

    auto typed = handle->typedHandle<Timer>();
    if (!typed) {
        g_last_error = "Invalid timer handle type in orkid_timer_end";
        return;
    }

    try {
        typed->get()->End();
        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("Timer end failed: ") + e.what();
    }
}

float orkid_timer_secs_since_start(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null timer handle in orkid_timer_secs_since_start";
        return 0.0f;
    }

    // Need const_cast because typedHandle is not const
    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<Timer>();
    if (!typed) {
        g_last_error = "Invalid timer handle type in orkid_timer_secs_since_start";
        return 0.0f;
    }

    try {
        float result = typed->get()->SecsSinceStart();
        g_last_error.clear();
        return result;
    } catch (const std::exception& e) {
        g_last_error = std::string("Timer secs_since_start failed: ") + e.what();
        return 0.0f;
    }
}

float orkid_timer_get_sync_time() {
    try {
        float result = Timer::get_sync_time();
        g_last_error.clear();
        return result;
    } catch (const std::exception& e) {
        g_last_error = std::string("Timer get_sync_time failed: ") + e.what();
        return 0.0f;
    }
}

} // extern "C"
