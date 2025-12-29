////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2025, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/lev2_types.h>  // gpu_external_surface_ptr_t, GpuExternalSurface forward decl
#include <memory>

namespace ork::lev2::vulkan {

//////////////////////////////////////////////////////////////////////////////
// IoSurfaceTexImpl - Wraps GpuExternalSurface with associated VkImage
// 1:1 mapping: IOSurface ↔ VkImage ↔ VkImageView
//////////////////////////////////////////////////////////////////////////////

struct IoSurfaceTexImpl {
  // Platform-agnostic surface wrapper (provides uniqueId, dimensions, _impl)
  gpu_external_surface_ptr_t surface;

  // Vulkan objects (opaque pointers - actual types in Vulkan code)
  std::shared_ptr<void> vkimage;      // vkimageobj_ptr_t (VkImage + VkImageView wrapper)
  std::shared_ptr<void> vkdescriptor; // Descriptor info for this image

  // Accessors via GpuExternalSurface
  size_t width() const;
  size_t height() const;
  uint32_t pixel_format() const;
  uint64_t uniqueId() const;

  ~IoSurfaceTexImpl() {
    // VkImage destroyed when vkimage shared_ptr goes to 0
    // Surface destroyed when surface shared_ptr goes to 0
  }
};

using iosurfaceteximpl_ptr_t = std::shared_ptr<IoSurfaceTexImpl>;

} // namespace ork::lev2::vulkan
