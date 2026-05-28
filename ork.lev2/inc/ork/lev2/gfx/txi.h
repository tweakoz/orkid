////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/path.h>
#include <ork/gfx/dds.h>

namespace ork::lev2 {
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Texture Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

struct MipChainLevel;
struct MipChain;
class ShmTexConsumer;

struct TextureArrayInitSubItem {
  uint32_t _usage = 0;
  // texture_ptr_t _subtex;
  image_ptr_t _subimg;
  compressedmipchain_ptr_t _cmipchain;
};
struct TextureArrayInitData {
  std::vector<TextureArrayInitSubItem> _slices;
};
struct TextureInitData {

  size_t computeSrcSize() const;
  size_t computeDstSize() const;

  int _w                                = 0;
  int _h                                = 0;
  int _d                                = 1;
  bool _initCubeTexture                 = false;
  EBufferFormat _src_format             = EBufferFormat::NONE;
  EBufferFormat _dst_format             = EBufferFormat::RGB8;
  bool _autogenmips                     = false;
  const void* _data                     = nullptr;
  size_t _truncation_length             = 0;
  bool _allow_async                     = true;
  TextureSamplingModeData               _samplingMode;
};

struct TexLoadReq {
  texture_ptr_t ptex;
  const dds::DDS_HEADER* _ddsheader = nullptr;
  svar16_t _impl;
  std::string _texname;
  DataBlockInputStream _inpstream;
  compressedmipchain_ptr_t _cmipchain;
  asset::loadrequest_ptr_t _assetloadreq;
};

using texloadreq_ptr_t = std::shared_ptr<TexLoadReq>;

// Chunked-upload region descriptor. Used by uploadTextureRegion.
// Data is expected to be in the texture's destination format already;
// callers convert (e.g., RGB→RGBA) before the upload step. uploadTextureRegion
// asserts that data_size == extent_w × extent_h × extent_d × bytesPerPixel(fmt).
struct TextureRegionUpload {
  int _mip_level     = 0;   // which mip level
  int _array_layer   = 0;   // for arrays / cubes, which slice (0 for plain 2D)
  int _offset_x      = 0;   // pixel offset within the mip
  int _offset_y      = 0;
  int _offset_z      = 0;   // for 3D textures
  int _extent_w      = 0;   // pixel extent of this region within the mip
  int _extent_h      = 0;
  int _extent_d      = 1;   // for 3D textures (1 for 2D / array slice)
  const void* _data  = nullptr;
  size_t _data_size  = 0;
};

class TextureInterface {
public:
  TextureInterface(context_rawptr_t ctx);

  bool LoadTexture(texture_ptr_t ptex, datablock_ptr_t inpdata);
  bool LoadTexture(const AssetPath& fname, texture_ptr_t ptex);
  void SaveTexture(const AssetPath& fname, Texture* ptex);

  texture_ptr_t createColorTexture(fvec4 color, int w, int h);
  texture_ptr_t createColorTextureV3(fvec3 color, int w, int h);
  texture_ptr_t createColorCubeTexture(fvec4 color, int w, int h);
  texturearray_ptr_t createColorTextureV3Array(fvec3 color, int w, int h, int d);

  bool _loadImageTexture(texture_ptr_t ptex, datablock_ptr_t src_datablock);
  bool _loadXTXTexture(texture_ptr_t ptex, datablock_ptr_t datablock);
  void _loadXTXTextureMainThreadPart(texloadreq_ptr_t req);
  bool _loadDDSTexture(texture_ptr_t ptex, datablock_ptr_t datablock);
  bool _loadDDSTexture(const AssetPath& infname, texture_ptr_t ptex);
  void _loadDDSTextureMainThreadPart(texloadreq_ptr_t req);

  virtual bool destroyTexture(texture_ptr_t ptex) = 0;
  virtual void generateMipMaps(Texture* ptex)     = 0;

  virtual void _createFromLoadReq(texloadreq_ptr_t req) {
  }

  //////////////////////////////////////////////////////////
  // Chunked upload API — reserve GPU storage upfront, fill it via one
  // or more sub-region uploads, then finalize layout for sampling.
  //
  // Usage:
  //   txi->reserveTexture(tex, w, h, num_mips, fmt);   // VkImage + memory + view
  //   for each mip / slice / sub-region:
  //     txi->uploadTextureRegion(tex, region_desc, on_complete);
  //   txi->finalizeUpload(tex);                         // layout → SHADER_READ
  //
  // After reserveTexture: image layout = TRANSFER_DST_OPTIMAL.
  // After uploadTextureRegion: layout stays TRANSFER_DST_OPTIMAL (no
  //   per-region transitions; allows many sub-regions to be filled in
  //   any order without redundant barriers).
  // After finalizeUpload: layout = SHADER_READ_ONLY_OPTIMAL and the
  //   texture is safe to bind/sample.
  //
  // Each region upload returns asynchronously; the optional
  // on_complete fires once the GPU has signaled the upload's
  // completion semaphore.
  //////////////////////////////////////////////////////////

  virtual void reserveTexture(
      Texture* tex,
      int width,
      int height,
      int num_mips,
      EBufferFormat fmt) {}

  virtual void reserveTextureArray(
      TextureArray* tarr,
      int width,
      int height,
      int num_slices,
      int num_mips,
      EBufferFormat fmt) {}

  virtual void uploadTextureRegion(
      Texture* tex,
      const TextureRegionUpload& upload,
      ::ork::void_lambda_t on_complete = nullptr) {}

  virtual void finalizeUpload(
      Texture* tex,
      ::ork::void_lambda_t on_complete = nullptr) {}

  virtual void ApplySamplingMode(Texture* ptex) {
  }

  virtual void initTextureFromData(Texture* ptex, TextureInitData tid) {
  }
  void initTextureFromImage(Texture* ptex, image_ptr_t img, bool autogenmips = false, bool asynch = true );
  virtual void initTextureArray1DFromData(TextureArray* ptex, TextureArrayInitData tid) {
  }
  virtual void initTextureArray2DFromData(TextureArray* ptex, TextureArrayInitData tid) {
  }
  virtual void initTextureArray2D(TextureArray* ptex) {
  }
  virtual void initTextureArray2DAsync(TextureArray* ptex) {
  }
  virtual void initTextureArray3DFromData(TextureArray* ptex, TextureArrayInitData tid) {
  }
  virtual void updateTextureArraySlice(TextureArraySliceRef* slice, image_ptr_t img) {
  }
  virtual void updateTextureArray(TextureArray* array) {
  }
  
  virtual Texture* createFromMipChain(MipChain* from_chain) {
    return nullptr;
  }
#if defined(ENABLE_PYTORCH)
  virtual void initTextureFromTensor(Texture* ptex, torchtensor_ptr_t tensor, EBufferFormat fmt) {
  }
#endif

  // GPU-direct external surface (IOSurface on macOS, DMA-BUF on Linux)
  // Called when _source == MOVIE && _impl_2 contains IoSurfaceTexImpl
  virtual void initTextureFromGpuExternalSurface(Texture* ptex) {
  }

  // Shared memory texture upload from ShmTexConsumer
  // Returns true if a new frame was uploaded to the texture
  virtual bool initFromShm(texture_ptr_t tex, std::shared_ptr<ShmTexConsumer> consumer) {
    return false;  // Default: not supported
  }

  context_rawptr_t _ctx;
};

} // namespace ork::lev2
