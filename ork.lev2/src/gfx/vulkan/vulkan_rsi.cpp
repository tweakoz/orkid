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

  VkRasterStateInterface::VkRasterStateInterface(vkcontext_rawptr_t ctx)
      : RasterStateInterface(*ctx)
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