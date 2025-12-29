////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2025, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#if defined(__APPLE__)

// Include Apple headers FIRST (before any namespace declarations)
#import <Foundation/Foundation.h>
#import <IOSurface/IOSurface.h>
#import <CoreVideo/CoreVideo.h>

// Now include our headers
#include "vk_gpu_surface.h"
#include <ork/lev2/gfx/texman.h>  // Full GpuExternalSurface definition

namespace ork::lev2::vulkan {

//////////////////////////////////////////////////////////////////////////////
// IoSurfaceTexImpl accessor implementations
//////////////////////////////////////////////////////////////////////////////

size_t IoSurfaceTexImpl::width() const { return surface ? surface->width() : 0; }
size_t IoSurfaceTexImpl::height() const { return surface ? surface->height() : 0; }
uint32_t IoSurfaceTexImpl::pixel_format() const { return surface ? surface->nativeFormat() : 0; }
uint64_t IoSurfaceTexImpl::uniqueId() const { return surface ? surface->uniqueId() : 0; }

//////////////////////////////////////////////////////////////////////////////
// GpuExternalSurface_IOSurface - macOS IOSurface implementation
//////////////////////////////////////////////////////////////////////////////

class GpuExternalSurface_IOSurface final : public GpuExternalSurface {
public:

  // From CVPixelBuffer (common from VideoToolbox)
  // NOTE: Does NOT retain - caller manages lifetime via pending.pixel_buffer
  explicit GpuExternalSurface_IOSurface(CVPixelBufferRef pixelBuffer)
  {
    if (pixelBuffer) {
      // No retain - this is a lightweight wrapper, caller owns the CVPixelBuffer
      IOSurfaceRef surface = CVPixelBufferGetIOSurface(pixelBuffer);
      _impl.set<NativeSurfaceHandle>(NativeSurfaceHandle{(void*)surface});
      _cacheProperties(surface);
    }
  }

  ~GpuExternalSurface_IOSurface() override = default;

  // Non-copyable
  GpuExternalSurface_IOSurface(const GpuExternalSurface_IOSurface&) = delete;
  GpuExternalSurface_IOSurface& operator=(const GpuExternalSurface_IOSurface&) = delete;

  //--------------------------------------------------------------------------
  // GpuExternalSurface interface
  //--------------------------------------------------------------------------

  GpuSurfacePlatformType platformType() const override {
    return GpuSurfacePlatformType::IOSURFACE;
  }

  uint64_t uniqueId() const override {
    auto handle_opt = _impl.tryAs<NativeSurfaceHandle>();
    if (handle_opt && handle_opt.value().handle) {
      return IOSurfaceGetID((IOSurfaceRef)handle_opt.value().handle);
    }
    return 0;
  }

  size_t width() const override { return _width; }
  size_t height() const override { return _height; }

  GpuSurfaceFormat format() const override { return _format; }
  uint32_t nativeFormat() const override { return _native_format; }
  size_t planeCount() const override { return _plane_count; }

private:

  void _cacheProperties(IOSurfaceRef surface) {
    if (!surface) return;

    _width = IOSurfaceGetWidth(surface);
    _height = IOSurfaceGetHeight(surface);
    _plane_count = IOSurfaceGetPlaneCount(surface);
    if (_plane_count == 0) _plane_count = 1;

    _native_format = (uint32_t)IOSurfaceGetPixelFormat(surface);
    _format = _mapFormat(_native_format);
  }

  GpuSurfaceFormat _mapFormat(uint32_t osType) const {
    switch (osType) {
      case kCVPixelFormatType_32BGRA:
        return GpuSurfaceFormat::BGRA8;
      case kCVPixelFormatType_32RGBA:
        return GpuSurfaceFormat::RGBA8;
      case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
      case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
        return GpuSurfaceFormat::NV12;
      case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
      case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
        return GpuSurfaceFormat::P010;
      default:
        return GpuSurfaceFormat::NATIVE;
    }
  }

  size_t _width = 0;
  size_t _height = 0;
  size_t _plane_count = 1;
  uint32_t _native_format = 0;
  GpuSurfaceFormat _format = GpuSurfaceFormat::UNKNOWN;
};

//////////////////////////////////////////////////////////////////////////////
// Factory function
//////////////////////////////////////////////////////////////////////////////

gpu_external_surface_ptr_t createGpuSurfaceFromCVPixelBuffer(CVPixelBufferRef pixelBuffer) {
  if (!pixelBuffer) return nullptr;
  return std::make_shared<GpuExternalSurface_IOSurface>(pixelBuffer);
}

} // namespace ork::lev2::vulkan

#endif // __APPLE__
