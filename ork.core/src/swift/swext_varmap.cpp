////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2025, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "ork/swift/orkid_handle.h"
#include "ork/swift/orkid_swift_bridge.h"
#include "ork/swift/orkid_swift_callback.h"
#include "ork/swift/swext_codec.h"
#include "ork/kernel/varmap.inl"
#include <vector>

using namespace ork;
using namespace ork::swift;

namespace ork::swift {
    extern thread_local std::string g_last_error;
}

extern "C" {

////////////////////////////////////////////////////////////////
// VarMap C Bridge Functions
////////////////////////////////////////////////////////////////

OrkidHandleBase* orkid_varmap_create() {
    return OrkidHandle<varmap::VarMap>::makeShared();
}

OrkidHandleBase* orkid_varmap_get(OrkidHandleBase* vmap_handle, const char* key) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_get";
        return nullptr;
    }

    auto vmap = typed_vmap->get();
    if (!vmap->hasKey(key)) {
        return nullptr;  // Key not found - not an error
    }

    const auto& variant = vmap->valueForKey(key);
    return swiftCodecEncode(variant);  // Use codec to convert variant → handle
}

void orkid_varmap_set(OrkidHandleBase* vmap_handle, const char* key, OrkidHandleBase* value_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_set";
        return;
    }

    if (!value_handle) {
        g_last_error = "Null value handle in varmap_set";
        return;
    }

    auto variant = swiftCodecDecode(value_handle);  // Use codec to convert handle → variant
    typed_vmap->get()->setValueForKey(key, variant);
}

void orkid_varmap_remove(OrkidHandleBase* vmap_handle, const char* key) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_remove";
        return;
    }

    typed_vmap->get()->clearKey(key);
}

bool orkid_varmap_contains(OrkidHandleBase* vmap_handle, const char* key) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_contains";
        return false;
    }

    return typed_vmap->get()->hasKey(key);
}

const char** orkid_varmap_keys(OrkidHandleBase* vmap_handle, int32_t* out_count) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_keys";
        *out_count = 0;
        return nullptr;
    }

    auto vmap = typed_vmap->get();
    size_t count = vmap->_themap.size();

    if (count == 0) {
        *out_count = 0;
        return nullptr;
    }

    // Allocate array of C strings
    const char** keys = (const char**)malloc(sizeof(char*) * count);

    size_t i = 0;
    for (const auto& kv : vmap->_themap) {
        const auto& key = kv.first;
        keys[i] = strdup(key.c_str());  // Caller must free with orkid_varmap_free_keys
        i++;
    }

    *out_count = (int32_t)count;
    return keys;
}

void orkid_varmap_free_keys(const char** keys, int32_t count) {
    if (!keys) return;

    for (int32_t i = 0; i < count; i++) {
        free((void*)keys[i]);
    }
    free((void*)keys);
}

int32_t orkid_varmap_size(OrkidHandleBase* vmap_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_size";
        return 0;
    }

    return (int32_t)typed_vmap->get()->_themap.size();
}

void orkid_varmap_clear(OrkidHandleBase* vmap_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_clear";
        return;
    }

    typed_vmap->get()->_themap.clear();
}

OrkidHandleBase* orkid_varmap_clone(OrkidHandleBase* vmap_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_clone";
        return nullptr;
    }

    // Create new VarMap and copy contents
    auto cloned = std::make_shared<varmap::VarMap>();
    *cloned = *typed_vmap->get();  // Copy operator

    return OrkidHandle<varmap::VarMap>::assign(cloned);
}

void orkid_varmap_invoke_callback(OrkidHandleBase* vmap_handle, const char* key) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_invoke_callback";
        return;
    }

    auto vmap = typed_vmap->get();
    if (!vmap->hasKey(key)) {
        g_last_error = FormatString("Key '%s' not found in VarMap", key);
        return;
    }

    // Get the variant
    const auto& variant = vmap->valueForKey(key);

    // Try to extract SwiftCallbackHolder shared_ptr
    auto callback_ptr_opt = variant.tryAsShared<SwiftCallbackHolder>();
    if (!callback_ptr_opt) {
        g_last_error = FormatString("Value at key '%s' is not a SwiftCallback", key);
        return;
    }

    // Get callback ID and invoke through Swift bridge
    uint64_t callback_id = callback_ptr_opt.value()->_callback_id;

    // Use the registered Swift callback invoker
    orkid_invoke_swift_callback(callback_id, nullptr);
}

void orkid_varmap_invoke_callback_1arg(OrkidHandleBase* vmap_handle, const char* key, OrkidHandleBase* arg_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_invoke_callback_1arg";
        return;
    }

    auto vmap = typed_vmap->get();
    if (!vmap->hasKey(key)) {
        g_last_error = FormatString("Key '%s' not found in VarMap", key);
        return;
    }

    // Get the variant
    const auto& variant = vmap->valueForKey(key);

    // Try to extract SwiftCallbackHolder shared_ptr
    auto callback_ptr_opt = variant.tryAsShared<SwiftCallbackHolder>();
    if (!callback_ptr_opt) {
        g_last_error = FormatString("Value at key '%s' is not a SwiftCallback", key);
        return;
    }

    // Get callback ID and invoke through Swift bridge with argument
    uint64_t callback_id = callback_ptr_opt.value()->_callback_id;

    // Pass the argument handle directly to Swift
    orkid_invoke_swift_callback(callback_id, arg_handle);
}

void orkid_varmap_invoke_callback_2arg(OrkidHandleBase* vmap_handle, const char* key, OrkidHandleBase* arg1_handle, OrkidHandleBase* arg2_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_invoke_callback_2arg";
        return;
    }

    auto vmap = typed_vmap->get();
    if (!vmap->hasKey(key)) {
        g_last_error = FormatString("Key '%s' not found in VarMap", key);
        return;
    }

    const auto& variant = vmap->valueForKey(key);
    auto callback_ptr_opt = variant.tryAsShared<SwiftCallbackHolder>();
    if (!callback_ptr_opt) {
        g_last_error = FormatString("Value at key '%s' is not a SwiftCallback", key);
        return;
    }

    uint64_t callback_id = callback_ptr_opt.value()->_callback_id;
    orkid_invoke_swift_callback_2arg(callback_id, arg1_handle, arg2_handle);
}

void orkid_varmap_invoke_callback_3arg(OrkidHandleBase* vmap_handle, const char* key, OrkidHandleBase* arg1_handle, OrkidHandleBase* arg2_handle, OrkidHandleBase* arg3_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle in varmap_invoke_callback_3arg";
        return;
    }

    auto vmap = typed_vmap->get();
    if (!vmap->hasKey(key)) {
        g_last_error = FormatString("Key '%s' not found in VarMap", key);
        return;
    }

    const auto& variant = vmap->valueForKey(key);
    auto callback_ptr_opt = variant.tryAsShared<SwiftCallbackHolder>();
    if (!callback_ptr_opt) {
        g_last_error = FormatString("Value at key '%s' is not a SwiftCallback", key);
        return;
    }

    uint64_t callback_id = callback_ptr_opt.value()->_callback_id;
    orkid_invoke_swift_callback_3arg(callback_id, arg1_handle, arg2_handle, arg3_handle);
}

} // extern "C"

////////////////////////////////////////////////////////////////
// Type Registration (called from orkid_swift_init)
////////////////////////////////////////////////////////////////

void swext_varmap_register_types() {
    // Register VarMap and svar128_t in type registry
    TypeRegistry::registerType<varmap::VarMap>("ork::varmap::VarMap");
    TypeRegistry::registerType<svar128_t>("ork::svar128_t");
}
