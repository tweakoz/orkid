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
  EBufferFormat _format = EBufferFormat::NONE;
  bool _autogenmips     = false;
  const void* _data     = nullptr;
};

class TextureInterface {
public:
  virtual void TexManInit(void) = 0;

  virtual bool DestroyTexture(Texture* ptex)                           = 0;
  virtual bool LoadTexture(const AssetPath& fname, Texture* ptex)      = 0;
  virtual bool LoadTexture(Texture* ptex, datablockptr_t inpdata)      = 0;
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
