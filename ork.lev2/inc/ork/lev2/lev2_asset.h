////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/util/choiceman.h>

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
  int _loadAttempts = 0;
};

struct StaticTexFileLoader final : public ork::asset::FileAssetLoader {
  StaticTexFileLoader();
  ork::asset::asset_ptr_t _doLoadAsset(ork::asset::loadrequest_ptr_t loadreq) final;
  void destroy(ork::asset::asset_ptr_t asset) final;
  void initLoadersForUriProto(const std::string& uriproto) final;
};

///////////////////////////////////////////////////////////////////////////////

class TextureChoices : public ::ork::util::ChoiceList {
public:
  void EnumerateChoices(bool bforcenocache=false) final;
  svar64_t provideSelection(const std::string& key) const final;
  TextureChoices();
};

///////////////////////////////////////////////////////////////////////////////

struct XgmModelAsset final : public ork::asset::Asset {
  DeclareConcreteX(XgmModelAsset, ork::asset::Asset);

public:
  static const char* assetTypeNameStatic(void) {
    return "xgmodel";
  }

  XgmModelAsset() {
    clearModel();
  }
  ~XgmModelAsset() final;

  void clearModel() {
    auto the_model = std::make_shared<XgmModel>();
    the_model->_asset = this;
    _model.atomicWrite(the_model);
  }

  XgmModel* GetModel() { // todo unsafe
    OrkAssert(this);
    return _model.atomicCopy().get();
  }
  xgmmodel_ptr_t getSharedModel() { // todo unsafe
    OrkAssert(this);
    return _model.atomicCopy();
  }
  const XgmModel* GetModel() const { // todo unsafe
    OrkAssert(this);
    return _model.atomicCopy().get();
  }

  LockedResource<xgmmodel_ptr_t> _model;
};

struct XgmModelLoader final : public ork::asset::FileAssetLoader {
public:
  XgmModelLoader();
  ork::asset::asset_ptr_t _doLoadAsset(ork::asset::loadrequest_ptr_t loadreq) final;
  void destroy(ork::asset::asset_ptr_t asset) final;
  void initLoadersForUriProto(const std::string& uriproto) final;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmAnimAsset : public ork::asset::Asset {
  DeclareConcreteX(XgmAnimAsset, ork::asset::Asset);
  static const char* assetTypeNameStatic(void) {
    return "xganim";
  }
  XgmAnim _animation;

public: //
  XgmAnim* GetAnim() {
    return &_animation;
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
