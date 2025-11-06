////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2025, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "ork/swift/orkid_handle.h"
#include "ork/swift/orkid_swift_bridge.h"
#include "ork/swift/orkid_swift_callback.h"
#include "ork/kernel/varmap.inl"
#include "ork/kernel/timer.h"
#include "ork/math/cvector3.h"
#include "ork/math/cvector4.h"
#include "ork/math/cmatrix4.h"
#include <unordered_map>
#include <typeindex>
#include <functional>

using namespace ork;
using namespace ork::swift;

namespace ork::swift {

extern thread_local std::string g_last_error;

////////////////////////////////////////////////////////////////
// Swift Codec (similar to PyCodecImpl in pycodec)
////////////////////////////////////////////////////////////////

struct SwiftCodecImpl {
    using encoderfn_t = std::function<OrkidHandleBase*(const svar128_t&)>;
    using decoderfn_t = std::function<svar128_t(OrkidHandleBase*)>;

    std::unordered_map<uint64_t, encoderfn_t> _encoders;  // Key is type hash
    std::unordered_map<uint64_t, decoderfn_t> _decoders;  // Key is type CRC

    // Variant ↔ Handle
    OrkidHandleBase* encode(const svar128_t& val) const;
    svar128_t decode(OrkidHandleBase* handle) const;

    // Raw C++ object ↔ Handle (template helpers)
    template <typename T>
    OrkidHandleBase* encodeObject(T* obj) const;

    template <typename T>
    OrkidHandleBase* encodeShared(std::shared_ptr<T> obj) const;

    template <typename T>
    std::shared_ptr<T> decodeShared(OrkidHandleBase* handle) const;
};

// Global codec instance
SwiftCodecImpl g_swift_codec;

////////////////////////////////////////////////////////////////
// Encode: variant → handle
////////////////////////////////////////////////////////////////

OrkidHandleBase* SwiftCodecImpl::encode(const svar128_t& val) const {
    if (!val.isSet()) {
        return nullptr;
    }

    auto orktypeid = val.getOrkTypeId();
    auto it = _encoders.find(orktypeid._hashed);

    if (it != _encoders.end()) {
        return it->second(val);
    }

    // Fallback: primitives (wrap in svar128_t handle)
    if (auto as_bool = val.tryAs<bool>()) {
        auto var = OrkidHandle<svar128_t>::makeShared();
        var->typedHandle<svar128_t>()->get()->set<bool>(as_bool.value());
        return var;
    } else if (auto as_int = val.tryAs<int>()) {
        auto var = OrkidHandle<svar128_t>::makeShared();
        var->typedHandle<svar128_t>()->get()->set<int>(as_int.value());
        return var;
    } else if (auto as_float = val.tryAs<float>()) {
        auto var = OrkidHandle<svar128_t>::makeShared();
        var->typedHandle<svar128_t>()->get()->set<float>(as_float.value());
        return var;
    } else if (auto as_double = val.tryAs<double>()) {
        auto var = OrkidHandle<svar128_t>::makeShared();
        var->typedHandle<svar128_t>()->get()->set<double>(as_double.value());
        return var;
    } else if (auto as_str = val.tryAs<std::string>()) {
        auto var = OrkidHandle<svar128_t>::makeShared();
        var->typedHandle<svar128_t>()->get()->set<std::string>(as_str.value());
        return var;
    }

    // Unknown type - print diagnostics and assert
    printf("SwiftCodec::encode ERROR: unregistered type '%s' (hash=%llx)\n",
           val.typeName(), orktypeid._hashed);
    printf("  Available encoders:\n");
    for (const auto& kv : _encoders) {
        printf("    hash=%llx\n", kv.first);
    }
    g_last_error = FormatString("SwiftCodec::encode: unregistered type %s", val.typeName());
    OrkAssert(false);
    return nullptr;
}

////////////////////////////////////////////////////////////////
// Decode: handle → variant
////////////////////////////////////////////////////////////////

svar128_t SwiftCodecImpl::decode(OrkidHandleBase* handle) const {
    if (!handle) {
        return svar128_t();
    }

    auto typeCRC = handle->typeCRC();
    auto it = _decoders.find(typeCRC);

    if (it != _decoders.end()) {
        return it->second(handle);
    }

    // Fallback: check if it's a wrapped primitive (svar128_t handle)
    if (auto typed_var = handle->typedHandle<svar128_t>()) {
        return *typed_var->get();
    }

    // Unknown type - print diagnostics and assert
    const char* type_name = handle->typeName();
    printf("SwiftCodec::decode ERROR: unregistered type '%s' (CRC=%llx)\n",
           type_name, typeCRC);
    printf("  Available decoders:\n");
    for (const auto& kv : _decoders) {
        printf("    CRC=%llx\n", kv.first);
    }
    g_last_error = FormatString("SwiftCodec::decode: unregistered type CRC %llx", typeCRC);
    OrkAssert(false);
    return svar128_t();
}

////////////////////////////////////////////////////////////////
// Direct Object ↔ Handle Conversions
////////////////////////////////////////////////////////////////

template <typename T>
OrkidHandleBase* SwiftCodecImpl::encodeObject(T* obj) const {
    if (!obj) return nullptr;
    // Wrap raw pointer - creates shared_ptr with custom deleter that does nothing
    // (assumes caller manages lifetime)
    return OrkidHandle<T>::makeShared(*obj);
}

template <typename T>
OrkidHandleBase* SwiftCodecImpl::encodeShared(std::shared_ptr<T> obj) const {
    if (!obj) return nullptr;
    return OrkidHandle<T>::assign(obj);
}

template <typename T>
std::shared_ptr<T> SwiftCodecImpl::decodeShared(OrkidHandleBase* handle) const {
    if (!handle) return nullptr;

    auto typed = handle->typedHandle<T>();
    if (!typed) return nullptr;

    return typed->get();
}

////////////////////////////////////////////////////////////////
// Generic Type Registration
////////////////////////////////////////////////////////////////

// For VALUE types (vec3, vec4, mat4, quat): variant stores VALUE, Swift uses shared_ptr
template <typename T>
void registerSwiftValueType() {
    auto value_type_hash = TypeId::of<T>()._hashed;  // Variant contains T (value)
    auto handle_type_hash = TypeRegistry::getTypeCRC<T>();  // Handle CRC from TypeRegistry

    // Encoder: variant → handle (variant contains T value, wrap in shared_ptr for Swift)
    g_swift_codec._encoders[value_type_hash] = [](const svar128_t& val) -> OrkidHandleBase* {
        if (auto value_opt = val.tryAs<T>()) {
            // Copy value and wrap in shared_ptr for Swift
            return OrkidHandle<T>::makeShared(value_opt.value());
        }
        return nullptr;
    };

    // Decoder: handle → variant (handle has shared_ptr<T>, unwrap to store value in variant)
    g_swift_codec._decoders[handle_type_hash] = [](OrkidHandleBase* handle) -> svar128_t {
        if (auto typed = handle->typedHandle<T>()) {
            svar128_t result;
            // Dereference shared_ptr and store value in variant
            result.set<T>(*typed->get());
            return result;
        }
        return svar128_t();
    };
}

// For OBJECT types (Timer, VarMap): variant stores shared_ptr, Swift uses shared_ptr
template <typename T>
void registerSwiftObjectType() {
    auto ptr_type_hash = TypeId::of<std::shared_ptr<T>>()._hashed;  // Variant contains shared_ptr<T>
    auto handle_type_hash = TypeRegistry::getTypeCRC<T>();           // Handle CRC from TypeRegistry

    // Encoder: variant → handle (variant contains shared_ptr<T>)
    g_swift_codec._encoders[ptr_type_hash] = [](const svar128_t& val) -> OrkidHandleBase* {
        if (auto ptr = val.tryAsShared<T>()) {
            return OrkidHandle<T>::assign(ptr.value());
        }
        return nullptr;
    };

    // Decoder: handle → variant (handle has shared_ptr<T>, store same shared_ptr in variant)
    g_swift_codec._decoders[handle_type_hash] = [](OrkidHandleBase* handle) -> svar128_t {
        if (auto typed = handle->typedHandle<T>()) {
            svar128_t result;
            result.set<std::shared_ptr<T>>(typed->get());  // Store the shared_ptr directly
            return result;
        }
        return svar128_t();
    };
}

////////////////////////////////////////////////////////////////
// Codec Registration (called during orkid_swift_init)
////////////////////////////////////////////////////////////////

void registerSwiftCodec() {
    // Register value types (variant stores value, Swift wraps in shared_ptr)
    registerSwiftValueType<fvec3>();
    registerSwiftValueType<fvec4>();
    registerSwiftValueType<fmtx4>();

    // Register object types (variant stores shared_ptr, Swift uses shared_ptr)
    registerSwiftObjectType<Timer>();
    registerSwiftObjectType<varmap::VarMap>();
    registerSwiftObjectType<SwiftCallbackHolder>();
}

////////////////////////////////////////////////////////////////
// Helper functions for VarMap bridge (hide codec internals)
////////////////////////////////////////////////////////////////

OrkidHandleBase* swiftCodecEncode(const svar128_t& val) {
    return g_swift_codec.encode(val);
}

svar128_t swiftCodecDecode(OrkidHandleBase* handle) {
    return g_swift_codec.decode(handle);
}

} // namespace ork::swift

////////////////////////////////////////////////////////////////
// Primitive encoding functions (C bridge for Swift)
////////////////////////////////////////////////////////////////

extern "C" {

using namespace ork::swift;

OrkidHandleBase* orkid_encode_int(int32_t value) {
    svar128_t var;
    var.set<int>(value);
    return swiftCodecEncode(var);
}

OrkidHandleBase* orkid_encode_float(float value) {
    svar128_t var;
    var.set<float>(value);
    return swiftCodecEncode(var);
}

OrkidHandleBase* orkid_encode_double(double value) {
    svar128_t var;
    var.set<double>(value);
    return swiftCodecEncode(var);
}

OrkidHandleBase* orkid_encode_string(const char* value) {
    svar128_t var;
    var.set<std::string>(std::string(value));
    return swiftCodecEncode(var);
}

int32_t orkid_decode_int(OrkidHandleBase* handle) {
    svar128_t var = swiftCodecDecode(handle);
    if (!var.isA<int>()) {
        printf("orkid_decode_int ERROR: expected int, got type '%s'\n", var.typeName());
        OrkAssert(false);
    }
    return var.get<int>();
}

float orkid_decode_float(OrkidHandleBase* handle) {
    svar128_t var = swiftCodecDecode(handle);
    if (!var.isA<float>()) {
        printf("orkid_decode_float ERROR: expected float, got type '%s'\n", var.typeName());
        OrkAssert(false);
    }
    return var.get<float>();
}

double orkid_decode_double(OrkidHandleBase* handle) {
    svar128_t var = swiftCodecDecode(handle);
    if (!var.isA<double>()) {
        printf("orkid_decode_double ERROR: expected double, got type '%s'\n", var.typeName());
        OrkAssert(false);
    }
    return var.get<double>();
}

const char* orkid_decode_string(OrkidHandleBase* handle) {
    svar128_t var = swiftCodecDecode(handle);
    if (!var.isA<std::string>()) {
        printf("orkid_decode_string ERROR: expected string, got type '%s'\n", var.typeName());
        OrkAssert(false);
    }
    // WARNING: Returns pointer to internal string - copy immediately in Swift
    return var.get<std::string>().c_str();
}

// Try decode functions (for type probing)
bool orkid_try_decode_int(OrkidHandleBase* handle, int32_t* out_value) {
    svar128_t var = swiftCodecDecode(handle);
    if (var.isA<int>()) {
        *out_value = var.get<int>();
        return true;
    }
    return false;
}

bool orkid_try_decode_float(OrkidHandleBase* handle, float* out_value) {
    svar128_t var = swiftCodecDecode(handle);
    if (var.isA<float>()) {
        *out_value = var.get<float>();
        return true;
    }
    return false;
}

bool orkid_try_decode_double(OrkidHandleBase* handle, double* out_value) {
    svar128_t var = swiftCodecDecode(handle);
    if (var.isA<double>()) {
        *out_value = var.get<double>();
        return true;
    }
    return false;
}

const char* orkid_try_decode_string(OrkidHandleBase* handle) {
    svar128_t var = swiftCodecDecode(handle);
    if (var.isA<std::string>()) {
        // WARNING: Returns pointer to internal string - copy immediately in Swift
        return var.get<std::string>().c_str();
    }
    return nullptr;
}

} // extern "C"
