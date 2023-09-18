////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkMatrixStackInterface::VkMatrixStackInterface(vkcontext_rawptr_t ctx)
    : MatrixStackInterface(*ctx)
    , _contextVK(ctx) {
}

fmtx4 VkMatrixStackInterface::Ortho(float left, float right, float top, float bottom, float fnear, float ffar) {
  fmtx4 ortho_projection_for_vulkan;
  ortho_projection_for_vulkan[0][0] = 2.0f / (right - left);
  ortho_projection_for_vulkan[1][1] = 2.0f / (top - bottom);
  ortho_projection_for_vulkan[2][2] = 1.0f / (fnear - ffar);
  ortho_projection_for_vulkan[3][0] = (left + right) / (left - right);
  ortho_projection_for_vulkan[3][1] = (top + bottom) / (bottom - top);
  ortho_projection_for_vulkan[3][2] = fnear / (fnear - ffar);
  return ortho_projection_for_vulkan;
}
fmtx4 VkMatrixStackInterface::Frustum(float left, float right, float top, float bottom, float zn, float zf) {
  fmtx4 persp_projection_for_vulkan;
  persp_projection_for_vulkan[0][0] = (2.0f * zn) / (right - left);
  persp_projection_for_vulkan[1][1] = (2.0f * zn) / (top - bottom);
  persp_projection_for_vulkan[2][0] = (right + left) / (right - left);
  persp_projection_for_vulkan[2][1] = (top + bottom) / (top - bottom);
  persp_projection_for_vulkan[2][2] = zf / (zf - zn);
  persp_projection_for_vulkan[2][3] = 1.0f;
  persp_projection_for_vulkan[3][2] = (zf * zn) / (zn - zf);
  persp_projection_for_vulkan[3][3] = 0.0f;
  return persp_projection_for_vulkan;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
