////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/core/singleton.h>
#include <ork/kernel/svariant.h>
#include <ork/util/md5.h>
#include <ork/math/cvector4.h>
#include <ork/file/path.h>
#include <ork/kernel/kernel.h>
#include <ork/kernel/datablock.h>
#include <ork/asset/Asset.h>

namespace ork { namespace lev2 {

void invoke_nvcompress(std::string inpath, std::string outpath, std::string other_args);

//////////////////////////////////////////////////////////////////////////
// External IPC Texture support
//
// VK: External Memory
// GL: EXT_external_objects
// GL: EXT_external_objects_fd
//////////////////////////////////////////////////////////////////////////

struct IpcTexture {
  int _image_fd         = 0;
  int _image_width      = 0;
  int _image_height     = 0;
  size_t _image_size    = 0;
  int _sema_complete_fd = 0;
  int _sema_ready_fd    = 0;
};

//////////////////////////////////////////////////////////////////////////////
// GpuExternalSurface - Platform-agnostic GPU memory for cross-API/cross-process sharing
//
// Implementations:
//   - IOSurface (macOS): VideoToolbox, Syphon, Core Image, AVFoundation
//   - DMA-BUF (Linux): VA-API, V4L2, GBM, Wayland
//////////////////////////////////////////////////////////////////////////////

// Wrapper for platform-specific native handles (avoids typeid ODR issues)
struct NativeSurfaceHandle {
  void* handle = nullptr;
};

enum class GpuSurfaceFormat : uint32_t {
  UNKNOWN = 0,
  BGRA8,      // 32-bit BGRA (macOS native)
  RGBA8,      // 32-bit RGBA
  NV12,       // 4:2:0 8-bit, 2 planes (Y + interleaved UV)
  P010,       // 4:2:0 10-bit, 2 planes
  NATIVE,     // Platform-specific passthrough
};

enum class GpuSurfacePlatformType : uint8_t {
  UNKNOWN = 0,
  IOSURFACE,     // macOS IOSurface
  DMABUF,        // Linux DMA-BUF
};

struct GpuExternalSurface {
  virtual ~GpuExternalSurface() = default;

  // Platform type (for Vulkan import path selection)
  virtual GpuSurfacePlatformType platformType() const = 0;

  // Unique identifier for this surface (for VkImage reuse tracking)
  virtual uint64_t uniqueId() const = 0;

  // Dimensions
  virtual size_t width() const = 0;
  virtual size_t height() const = 0;

  // Format
  virtual GpuSurfaceFormat format() const = 0;
  virtual uint32_t nativeFormat() const = 0;
  virtual size_t planeCount() const = 0;

  // Platform-specific native handle (IOSurfaceRef, DMA-BUF fd, etc.)
  // Use _impl.makeShared<T>() to set, _impl.getShared<T>() to get
  svar16_t _impl;
};

//////////////////////////////////////////////////////////////////////////

enum ETextureUsage {
  ETEXUSAGE_COLOR = 0,
  ETEXUSAGE_COLOR_NC,
  ETEXUSAGE_GREYSCALE,
  ETEXUSAGE_NORMAL,
  ETEXUSAGE_DATA,
};

//////////////////////////////////////////////////////////////////////////

struct TextureSamplingModeData {

  void presetPointAndClamp();
  void presetTrilinearWrap();
  void presetTrilinearClamp();

  // STR, huh?
  TextureAddressMode _texAddrModeS          = TextureAddressMode::WRAP;
  TextureAddressMode _texAddrModeT          = TextureAddressMode::WRAP;
  TextureAddressMode _texAddrModeR          = TextureAddressMode::WRAP;
  ETextureMinifyFilterMode _texFiltModeMin  = ETextureMinifyFilterMode::LINEAR;
  ETextureMagnifyFilterMode _texFiltModeMag = ETextureMagnifyFilterMode::LINEAR;
  float _maxAnisotropy                      = 16.0f;
  int _maxMipLevel                          = 8;
};

//////////////////////////////////////////////////////////////////////////

struct MipChainLevel {

  template <typename T> T& sample(int x, int y) {
    auto base = (T*)_data;
    assert(x < _width);
    assert(y < _height);
    size_t index = y * _width + x;
    assert((index * sizeof(T)) < _length);
    return base[index];
  }

  int _width     = 0;
  int _height    = 0;
  size_t _length = 0;
  void* _data    = nullptr;
};

//////////////////////////////////////////////////////////////////////////

struct MipChain {
  MipChain(int w, int h, EBufferFormat fmt, ETextureType typ);
  ~MipChain();

  typedef std::shared_ptr<MipChainLevel> mipchainlevel_t;
  std::vector<mipchainlevel_t> _levels;
  int _width  = 0;
  int _height = 0;
  std::string _debugName;

  EBufferFormat _format = EBufferFormat::NONE;
  ETextureType _type    = ETEXTYPE_END;
};

//////////////////////////////////////////////////////////////////////////

struct Texture {
  typedef std::function<datablock_ptr_t(texture_ptr_t, Context*, datablock_constptr_t)> proc_t;

  static std::atomic<size_t> _texture_count;

  //////////////////////////////////////////////////////

  Texture(const TextureAsset* asset = nullptr);
  Texture(ipctexture_ptr_t external_memory);

  ~Texture();

  //////////////////////////////////////////////////////

  bool IsVolumeTexture(void) const {
    return (_depth > 1);
  }
  bool IsDirty(void) const {
    return _dirty;
  }

  ETextureType GetTexType(void) const {
    return _texType;
  }
  ETextureDest GetTexDest(void) const {
    return _texDest;
  }

  //////////////////////////////////////////////////////

  Md5Sum GetMd5Sum(void) const {
    return mMd5Sum;
  }
  void SetMd5Sum(Md5Sum sum) {
    mMd5Sum = sum;
  }

  const TextureSamplingModeData& TexSamplingMode() const {
    return mTexSampleMode;
  }
  TextureSamplingModeData& TexSamplingMode() {
    return mTexSampleMode;
  }

  //////////////////////////////////////////////////////

  static texture_ptr_t LoadUnManaged(const AssetPath& fname);
  static texture_ptr_t createBlank(int iw, int ih, EBufferFormat efmt);

  //////////////////////////////////////////////////////////
  asset::loadrequest_ptr_t loadRequest() const;
  //////////////////////////////////////////////////////////

  static void RegisterLoaders(void);

  Md5Sum mMd5Sum; // for dirty checking (mipgen/palettegen)
  int miTotalUniqueColors;
  int miMaxMipUniqueColors;

  TextureSamplingModeData mTexSampleMode;

  ETextureDest _texDest    = ETEXDEST_END;
  ETextureType _texType    = ETEXTYPE_END;
  EBufferFormat _texFormat = EBufferFormat::NONE;

  int _width                  = 0;
  int _height                 = 0;
  int _depth                  = 0;
  int _num_mips               = 0;
  MsaaSamples _msaa_samples   = MsaaSamples::MSAA_1X;
  uint64_t _flags             = 0;
  uint64_t _contentHash       = 0;
  MipChain* _chain            = nullptr;
  mutable bool _dirty         = true;
  const void* _data           = nullptr;
  mutable svarshp_t _impl     = nullptr;

  // External memory backend implementation (e.g., Vulkan manages IOSurfaces internally)
  // Backend-specific impl manages buffering/synchronization as needed
  mutable svar16_t _impl_2;  // Backend implementation (e.g., VulkanExternalTextureImpl)

  Context* _creatingTarget    = nullptr;
  std::string _debugName;
  bool _isDepthTexture = false;
  ETextureSource _source = ETextureSource::NONE;
  texture_provider_ptr_t _update_provider;  // For MOVIE textures: poll to trigger frame updates
  varmap::varmap_ptr_t _vars;
  const TextureAsset* _asset    = nullptr;
  bool _formatSupportsFiltering = true;
  ipctexture_ptr_t _external_memory;
  std::atomic<int> _residenceState;
  datablock_ptr_t _final_datablock;
};

///////////////////////////////////////////////////////////////////////////////
struct TextureArray {

  TextureArray();
  ~TextureArray();
  texturearraysliceref_ptr_t load(const std::string& path);
  void resize(size_t w, size_t h, size_t maxslices,EBufferFormat efmt);
  texturearraysliceref_ptr_t slice(size_t index) const;
  void _conform(EBufferFormat fmt);
  size_t _width = 0;
  size_t _height = 0;
  size_t _maxslices = 0;
  int _num_mips = 1;
  bool _requires_mips = false;
  bool _needsRadianceCache = false;
  EBufferFormat _format = EBufferFormat::RGB8;
  std::map<std::string,size_t> _slices_by_path;
  texture_ptr_t _tex;
  varmap::varmap_ptr_t _vars;
  std::set<size_t> _free_slices;
  mutable std::set<size_t> _dirty_slices;
  std::unordered_map<size_t,image_ptr_t> _images;
  rtgroup_ptr_t _rtg;
  std::string _debugName;
  bool _isDirty = true;
  bool _gpuInitialized = false;

};

struct TextureArraySliceRef {
  TextureArraySliceRef(TextureArray* ary, int slice);
  rtgroup_ptr_t createRenderTarget(Context* ctx);
  TextureArray* _array;
  int _slice = 0;
};

struct TextureProvider {
  virtual texture_ptr_t getTexture() = 0;
  virtual ~TextureProvider() = 0;
};

struct LambdaTextureProvider : public TextureProvider {
  LambdaTextureProvider(std::function<texture_ptr_t()> func);
  texture_ptr_t getTexture() final;
  std::function<texture_ptr_t()> _func;
};
}} // namespace ork::lev2
