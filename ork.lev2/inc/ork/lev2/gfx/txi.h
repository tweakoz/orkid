////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Texture Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

struct MipChainLevel;
struct MipChain;

struct TextureInitData {

  size_t computeSrcSize() const;
  size_t computeDstSize() const;

  int _w                = 0;
  int _h                = 0;
  int _d                = 1;
  EBufferFormat _src_format = EBufferFormat::NONE;
  EBufferFormat _dst_format = EBufferFormat::RGB8;
  bool _autogenmips     = false;
  const void* _data     = nullptr;
  size_t _truncation_length = 0;
  bool _allow_async = false;
};

class TextureInterface {
public:

  virtual void TexManInit(void) = 0;

  texture_ptr_t createColorTexture(fvec4 color, int w, int h);

  virtual bool destroyTexture(texture_ptr_t ptex)                           = 0;
  virtual bool LoadTexture(const AssetPath& fname, texture_ptr_t ptex)      = 0;
  virtual bool LoadTexture(texture_ptr_t ptex, datablock_ptr_t inpdata)      = 0;
  virtual void SaveTexture(const ork::AssetPath& fname, Texture* ptex) = 0;
  virtual void UpdateAnimatedTexture(Texture* ptex, TextureAnimationInst* tai) {
  }
  virtual void ApplySamplingMode(Texture* ptex) {
  }
  virtual void initTextureFromData(Texture* ptex, TextureInitData tid) {
  }
  virtual Texture* createFromMipChain(MipChain* from_chain) {
    return nullptr;
  }
  virtual void generateMipMaps(Texture* ptex) = 0;
};
