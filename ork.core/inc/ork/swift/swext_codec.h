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

// Primitive encoding/decoding functions for Swift callback arguments
OrkidHandleBase* orkid_encode_int(int32_t value);
OrkidHandleBase* orkid_encode_float(float value);
OrkidHandleBase* orkid_encode_double(double value);
OrkidHandleBase* orkid_encode_string(const char* value);

// Decoding functions (asserts if wrong type - rigid typing)
int32_t orkid_decode_int(OrkidHandleBase* handle);
float orkid_decode_float(OrkidHandleBase* handle);
double orkid_decode_double(OrkidHandleBase* handle);
const char* orkid_decode_string(OrkidHandleBase* handle);

// Try decode (returns false if wrong type - for type probing in Swift)
bool orkid_try_decode_int(OrkidHandleBase* handle, int32_t* out_value);
bool orkid_try_decode_float(OrkidHandleBase* handle, float* out_value);
bool orkid_try_decode_double(OrkidHandleBase* handle, double* out_value);
const char* orkid_try_decode_string(OrkidHandleBase* handle);  // Returns NULL if wrong type

#ifdef __cplusplus
}
#endif
