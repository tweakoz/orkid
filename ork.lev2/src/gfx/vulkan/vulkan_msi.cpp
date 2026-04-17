////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkMatrixStackInterface::VkMatrixStackInterface(vkcontext_rawptr_t ctx)
    : MatrixStackInterface(*ctx)
    , _contextVK(ctx) {
}

// Standard GL/RH orthographic projection. The Vulkan Y-flip now lives at the
// rasterizer (negative-height viewport) — see FLIP_Y_LIKE_OPENGL in
// vulkan_ctx.h. Baking a Y-flip here was a leftover from the old scheme and
// caused upside-down UIs (UIs use PushUIMatrix → this Ortho).
fmtx4 VkMatrixStackInterface::Ortho(float left, float right, float top, float bottom, float fnear, float ffar) {
  fmtx4 o;
  o.setElemXY(0,0, 2.0f / (right - left));
  o.setElemXY(1,1, 2.0f / (top - bottom));
  o.setElemXY(2,2, 1.0f / (fnear - ffar));
  o.setElemXY(3,0, (left + right) / (left - right));
  o.setElemXY(3,1, (top + bottom) / (bottom - top));
  o.setElemXY(3,2, fnear / (fnear - ffar));
  return o;
}

// Standard GL/RH frustum — Y-flip owned by the rasterizer (see above).
fmtx4 VkMatrixStackInterface::Frustum(float left, float right, float top, float bottom, float zn, float zf) {
  fmtx4 rval;
  rval.setToIdentity();

  const float two_near_dist = 2.0f * zn;
  const float right_minus_left = right - left;
  const float far_minus_near = zf - zn;

  const float m00 = two_near_dist / right_minus_left;
  const float m02 = (right + left) / right_minus_left;
  const float m11 = two_near_dist / (top - bottom);
  const float m12 = (top + bottom) / (top - bottom);
  const float m22 = -zf / far_minus_near;                  // Vulkan [0,1] depth range
  const float m23 = -(zf * zn) / far_minus_near;
  const float m32 = -1.0f;

  rval.setRow(0, fvec4(m00, 0.0f, m02, 0.0f) );
  rval.setRow(1, fvec4(0.0f, m11, m12, 0.0f) );
  rval.setRow(2, fvec4(0.0f, 0.0f, m22, m23) );
  rval.setRow(3, fvec4(0.0f, 0.0f, m32, 0.0f) );
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
