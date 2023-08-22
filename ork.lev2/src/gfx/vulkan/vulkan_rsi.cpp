#include "vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

  VkRasterStateInterface::VkRasterStateInterface(vkcontext_ptr_t ctx)
      : RasterStateInterface(*ctx.get())
      , _contextVK(ctx) {
  }
  void VkRasterStateInterface::BindRasterState(const SRasterState& rState, bool bForce) {
  }

  void VkRasterStateInterface::SetZWriteMask(bool bv) {
  }
  void VkRasterStateInterface::SetRGBAWriteMask(bool rgb, bool a) {
  }
  RGBAMask VkRasterStateInterface::SetRGBAWriteMask(const RGBAMask& newmask) {
    return RGBAMask();
  }
  void VkRasterStateInterface::SetBlending(Blending eVal) {
  }
  void VkRasterStateInterface::SetDepthTest(EDepthTest eVal) {
  }
  void VkRasterStateInterface::SetCullTest(ECullTest eVal) {
  }
  void VkRasterStateInterface::setScissorTest(EScissorTest eVal) {
  }
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
