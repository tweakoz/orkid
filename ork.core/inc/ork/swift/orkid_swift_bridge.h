////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>

#ifdef __cplusplus
namespace ork::swift { class OrkidHandleBase; }
typedef ork::swift::OrkidHandleBase OrkidHandleBase;
extern "C" {
#else
typedef struct OrkidHandleBase OrkidHandleBase;  // Opaque to C/Swift
#endif

// NOTE: For production, add nullability annotations:
//   - OrkidHandleBase* _Nonnull for creation functions (never return null)
//   - OrkidHandleBase* _Nullable for query functions (may return null)
// This provides cleaner Swift imports without force-unwrap operators.

// ================================================================
// Handle Management
// ================================================================

/// Release handle (virtual dtor properly cleans up derived class)
void orkid_handle_release(OrkidHandleBase* handle);

/// Get shared_ptr use count (for debugging)
int32_t orkid_handle_use_count(const OrkidHandleBase* handle);

/// Get type CRC
uint64_t orkid_handle_type_crc(const OrkidHandleBase* handle);

/// Get type name
const char* orkid_handle_type_name(const OrkidHandleBase* handle);

// ================================================================
// Core Lifecycle
// ================================================================

void orkid_swift_init(int argc, char** argv);
void orkid_swift_exit(void);
void orkid_swift_poll(void);
const char* orkid_get_last_error(void);

#ifdef __cplusplus
}
#endif

// ================================================================
// Module Headers
// ================================================================

#include <ork/swift/swext_timer.h>
#include <ork/swift/swext_math.h>
#include <ork/swift/swext_varmap.h>
#include <ork/swift/swext_codec.h>
