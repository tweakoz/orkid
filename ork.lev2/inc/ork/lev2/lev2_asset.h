////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// level2 assets
///////////////////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/asset/Asset.h>
#include <ork/asset/AssetManager.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxanim.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

class TextureAsset : public ork::asset::Asset {
  DeclareConcreteX(TextureAsset, ork::asset::Asset);

  static const char* assetTypeNameStatic(void) {
    return "lev2tex";
  }

  ork::atomic<Texture*> mData;

public: //
  TextureAsset();
  ~TextureAsset() override;

  Texture* GetTexture() const {
    return mData;
  }
  void SetTexture(Texture* pt);
};

typedef TextureAsset* textureassetptr_t; // prep for shared_ptr

///////////////////////////////////////////////////////////////////////////////

struct XgmModelAsset : public ork::asset::Asset {
  DeclareConcreteX(XgmModelAsset, ork::asset::Asset);

public:
  static const char* assetTypeNameStatic(void) {
    return "xgmodel";
  }

  XgmModelAsset() {
    _model.atomicWrite(std::make_shared<XgmModel>());
  }
  ~XgmModelAsset() override;

  void clearModel() {
    _model.atomicWrite(std::make_shared<XgmModel>());
  }

  XgmModel* GetModel() { // todo unsafe
    return _model.atomicCopy().get();
  }
  const XgmModel* GetModel() const { // todo unsafe
    return _model.atomicCopy().get();
  }

  LockedResource<model_ptr_t> _model;
};

typedef XgmModelAsset* xgmmodelassetptr_t; // prep for shared_ptr

///////////////////////////////////////////////////////////////////////////////

class XgmAnimAsset : public ork::asset::Asset {
  DeclareConcreteX(XgmAnimAsset, ork::asset::Asset);
  static const char* assetTypeNameStatic(void) {
    return "xganim";
  }
  XgmAnim mData;

public: //
  XgmAnim* GetAnim() {
    return &mData;
  }
};

typedef XgmAnimAsset* xgmanimassetptr_t; // prep for shared_ptr

///////////////////////////////////////////////////////////////////////////////

class FxShaderAsset : public ork::asset::Asset {
  DeclareConcreteX(FxShaderAsset, ork::asset::Asset);
  static const char* assetTypeNameStatic(void) {
    return "fxshader";
  }
  FxShader mData;

public: //
  FxShader* GetFxShader() {
    return &mData;
  }
};

typedef FxShaderAsset* fxshaderassetptr_t; // prep for shared_ptr

///////////////////////////////////////////////////////////////////////////////
void autoloadAssets(bool wait);
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
