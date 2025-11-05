////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#ifdef __cplusplus
namespace ork::swift { class OrkidHandleBase; }
typedef ork::swift::OrkidHandleBase OrkidHandleBase;
extern "C" {
#else
typedef struct OrkidHandleBase OrkidHandleBase;
#endif

// ================================================================
// Math - vec3
// ================================================================

OrkidHandleBase* orkid_fvec3_create(float x, float y, float z);
float orkid_fvec3_get_x(const OrkidHandleBase* handle);
float orkid_fvec3_get_y(const OrkidHandleBase* handle);
float orkid_fvec3_get_z(const OrkidHandleBase* handle);
void orkid_fvec3_set_x(OrkidHandleBase* handle, float value);
void orkid_fvec3_set_y(OrkidHandleBase* handle, float value);
void orkid_fvec3_set_z(OrkidHandleBase* handle, float value);
float orkid_fvec3_length(const OrkidHandleBase* handle);
OrkidHandleBase* orkid_fvec3_normalized(const OrkidHandleBase* handle);

// ================================================================
// Math - vec4
// ================================================================

OrkidHandleBase* orkid_fvec4_create(float x, float y, float z, float w);
float orkid_fvec4_get_x(const OrkidHandleBase* handle);
float orkid_fvec4_get_y(const OrkidHandleBase* handle);
float orkid_fvec4_get_z(const OrkidHandleBase* handle);
float orkid_fvec4_get_w(const OrkidHandleBase* handle);
void orkid_fvec4_set_x(OrkidHandleBase* handle, float value);
void orkid_fvec4_set_y(OrkidHandleBase* handle, float value);
void orkid_fvec4_set_z(OrkidHandleBase* handle, float value);
void orkid_fvec4_set_w(OrkidHandleBase* handle, float value);
float orkid_fvec4_length(const OrkidHandleBase* handle);
OrkidHandleBase* orkid_fvec4_normalized(const OrkidHandleBase* handle);
float orkid_fvec4_dot(const OrkidHandleBase* a, const OrkidHandleBase* b);

// ================================================================
// Math - mat4 (Matrix44)
// ================================================================

OrkidHandleBase* orkid_fmtx4_create_identity(void);
OrkidHandleBase* orkid_fmtx4_create_translation(float x, float y, float z);
OrkidHandleBase* orkid_fmtx4_create_scale(float x, float y, float z);
OrkidHandleBase* orkid_fmtx4_create_rotation_x(float radians);
OrkidHandleBase* orkid_fmtx4_create_rotation_y(float radians);
OrkidHandleBase* orkid_fmtx4_create_rotation_z(float radians);

void orkid_fmtx4_get_translation(const OrkidHandleBase* handle, float* out_x, float* out_y, float* out_z);
void orkid_fmtx4_set_translation(OrkidHandleBase* handle, float x, float y, float z);

OrkidHandleBase* orkid_fmtx4_multiply(const OrkidHandleBase* a, const OrkidHandleBase* b);
OrkidHandleBase* orkid_fmtx4_inverse(const OrkidHandleBase* handle);
OrkidHandleBase* orkid_fmtx4_transpose(const OrkidHandleBase* handle);

OrkidHandleBase* orkid_fmtx4_transform_vec4(const OrkidHandleBase* mtx, const OrkidHandleBase* vec);

#ifdef __cplusplus
}
#endif
