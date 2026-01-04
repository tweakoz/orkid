////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// Linux stub for external surface texture import (DMA-BUF)
// Not yet implemented - see vulkan_txi_from_external.mm for macOS IOSurface implementation

#if defined(__linux__)

#include "headers/vulkan_ctx.h"

namespace ork::lev2::vulkan {

///////////////////////////////////////////////////////////////////////////////
// GPU-Direct External Memory Import (DMA-BUF on Linux)
// Stub implementation - returns without doing anything
///////////////////////////////////////////////////////////////////////////////

void VkTextureInterface::initTextureFromGpuExternalSurface(Texture* ptex) {
  // TODO: Implement DMA-BUF import via VK_EXT_external_memory_dma_buf
  printf("VkTextureInterface::initTextureFromGpuExternalSurface: DMA-BUF import not yet implemented on Linux\n");
}

///////////////////////////////////////////////////////////////////////////////
// Check if external texture backing changed
///////////////////////////////////////////////////////////////////////////////

bool VkTextureInterface::externalTextureChanged(const Texture* ptex) {
  // TODO: Implement for VA-API/DMA-BUF
  return false;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::vulkan

#endif // __linux__
