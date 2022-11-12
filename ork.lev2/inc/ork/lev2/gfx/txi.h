////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

  size_t computeSize() const;

  int _w                = 0;
  int _h                = 0;
  int _d                = 1;
  EBufferFormat _format = EBufferFormat::NONE;
  bool _autogenmips     = false;
  const void* _data     = nullptr;
  size_t _truncation_length = 0;
};

class TextureInterface {
public:
  virtual void TexManInit(void) = 0;

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
