////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/asset/Asset.inl>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/FileAssetNamer.h>
#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/opq.h>
#include <ork/asset/AssetManager.hpp>
#include <ork/lev2/aud/audiodevice.h>

ImplementReflectionX(ork::lev2::FxShaderAsset, "FxShader");
ImplementReflectionX(ork::lev2::TextureAsset, "lev2tex");
ImplementReflectionX(ork::lev2::XgmModelAsset, "xgmodel");
ImplementReflectionX(ork::lev2::XgmAnimAsset, "xganim");

template class ork::orklut<ork::PoolString, ork::lev2::FxShaderAsset*>;
template struct ork::asset::AssetManager<ork::lev2::FxShaderAsset>;
template struct ork::asset::AssetManager<ork::lev2::XgmModelAsset>;
template struct ork::asset::AssetManager<ork::lev2::TextureAsset>;
template struct ork::asset::AssetManager<ork::lev2::XgmAnimAsset>;

///////////////////////////////////////////////////////////////////////////////

using namespace ork::asset;
namespace ork::lev2 {

XgmModelAsset::~XgmModelAsset() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

XgmModelLoader::XgmModelLoader()
    : FileAssetLoader(XgmModelAsset::GetClassStatic()) {
  initLoadersForUriProto("data://");
  initLoadersForUriProto("src://");
}

ork::asset::asset_ptr_t XgmModelLoader::_doLoadAsset(AssetPath assetpath, ork::asset::vars_constptr_t vars) {
  auto absolutepath = assetpath.ToAbsolute();
  auto modelasset   = std::make_shared<XgmModelAsset>();
  printf("LoadModelAsset<%s>\n", absolutepath.c_str());
  bool OK = false;
  if (absolutepath.GetExtension() == "xgm" or //
      absolutepath.GetExtension() == "dae" or //
      absolutepath.GetExtension() == "obj" or //
      absolutepath.GetExtension() == "glb" or //
      absolutepath.GetExtension() == "fbx" or //
      absolutepath.GetExtension() == "gltf") {
    modelasset->clearModel();
    OK = XgmModel::LoadUnManaged(modelasset->GetModel(), assetpath,vars);
    // route to caching assimp->xgm processor
    OrkAssert(OK);
  }
  OrkAssert(OK);
  return modelasset;
}

void XgmModelLoader::destroy(ork::asset::asset_ptr_t asset) {
  auto modelasset = std::dynamic_pointer_cast<XgmModelAsset>(asset);
  //	delete modelasset;
}

void XgmModelLoader::initLoadersForUriProto(const std::string& uriproto) {
  auto ctx = FileEnv::contextForUriProto(uriproto);
  addLocation(ctx, ".xgm");
  addLocation(ctx, ".glb");
  addLocation(ctx, ".gltf");
  addLocation(ctx, ".dae");
  addLocation(ctx, ".obj");
  addLocation(ctx, ".fbx");
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelAsset::describeX(class_t* clazz) {
  auto loader = std::make_shared<XgmModelLoader>();
  registerLoader<XgmModelAsset>(loader);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class StaticTexFileLoader : public FileAssetLoader {
public:
  StaticTexFileLoader()
      : FileAssetLoader(TextureAsset::GetClassStatic()) {
    auto datactx = FileEnv::contextForUriProto("data://");
    auto lev2ctx = FileEnv::contextForUriProto("lev2://");
    addLocation(datactx, ".vds");
    addLocation(datactx, ".qtz");
    addLocation(datactx, ".dds");
    addLocation(datactx, ".png");
    addLocation(lev2ctx, ".dds");
  }

  asset_ptr_t _doLoadAsset(AssetPath assetpath, vars_constptr_t vars) {
    auto texture_asset = std::make_shared<TextureAsset>();
    if (vars) {
      texture_asset->_varmap               = vars;
      texture_asset->GetTexture()->_varmap = *vars;
      // if (vars->hasKey("postproc"))
      // printf("texasset<%p:%s> has postproc\n", texture_asset.get(), assetpath.c_str());
    }
    // OrkAssert(false == texture_asset->GetTexture()->_varmap.hasKey("preproc"));
    auto context = lev2::contextForCurrentThread();

    auto txi = context->TXI();
    bool bOK = txi->LoadTexture(assetpath, texture_asset->GetTexture());
    OrkAssert(bOK);
    return texture_asset;
  }

  void destroy(asset_ptr_t asset) {
    auto texture_asset = std::dynamic_pointer_cast<TextureAsset>(asset);
  }
};

///////////////////////////////////////////////////////////////////////////////

TextureAsset::TextureAsset() {
  _texture = std::make_shared<Texture>(this);
}
TextureAsset::~TextureAsset() {
  _texture = nullptr;
}

void TextureAsset::describeX(class_t* clazz) {
  auto loader = std::make_shared<StaticTexFileLoader>();
  registerLoader<TextureAsset>(loader);
}

void TextureAsset::SetTexture(texture_ptr_t pt) {
  _texture = pt;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class XgmAnimLoader final : public FileAssetLoader {
public:
  XgmAnimLoader()
      : FileAssetLoader(XgmAnimAsset::GetClassStatic()) {
    auto datactx = FileEnv::contextForUriProto("data://");
    addLocation(datactx, ".xga");
    addLocation(datactx, ".fbx");
  }

  asset_ptr_t _doLoadAsset(AssetPath filename, vars_constptr_t vars) override {
    auto animasset = std::make_shared<XgmAnimAsset>();
    bool bOK       = XgmAnim::LoadUnManaged(animasset->GetAnim(), filename);
    OrkAssert(bOK);
    return animasset;
  }

  void destroy(asset_ptr_t asset) override {
#if defined(ORKCONFIG_ASSET_UNLOAD)
    auto animasset = std::dynamic_pointer_cast<XgmAnimAsset>(asset);
    bool bOK       = XgmAnim::unloadUnManaged(animasset->GetAnim());
    OrkAssert(bOK);
#endif
  }
};

///////////////////////////////////////////////////////////////////////////////

void XgmAnimAsset::describeX(class_t* clazz) {
  auto loader = std::make_shared<XgmAnimLoader>();
  registerLoader<XgmAnimAsset>(loader);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FxShaderAsset::FxShaderAsset() {
  _shader = new FxShader;
}
FxShaderAsset::~FxShaderAsset() {
  if (_shader)
    delete _shader;
}
class FxShaderLoader final : public FileAssetLoader {
public:
  FxShaderLoader();

  asset_ptr_t _doLoadAsset(AssetPath filename, vars_constptr_t vars) override;
  void destroy(asset_ptr_t asset) override {
    auto shader_asset = std::dynamic_pointer_cast<FxShaderAsset>(asset);
  }
};

///////////////////////////////////////////////////////////////////////////////

FxShaderLoader::FxShaderLoader()
    : FileAssetLoader(FxShaderAsset::GetClassStatic()) {
  /////////////////////
  // hmm, this wants to be target dependant, hence dynamically switchable
  /////////////////////
  FxShader::RegisterLoaders("shaders/glfx/", "glfx");
  auto shadctx = FileEnv::contextForUriProto("orkshader://");
  auto democtx = FileEnv::contextForUriProto("demo://");

  addLocation(shadctx, ".glfx"); // for glsl targets
  addLocation(shadctx, ".fxml"); // for the dummy target

  if( democtx ){
    addLocation(democtx, ".glfx"); // for glsl targets
  }
}

///////////////////////////////////////////////////////////////////////////////

asset_ptr_t FxShaderLoader::_doLoadAsset(AssetPath resolvedpath, vars_constptr_t vars) {
  auto pshader = std::make_shared<FxShaderAsset>();
  auto context = lev2::contextForCurrentThread();
  auto fxi     = context->FXI();
  bool bOK     = fxi->LoadFxShader(resolvedpath, pshader->GetFxShader());
  OrkAssert(bOK);
  if (bOK)
    pshader->GetFxShader()->SetName(resolvedpath.c_str());
  return pshader;
}

///////////////////////////////////////////////////////////////////////////////

void FxShaderAsset::describeX(class_t* clazz) {
  auto loader = std::make_shared<FxShaderLoader>();
  registerLoader<FxShaderAsset>(loader);
}

///////////////////////////////////////////////////////////////////////////////

void autoloadAssets(bool wait) {
  auto autoload_op = []() {
    bool loaded;
    do {
      loaded = false;
    } while (loaded);
  };
  if (opq::TrackCurrent::is(opq::mainSerialQueue()))
    autoload_op(); // already on main thread, just do it..
  else {
    if (wait) {
      // not on main thread, but waiting,
      //  fire and synchronize
      auto task = opq::createCompletionGroup(
          opq::mainSerialQueue(), //
          "AssetLoad");
      task->enqueue(autoload_op);
      task->join();
    } else {
      // not waiting and not on main thread,
      //  just fire and forget..
      opq::mainSerialQueue()->enqueue(autoload_op);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
