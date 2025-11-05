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

// ================================================================
// vec4 Functions
// ================================================================

OrkidHandleBase* orkid_fvec4_create(float x, float y, float z, float w) {
    try {
        auto handle = OrkidHandle<fvec4>::makeShared(x, y, z, w);
        g_last_error.clear();
        return handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 creation failed: ") + e.what();
        return nullptr;
    }
}

float orkid_fvec4_get_x(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fvec4 handle in orkid_fvec4_get_x";
        return 0.0f;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fvec4>();
    if (!typed) {
        g_last_error = "Invalid fvec4 handle type";
        return 0.0f;
    }

    try {
        g_last_error.clear();
        return typed->get()->x;
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 get_x failed: ") + e.what();
        return 0.0f;
    }
}

float orkid_fvec4_get_y(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fvec4 handle in orkid_fvec4_get_y";
        return 0.0f;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fvec4>();
    if (!typed) {
        g_last_error = "Invalid fvec4 handle type";
        return 0.0f;
    }

    try {
        g_last_error.clear();
        return typed->get()->y;
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 get_y failed: ") + e.what();
        return 0.0f;
    }
}

float orkid_fvec4_get_z(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fvec4 handle in orkid_fvec4_get_z";
        return 0.0f;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fvec4>();
    if (!typed) {
        g_last_error = "Invalid fvec4 handle type";
        return 0.0f;
    }

    try {
        g_last_error.clear();
        return typed->get()->z;
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 get_z failed: ") + e.what();
        return 0.0f;
    }
}

float orkid_fvec4_get_w(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fvec4 handle in orkid_fvec4_get_w";
        return 0.0f;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fvec4>();
    if (!typed) {
        g_last_error = "Invalid fvec4 handle type";
        return 0.0f;
    }

    try {
        g_last_error.clear();
        return typed->get()->w;
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 get_w failed: ") + e.what();
        return 0.0f;
    }
}

void orkid_fvec4_set_x(OrkidHandleBase* handle, float value) {
    if (!handle) {
        g_last_error = "Null fvec4 handle in orkid_fvec4_set_x";
        return;
    }

    auto typed = handle->typedHandle<fvec4>();
    if (!typed) {
        g_last_error = "Invalid fvec4 handle type";
        return;
    }

    try {
        typed->get()->x = value;
        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 set_x failed: ") + e.what();
    }
}

void orkid_fvec4_set_y(OrkidHandleBase* handle, float value) {
    if (!handle) {
        g_last_error = "Null fvec4 handle in orkid_fvec4_set_y";
        return;
    }

    auto typed = handle->typedHandle<fvec4>();
    if (!typed) {
        g_last_error = "Invalid fvec4 handle type";
        return;
    }

    try {
        typed->get()->y = value;
        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 set_y failed: ") + e.what();
    }
}

void orkid_fvec4_set_z(OrkidHandleBase* handle, float value) {
    if (!handle) {
        g_last_error = "Null fvec4 handle in orkid_fvec4_set_z";
        return;
    }

    auto typed = handle->typedHandle<fvec4>();
    if (!typed) {
        g_last_error = "Invalid fvec4 handle type";
        return;
    }

    try {
        typed->get()->z = value;
        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 set_z failed: ") + e.what();
    }
}

void orkid_fvec4_set_w(OrkidHandleBase* handle, float value) {
    if (!handle) {
        g_last_error = "Null fvec4 handle in orkid_fvec4_set_w";
        return;
    }

    auto typed = handle->typedHandle<fvec4>();
    if (!typed) {
        g_last_error = "Invalid fvec4 handle type";
        return;
    }

    try {
        typed->get()->w = value;
        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 set_w failed: ") + e.what();
    }
}

float orkid_fvec4_length(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fvec4 handle in orkid_fvec4_length";
        return 0.0f;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fvec4>();
    if (!typed) {
        g_last_error = "Invalid fvec4 handle type";
        return 0.0f;
    }

    try {
        g_last_error.clear();
        return typed->get()->magnitude();
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 length failed: ") + e.what();
        return 0.0f;
    }
}

OrkidHandleBase* orkid_fvec4_normalized(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fvec4 handle in orkid_fvec4_normalized";
        return nullptr;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fvec4>();
    if (!typed) {
        g_last_error = "Invalid fvec4 handle type";
        return nullptr;
    }

    try {
        fvec4 normalized = typed->get()->normalized();
        auto result = OrkidHandle<fvec4>::makeShared(normalized.x, normalized.y, normalized.z, normalized.w);
        g_last_error.clear();
        return result;
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 normalized failed: ") + e.what();
        return nullptr;
    }
}

float orkid_fvec4_dot(const OrkidHandleBase* a, const OrkidHandleBase* b) {
    if (!a || !b) {
        g_last_error = "Null fvec4 handle in orkid_fvec4_dot";
        return 0.0f;
    }

    auto typed_a = const_cast<OrkidHandleBase*>(a)->typedHandle<fvec4>();
    auto typed_b = const_cast<OrkidHandleBase*>(b)->typedHandle<fvec4>();

    if (!typed_a || !typed_b) {
        g_last_error = "Invalid fvec4 handle type in orkid_fvec4_dot";
        return 0.0f;
    }

    try {
        g_last_error.clear();
        return typed_a->get()->dotWith(*typed_b->get());
    } catch (const std::exception& e) {
        g_last_error = std::string("fvec4 dot failed: ") + e.what();
        return 0.0f;
    }
}

// ================================================================
// mat4 (Matrix44) Functions
// ================================================================

OrkidHandleBase* orkid_fmtx4_create_identity() {
    try {
        auto handle = OrkidHandle<fmtx4>::makeShared();
        handle->get()->setToIdentity();
        g_last_error.clear();
        return handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 creation failed: ") + e.what();
        return nullptr;
    }
}

OrkidHandleBase* orkid_fmtx4_create_translation(float x, float y, float z) {
    try {
        auto handle = OrkidHandle<fmtx4>::makeShared();
        handle->get()->setToIdentity();
        handle->get()->setTranslation(x, y, z);
        g_last_error.clear();
        return handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 create_translation failed: ") + e.what();
        return nullptr;
    }
}

OrkidHandleBase* orkid_fmtx4_create_scale(float x, float y, float z) {
    try {
        auto handle = OrkidHandle<fmtx4>::makeShared();
        handle->get()->setToIdentity();
        handle->get()->setScale(x, y, z);
        g_last_error.clear();
        return handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 create_scale failed: ") + e.what();
        return nullptr;
    }
}

OrkidHandleBase* orkid_fmtx4_create_rotation_x(float radians) {
    try {
        auto handle = OrkidHandle<fmtx4>::makeShared();
        handle->get()->setToIdentity();
        handle->get()->setRotateX(radians);
        g_last_error.clear();
        return handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 create_rotation_x failed: ") + e.what();
        return nullptr;
    }
}

OrkidHandleBase* orkid_fmtx4_create_rotation_y(float radians) {
    try {
        auto handle = OrkidHandle<fmtx4>::makeShared();
        handle->get()->setToIdentity();
        handle->get()->setRotateY(radians);
        g_last_error.clear();
        return handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 create_rotation_y failed: ") + e.what();
        return nullptr;
    }
}

OrkidHandleBase* orkid_fmtx4_create_rotation_z(float radians) {
    try {
        auto handle = OrkidHandle<fmtx4>::makeShared();
        handle->get()->setToIdentity();
        handle->get()->setRotateZ(radians);
        g_last_error.clear();
        return handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 create_rotation_z failed: ") + e.what();
        return nullptr;
    }
}

void orkid_fmtx4_get_translation(const OrkidHandleBase* handle, float* out_x, float* out_y, float* out_z) {
    if (!handle) {
        g_last_error = "Null fmtx4 handle in orkid_fmtx4_get_translation";
        return;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fmtx4>();
    if (!typed) {
        g_last_error = "Invalid fmtx4 handle type";
        return;
    }

    try {
        fvec3 trans = typed->get()->translation();
        if (out_x) *out_x = trans.x;
        if (out_y) *out_y = trans.y;
        if (out_z) *out_z = trans.z;
        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 get_translation failed: ") + e.what();
    }
}

void orkid_fmtx4_set_translation(OrkidHandleBase* handle, float x, float y, float z) {
    if (!handle) {
        g_last_error = "Null fmtx4 handle in orkid_fmtx4_set_translation";
        return;
    }

    auto typed = handle->typedHandle<fmtx4>();
    if (!typed) {
        g_last_error = "Invalid fmtx4 handle type";
        return;
    }

    try {
        typed->get()->setTranslation(x, y, z);
        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 set_translation failed: ") + e.what();
    }
}

OrkidHandleBase* orkid_fmtx4_multiply(const OrkidHandleBase* a, const OrkidHandleBase* b) {
    if (!a || !b) {
        g_last_error = "Null fmtx4 handle in orkid_fmtx4_multiply";
        return nullptr;
    }

    auto typed_a = const_cast<OrkidHandleBase*>(a)->typedHandle<fmtx4>();
    auto typed_b = const_cast<OrkidHandleBase*>(b)->typedHandle<fmtx4>();

    if (!typed_a || !typed_b) {
        g_last_error = "Invalid fmtx4 handle type in orkid_fmtx4_multiply";
        return nullptr;
    }

    try {
        fmtx4 result = typed_a->get()->multiply_rtol(*typed_b->get());
        auto handle = OrkidHandle<fmtx4>::makeShared();
        *handle->get() = result;
        g_last_error.clear();
        return handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 multiply failed: ") + e.what();
        return nullptr;
    }
}

OrkidHandleBase* orkid_fmtx4_inverse(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fmtx4 handle in orkid_fmtx4_inverse";
        return nullptr;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fmtx4>();
    if (!typed) {
        g_last_error = "Invalid fmtx4 handle type";
        return nullptr;
    }

    try {
        fmtx4 result = typed->get()->inverse();
        auto result_handle = OrkidHandle<fmtx4>::makeShared();
        *result_handle->get() = result;
        g_last_error.clear();
        return result_handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 inverse failed: ") + e.what();
        return nullptr;
    }
}

OrkidHandleBase* orkid_fmtx4_transpose(const OrkidHandleBase* handle) {
    if (!handle) {
        g_last_error = "Null fmtx4 handle in orkid_fmtx4_transpose";
        return nullptr;
    }

    auto typed = const_cast<OrkidHandleBase*>(handle)->typedHandle<fmtx4>();
    if (!typed) {
        g_last_error = "Invalid fmtx4 handle type";
        return nullptr;
    }

    try {
        fmtx4 result = typed->get()->transposed();
        auto result_handle = OrkidHandle<fmtx4>::makeShared();
        *result_handle->get() = result;
        g_last_error.clear();
        return result_handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 transpose failed: ") + e.what();
        return nullptr;
    }
}

OrkidHandleBase* orkid_fmtx4_transform_vec4(const OrkidHandleBase* mtx, const OrkidHandleBase* vec) {
    if (!mtx || !vec) {
        g_last_error = "Null handle in orkid_fmtx4_transform_vec4";
        return nullptr;
    }

    auto typed_mtx = const_cast<OrkidHandleBase*>(mtx)->typedHandle<fmtx4>();
    auto typed_vec = const_cast<OrkidHandleBase*>(vec)->typedHandle<fvec4>();

    if (!typed_mtx || !typed_vec) {
        g_last_error = "Invalid handle type in orkid_fmtx4_transform_vec4";
        return nullptr;
    }

    try {
        fvec4 result = typed_vec->get()->transform(*typed_mtx->get());
        auto result_handle = OrkidHandle<fvec4>::makeShared(result.x, result.y, result.z, result.w);
        g_last_error.clear();
        return result_handle;
    } catch (const std::exception& e) {
        g_last_error = std::string("fmtx4 transform_vec4 failed: ") + e.what();
        return nullptr;
    }
}

} // extern "C"
