#include "vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkMatrixStackInterface::VkMatrixStackInterface(vkcontext_ptr_t ctx)
    : MatrixStackInterface(*ctx.get())
    , _contextVK(ctx) {
}

fmtx4 VkMatrixStackInterface::Ortho(float left, float right, float top, float bottom, float fnear, float ffar) {
  return fmtx4();
}
fmtx4 VkMatrixStackInterface::Frustum(float left, float right, float top, float bottom, float zn, float zf) {
  return fmtx4();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
