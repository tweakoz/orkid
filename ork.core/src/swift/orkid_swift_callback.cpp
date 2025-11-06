////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2025, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "ork/swift/orkid_swift_callback.h"
#include "ork/swift/orkid_handle.h"
#include "ork/swift/orkid_swift_bridge.h"
#include <atomic>
#include <functional>

using namespace ork;
using namespace ork::swift;

namespace ork::swift {

// Simple ID generator
static std::atomic<uint64_t> g_next_callback_id{1};

// Function pointers for Swift callback invokers (set by Swift at runtime)
static std::function<void(uint64_t, OrkidHandleBase*)> g_swift_callback_invoker = nullptr;
static std::function<void(uint64_t, OrkidHandleBase*, OrkidHandleBase*)> g_swift_callback_invoker_2arg = nullptr;
static std::function<void(uint64_t, OrkidHandleBase*, OrkidHandleBase*, OrkidHandleBase*)> g_swift_callback_invoker_3arg = nullptr;

extern thread_local std::string g_last_error;

} // namespace ork::swift

extern "C" {

////////////////////////////////////////////////////////////////
// SwiftCallback C Bridge Functions
////////////////////////////////////////////////////////////////

OrkidHandleBase* orkid_swiftcallback_create() {
    auto holder = std::make_shared<SwiftCallbackHolder>();
    holder->_callback_id = g_next_callback_id.fetch_add(1);
    return OrkidHandle<SwiftCallbackHolder>::assign(holder);
}

uint64_t orkid_swiftcallback_get_id(OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null handle in swiftcallback_get_id";
        return 0;
    }

    auto typed = handle->typedHandle<SwiftCallbackHolder>();
    if (!typed) {
        g_last_error = "Invalid SwiftCallbackHolder handle in swiftcallback_get_id";
        return 0;
    }

    return typed->get()->_callback_id;
}

// Register Swift callback invokers (called by Swift during init)
void orkid_register_swift_callback_invoker(void (*invoker)(uint64_t, OrkidHandleBase*)) {
    g_swift_callback_invoker = invoker;
}

void orkid_register_swift_callback_invoker_2arg(void (*invoker)(uint64_t, OrkidHandleBase*, OrkidHandleBase*)) {
    g_swift_callback_invoker_2arg = invoker;
}

void orkid_register_swift_callback_invoker_3arg(void (*invoker)(uint64_t, OrkidHandleBase*, OrkidHandleBase*, OrkidHandleBase*)) {
    g_swift_callback_invoker_3arg = invoker;
}

// C++ can call these to invoke Swift callbacks
void orkid_invoke_swift_callback(uint64_t callback_id, OrkidHandleBase* args) {
    if (g_swift_callback_invoker) {
        g_swift_callback_invoker(callback_id, args);
    } else {
        g_last_error = "Swift callback invoker not registered";
    }
}

void orkid_invoke_swift_callback_2arg(uint64_t callback_id, OrkidHandleBase* arg1, OrkidHandleBase* arg2) {
    if (g_swift_callback_invoker_2arg) {
        g_swift_callback_invoker_2arg(callback_id, arg1, arg2);
    } else {
        g_last_error = "Swift callback 2-arg invoker not registered";
    }
}

void orkid_invoke_swift_callback_3arg(uint64_t callback_id, OrkidHandleBase* arg1, OrkidHandleBase* arg2, OrkidHandleBase* arg3) {
    if (g_swift_callback_invoker_3arg) {
        g_swift_callback_invoker_3arg(callback_id, arg1, arg2, arg3);
    } else {
        g_last_error = "Swift callback 3-arg invoker not registered";
    }
}

} // extern "C"

////////////////////////////////////////////////////////////////
// Type Registration (called from orkid_swift_init)
////////////////////////////////////////////////////////////////

void swext_callback_register_types() {
    // Register SwiftCallbackHolder in type registry
    TypeRegistry::registerType<SwiftCallbackHolder>("ork::swift::SwiftCallbackHolder");
}
