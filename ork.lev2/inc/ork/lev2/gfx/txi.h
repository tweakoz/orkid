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

  virtual void ApplySamplingMode(Texture* ptex) {
  }

  virtual void initTextureFromData(Texture* ptex, TextureInitData tid) {
  }
  void initTextureFromImage(Texture* ptex, image_ptr_t img, bool autogenmips = false );
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
