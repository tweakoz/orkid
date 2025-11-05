////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/swift/orkid_handle.h>
#include <ork/swift/orkid_swift_bridge.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>
#include <string>

using namespace ork;
using namespace ork::swift;

namespace ork::swift {
    extern thread_local std::string g_last_error;
}

// ================================================================
// Math Type Registration
// ================================================================

void swext_math_register_types() {
    TypeRegistry::registerType<fvec3>("ork::fvec3");
    TypeRegistry::registerType<fvec4>("ork::fvec4");
    TypeRegistry::registerType<fmtx4>("ork::fmtx4");
    TypeRegistry::registerType<fquat>("ork::fquat");
}

// ================================================================
// Math C API Implementation
// ================================================================

extern "C" {

// ================================================================
// vec3 Functions
// ================================================================

OrkidHandleBase* orkid_fvec3_create(float x, float y, float z) {
    try {
        auto handle = OrkidHandle<fvec3>::makeShared(x, y, z);
        g_last_error.clear();
        return handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec3 creation failed: ") + e.what();
        return nullptr;
    }
}

float orkid_fvec3_get_x(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fvec3 handle in orkid_fvec3_get_x";
        return 0.0f;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fvec3>();
    if (!typed) {
        g_last_error = "Invalid fvec3 handle type";
        return 0.0f;
    }

    try {
        g_last_error.clear();
        return typed->get()->x;
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec3 get_x failed: ") + e.what();
        return 0.0f;
    }
}

float orkid_fvec3_get_y(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fvec3 handle in orkid_fvec3_get_y";
        return 0.0f;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fvec3>();
    if (!typed) {
        g_last_error = "Invalid fvec3 handle type";
        return 0.0f;
    }

    try {
        g_last_error.clear();
        return typed->get()->y;
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec3 get_y failed: ") + e.what();
        return 0.0f;
    }
}

float orkid_fvec3_get_z(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fvec3 handle in orkid_fvec3_get_z";
        return 0.0f;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fvec3>();
    if (!typed) {
        g_last_error = "Invalid fvec3 handle type";
        return 0.0f;
    }

    try {
        g_last_error.clear();
        return typed->get()->z;
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec3 get_z failed: ") + e.what();
        return 0.0f;
    }
}

void orkid_fvec3_set_x(OrkidHandleBase* handle, float value) {
    if (!handle) {
        g_last_error = "Null fvec3 handle in orkid_fvec3_set_x";
        return;
    }

    auto typed = handle->typedHandle<fvec3>();
    if (!typed) {
        g_last_error = "Invalid fvec3 handle type";
        return;
    }

    try {
        typed->get()->x = value;
        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec3 set_x failed: ") + e.what();
    }
}

void orkid_fvec3_set_y(OrkidHandleBase* handle, float value) {
    if (!handle) {
        g_last_error = "Null fvec3 handle in orkid_fvec3_set_y";
        return;
    }

    auto typed = handle->typedHandle<fvec3>();
    if (!typed) {
        g_last_error = "Invalid fvec3 handle type";
        return;
    }

    try {
        typed->get()->y = value;
        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec3 set_y failed: ") + e.what();
    }
}

void orkid_fvec3_set_z(OrkidHandleBase* handle, float value) {
    if (!handle) {
        g_last_error = "Null fvec3 handle in orkid_fvec3_set_z";
        return;
    }

    auto typed = handle->typedHandle<fvec3>();
    if (!typed) {
        g_last_error = "Invalid fvec3 handle type";
        return;
    }

    try {
        typed->get()->z = value;
        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec3 set_z failed: ") + e.what();
    }
}

float orkid_fvec3_length(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fvec3 handle in orkid_fvec3_length";
        return 0.0f;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fvec3>();
    if (!typed) {
        g_last_error = "Invalid fvec3 handle type";
        return 0.0f;
    }

    try {
        g_last_error.clear();
        return typed->get()->length();
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec3 length failed: ") + e.what();
        return 0.0f;
    }
}

OrkidHandleBase* orkid_fvec3_normalized(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fvec3 handle in orkid_fvec3_normalized";
        return nullptr;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fvec3>();
    if (!typed) {
        g_last_error = "Invalid fvec3 handle type";
        return nullptr;
    }

    try {
        fvec3 normalized = typed->get()->normalized();
        auto result = OrkidHandle<fvec3>::makeShared(normalized.x, normalized.y, normalized.z);
        g_last_error.clear();
        return result;
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec3 normalized failed: ") + e.what();
        return nullptr;
    }
}

} // extern "C"
