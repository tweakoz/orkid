////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2021, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// level2 assets
///////////////////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/lev2_types.h>
#include <ork/asset/Asset.h>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/AssetManager.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxanim.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

struct TextureAsset : public ork::asset::Asset {
  DeclareConcreteX(TextureAsset, ork::asset::Asset);

  static const char* assetTypeNameStatic(void) {
    return "lev2tex";
  }

public: //
  TextureAsset();
  ~TextureAsset() override;

  texture_ptr_t GetTexture() const {
    return _texture;
  }
  void SetTexture(texture_ptr_t pt);
  texture_ptr_t _texture;
};

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
  model_ptr_t getSharedModel() { // todo unsafe
    return _model.atomicCopy();
  }
  const XgmModel* GetModel() const { // todo unsafe
    return _model.atomicCopy().get();
  }

  LockedResource<model_ptr_t> _model;
};

struct XgmModelLoader final : public ork::asset::FileAssetLoader {
public:
  XgmModelLoader();
  ork::asset::asset_ptr_t _doLoadAsset(AssetPath assetpath, ork::asset::vars_constptr_t vars) override;
  void destroy(ork::asset::asset_ptr_t asset) override;
  void initLoadersForUriProto(const std::string& uriproto) override;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmAnimAsset : public ork::asset::Asset {
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

///////////////////////////////////////////////////////////////////////////////

struct FxShaderAsset : public ork::asset::Asset {
  DeclareConcreteX(FxShaderAsset, ork::asset::Asset);
  static const char* assetTypeNameStatic(void) {
    return "fxshader";
  }

public: //
  FxShaderAsset();
  ~FxShaderAsset();
  FxShader* GetFxShader() const {
    return _shader;
  }
  FxShader* _shader;
};

///////////////////////////////////////////////////////////////////////////////
void autoloadAssets(bool wait);
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
