////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/core/singleton.h>
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
	int _image_fd = 0;
  int _image_width = 0;
  int _image_height = 0;
  size_t _image_size = 0;
	int _sema_complete_fd = 0;
	int _sema_ready_fd = 0;
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
  TextureSamplingModeData()
      : mTexAddrModeU(TextureAddressMode::WRAP)
      , mTexAddrModeV(TextureAddressMode::WRAP)
      , mTexFiltModeMin(ETEXFILT_POINT)
      , mTexFiltModeMag(ETEXFILT_POINT)
      , mTexFiltModeMip(ETEXFILT_POINT) {
  }

  TextureAddressMode GetAddrModeU() const {
    return mTexAddrModeU;
  }
  TextureAddressMode GetAddrModeV() const {
    return mTexAddrModeV;
  }
  ETextureFilterMode GetFiltModeMin() const {
    return mTexFiltModeMin;
  }
  ETextureFilterMode GetFiltModeMag() const {
    return mTexFiltModeMag;
  }
  ETextureFilterMode GetFiltModeMip() const {
    return mTexFiltModeMip;
  }

  TextureAddressMode mTexAddrModeU;
  TextureAddressMode mTexAddrModeV;
  ETextureFilterMode mTexFiltModeMin;
  ETextureFilterMode mTexFiltModeMag;
  ETextureFilterMode mTexFiltModeMip;

  void PresetPointAndClamp();
  void PresetTrilinearWrap();
};

//////////////////////////////////////////////////////////////////////////

class TextureAnimationBase {
public:
  virtual void UpdateTexture(TextureInterface* txi, Texture* ptex, TextureAnimationInst* ptexanim) = 0;
  virtual float GetLengthOfTime(void) const                                                        = 0;
  virtual ~TextureAnimationBase() {
  }

private:
};

//////////////////////////////////////////////////////////////////////////

class TextureAnimationInst {
public:
  TextureAnimationInst(TextureAnimationBase* panim = 0)
      : mfCurrentTime(0.0f)
      , mpAnim(panim) {
  }
  float GetCurrentTime() const {
    return mfCurrentTime;
  }
  void SetCurrentTime(float fv) {
    mfCurrentTime = fv;
  }
  TextureAnimationBase* GetAnim() const {
    return mpAnim;
  }

private:
  float mfCurrentTime;
  TextureAnimationBase* mpAnim;
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

  TextureAnimationBase* GetTexAnim() const {
    return _anim;
  }
  void SetTexAnim(TextureAnimationBase* ptexanim) {
    _anim = ptexanim;
  }

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

  int _width                    = 0;
  int _height                   = 0;
  int _depth                    = 0;
  int _num_mips                 = 0;
  MsaaSamples _msaa_samples     = MsaaSamples::MSAA_1X;
  uint64_t _flags               = 0;
  uint64_t _contentHash         = 0;
  MipChain* _chain              = nullptr;
  mutable bool _dirty           = true;
  const void* _data             = nullptr;
  TextureAnimationBase* _anim   = nullptr;
  mutable svar64_t _impl        = nullptr;
  Context* _creatingTarget      = nullptr;
  std::string _debugName;
  bool _isDepthTexture = false;
  varmap::varmap_ptr_t _vars;
  const TextureAsset* _asset = nullptr;
  bool _formatSupportsFiltering = true;
  ipctexture_ptr_t _external_memory;
  std::atomic<int> _residenceState;
  datablock_ptr_t _final_datablock;
  std::vector<image_ptr_t> _images;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
