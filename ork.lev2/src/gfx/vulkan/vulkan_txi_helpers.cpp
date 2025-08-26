////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/lev2/gfx/texman.h>

#if defined(ENABLE_VULKAN)

namespace ork::lev2::vulkan {

////////////////////////////////////////////////////////////////////////////////
// Platform-specific format conversion helper
// Converts 24-bit formats to 32-bit equivalents on macOS/Metal
////////////////////////////////////////////////////////////////////////////////

EBufferFormat VkTextureInterface::convertFormatForPlatform(EBufferFormat format) {
#if defined(__APPLE__)
  switch(format) {
    case EBufferFormat::BGR8:
      return EBufferFormat::BGRA8;
    case EBufferFormat::RGB8:
      return EBufferFormat::RGBA8;
    case EBufferFormat::RGB32F:
      return EBufferFormat::RGBA32F;
    case EBufferFormat::RGB16:
      return EBufferFormat::RGBA16;
    default:
      return format;
  }
#else
  return format;
#endif
}

} // namespace ork::lev2::vulkan

#endif // ENABLE_VULKAN