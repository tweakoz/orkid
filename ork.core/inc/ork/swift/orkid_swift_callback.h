////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2025, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>

#ifdef __cplusplus
#include <memory>

namespace ork::swift { class OrkidHandleBase; }
typedef ork::swift::OrkidHandleBase OrkidHandleBase;

namespace ork::swift {

// Opaque callback holder (C++ doesn't know about Swift closures)
struct SwiftCallbackHolder {
    uint64_t _callback_id = 0;     // Unique ID for Swift registry lookup

    SwiftCallbackHolder() = default;
    ~SwiftCallbackHolder() = default;
};

using swiftcallback_ptr_t = std::shared_ptr<SwiftCallbackHolder>;

} // namespace ork::swift

extern "C" {
#else
typedef struct OrkidHandleBase OrkidHandleBase;
#endif

// ================================================================
// SwiftCallback C Bridge
// ================================================================

// Create opaque callback holder
OrkidHandleBase* orkid_swiftcallback_create(void);

// Get callback ID for Swift lookup
uint64_t orkid_swiftcallback_get_id(OrkidHandleBase* handle);

// Register Swift callback invoker (called by Swift during init)
void orkid_register_swift_callback_invoker(void (*invoker)(uint64_t, OrkidHandleBase*));

// C++ calls this to invoke Swift callbacks (uses registered invoker)
void orkid_invoke_swift_callback(uint64_t callback_id, OrkidHandleBase* args);

#ifdef __cplusplus
}
#endif
