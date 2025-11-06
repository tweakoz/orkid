////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2025, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#ifdef __cplusplus

#include "ork/kernel/svariant.h"

namespace ork::swift {
    class OrkidHandleBase;

    // Registration
    void registerSwiftCodec();

    // Helper functions for VarMap bridge (hide codec internals)
    OrkidHandleBase* swiftCodecEncode(const svar128_t& val);
    svar128_t swiftCodecDecode(OrkidHandleBase* handle);
}
extern "C" {
#endif

// Codec is internal to C++ - no C bridge functions needed
// Registration happens during orkid_swift_init()

#ifdef __cplusplus
}
#endif
