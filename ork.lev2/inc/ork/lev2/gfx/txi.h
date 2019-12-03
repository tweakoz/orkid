#pragma once

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Texture Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

struct MipChainLevel;
struct MipChain;

class TextureInterface {
public:
  virtual void TexManInit(void) = 0;

  virtual bool DestroyTexture(Texture* ptex)                           = 0;
  virtual bool LoadTexture(const AssetPath& fname, Texture* ptex)      = 0;
  virtual bool LoadTexture(Texture* ptex, chunkfile::InputStream& inpstream) = 0;
  virtual void SaveTexture(const ork::AssetPath& fname, Texture* ptex) = 0;
  virtual void UpdateAnimatedTexture(Texture* ptex, TextureAnimationInst* tai) {}
  virtual void ApplySamplingMode(Texture* ptex) {}
  virtual void initTextureFromData(Texture* ptex, bool autogenmips) {}
  virtual Texture* createFromMipChain(MipChain* from_chain) { return nullptr; }
  virtual void generateMipMaps(Texture* ptex) = 0;
};
