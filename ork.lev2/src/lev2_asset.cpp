////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/lev2_asset.h>
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
template class ork::asset::AssetManager<ork::lev2::FxShaderAsset>;
template class ork::asset::AssetManager<ork::lev2::XgmModelAsset>;
template class ork::asset::AssetManager<ork::lev2::TextureAsset>;
template class ork::asset::AssetManager<ork::lev2::XgmAnimAsset>;

///////////////////////////////////////////////////////////////////////////////

using namespace ork::asset;
namespace ork::lev2 {

XgmModelAsset::~XgmModelAsset() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class XgmModelLoader final : public ork::asset::FileAssetLoader {
public:
  XgmModelLoader()
      : FileAssetLoader(XgmModelAsset::GetClassStatic()) {

    auto datactx = FileEnv::contextForUriProto("data://");
    auto srcctx  = FileEnv::contextForUriProto("src://");
    AddLocation(datactx, ".xgm");
    AddLocation(datactx, ".glb");
    AddLocation(datactx, ".gltf");
    AddLocation(datactx, ".dae");
    AddLocation(datactx, ".obj");
    AddLocation(srcctx, ".xgm");
    AddLocation(srcctx, ".glb");
    AddLocation(srcctx, ".gltf");
    AddLocation(srcctx, ".dae");
    AddLocation(srcctx, ".obj");
  }

  bool LoadFileAsset(
      asset::asset_ptr_t asset, //
      AssetPath assetpath) override {
    auto absolutepath = assetpath.ToAbsolute();
    auto modelasset   = std::dynamic_pointer_cast<XgmModelAsset>(asset);
    printf("LoadModelAsset<%s>\n", absolutepath.c_str());
    bool OK = false;
    if (absolutepath.GetExtension() == "xgm" or //
        absolutepath.GetExtension() == "dae" or //
        absolutepath.GetExtension() == "obj" or //
        absolutepath.GetExtension() == "glb" or //
        absolutepath.GetExtension() == "gltf") {
      modelasset->clearModel();
      OK = XgmModel::LoadUnManaged(modelasset->GetModel(), assetpath);
      asset::AssetManager<lev2::TextureAsset>::AutoLoad();
      // route to caching assimp->xgm processor
      OrkAssert(OK);
    }
    OrkAssert(OK);
    return OK;
  }

  void DestroyAsset(asset::asset_ptr_t asset) override {
    auto modelasset = std::dynamic_pointer_cast<XgmModelAsset>(asset);
    //	delete modelasset;
  }
};

///////////////////////////////////////////////////////////////////////////////

ork::asset::FileAssetLoader* modelLoader() {
  static XgmModelLoader* _loader = new XgmModelLoader;
  return _loader;
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelAsset::describeX(class_t* clazz) {
  auto loader = modelLoader();
  /*GetClassStatic()->AddLoader(loader);
  GetClassStatic()->SetAssetNamer("");
  GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("xgmodel"));
  */
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class StaticTexFileLoader : public ork::asset::FileAssetLoader {
public:
  StaticTexFileLoader()
      : FileAssetLoader(TextureAsset::GetClassStatic()) {
    auto datactx = FileEnv::contextForUriProto("data://");
    auto lev2ctx = FileEnv::contextForUriProto("lev2://");
    AddLocation(datactx, ".vds");
    AddLocation(datactx, ".qtz");
    AddLocation(datactx, ".dds");
    AddLocation(datactx, ".png");
    AddLocation(lev2ctx, ".dds");
  }

  bool LoadFileAsset(
      asset::asset_ptr_t asset, //
      AssetPath assetpath) {
    auto texture_asset                   = std::dynamic_pointer_cast<TextureAsset>(asset);
    texture_asset->GetTexture()->_varmap = texture_asset->_varmap;
    if (texture_asset->_varmap.hasKey("postproc")) {
      printf("texasset<%p:%s> has postproc\n", texture_asset.get(), assetpath.c_str());
    } else {
      // printf("texasset<%p:%s> does NOT have postproc\n", texture_asset, filename.c_str());
    }

    // OrkAssert(false == texture_asset->GetTexture()->_varmap.hasKey("preproc"));

    while (0 == GfxEnv::GetRef().loadingContext()) {
      ork::msleep(100);
    }
    auto p   = file::Path(asset->name());
    bool bOK = GfxEnv::GetRef().loadingContext()->TXI()->LoadTexture(p, texture_asset->GetTexture());
    OrkAssert(bOK);
    return true;
  }

  void DestroyAsset(asset::asset_ptr_t asset) {
    auto texture_asset = std::dynamic_pointer_cast<TextureAsset>(asset);
  }
};

///////////////////////////////////////////////////////////////////////////////

TextureAsset::TextureAsset() {
  mData = new Texture(this);
}
TextureAsset::~TextureAsset() {
  auto i = mData.exchange(nullptr);
  if (i)
    delete i;
}

void TextureAsset::describeX(class_t* clazz) {
  // GetClassStatic()->AddLoader(new StaticTexFileLoader);
  // GetClassStatic()->SetAssetNamer("");
  // GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("lev2tex"));
}

void TextureAsset::SetTexture(Texture* pt) {
  auto prev = mData.exchange(pt);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class XgmAnimLoader final : public ork::asset::FileAssetLoader {
public:
  XgmAnimLoader()
      : FileAssetLoader(XgmAnimAsset::GetClassStatic()) {
    auto datactx = FileEnv::contextForUriProto("data://");
    AddLocation(datactx, ".xga");
  }

  bool LoadFileAsset(asset::asset_ptr_t asset, AssetPath filename) override {
    auto animasset = std::dynamic_pointer_cast<XgmAnimAsset>(asset);
    bool bOK       = XgmAnim::LoadUnManaged(animasset->GetAnim(), filename);
    OrkAssert(bOK);
    return true;
  }

  void DestroyAsset(asset::asset_ptr_t asset) override {
#if defined(ORKCONFIG_ASSET_UNLOAD)
    auto animasset = std::dynamic_pointer_cast<XgmAnimAsset>(asset);
    bool bOK       = XgmAnim::UnLoadUnManaged(animasset->GetAnim());
    OrkAssert(bOK);
#endif
  }
};

///////////////////////////////////////////////////////////////////////////////

void XgmAnimAsset::describeX(class_t* clazz) {
  auto loader = new XgmAnimLoader;
  // GetClassStatic()->AddLoader(loader);
  // GetClassStatic()->SetAssetNamer("");
  // GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("xganim"));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FxShaderLoader : public ork::asset::FileAssetLoader {
public:
  FxShaderLoader();

  bool LoadFileAsset(asset::asset_ptr_t asset, AssetPath filename) final;
  void DestroyAsset(asset::asset_ptr_t asset) final {
    auto shader_asset = std::dynamic_pointer_cast<FxShaderAsset>(asset);
  }
};

///////////////////////////////////////////////////////////////////////////////

ork::asset::FileAssetLoader* shaderLoader() {
  static FxShaderLoader* _loader = new FxShaderLoader;
  return _loader;
}

///////////////////////////////////////////////////////////////////////////////

FxShaderLoader::FxShaderLoader()
    : FileAssetLoader(FxShaderAsset::GetClassStatic()) {
  /////////////////////
  // hmm, this wants to be target dependant, hence dynamically switchable
  /////////////////////
  FxShader::RegisterLoaders("shaders/glfx/", "glfx");
  auto shadctx = FileEnv::contextForUriProto("orkshader://");

  AddLocation(shadctx, ".glfx"); // for glsl targets
  AddLocation(shadctx, ".fxml"); // for the dummy target
}

///////////////////////////////////////////////////////////////////////////////

bool FxShaderLoader::LoadFileAsset(asset::asset_ptr_t asset, AssetPath filename) {
  // printf("Loading Effect url<%s> abs<%s>\n", filename.c_str(), pth.ToAbsolute().c_str());
  auto pshader = std::dynamic_pointer_cast<FxShaderAsset>(asset);
  bool bOK     = GfxEnv::GetRef().loadingContext()->FXI()->LoadFxShader(filename, pshader->GetFxShader());
  OrkAssert(bOK);
  if (bOK)
    pshader->GetFxShader()->SetName(filename.c_str());
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void FxShaderAsset::describeX(class_t* clazz) {
  auto loader = shaderLoader();
  // printf( "Registering FxShaderAsset\n" );

  // GetClassStatic()->AddLoader(loader);
  // GetClassStatic()->SetAssetNamer("orkshader://");
  // GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("fxshader"));
}

///////////////////////////////////////////////////////////////////////////////

void autoloadAssets(bool wait) {
  auto autoload_op = []() {
    bool loaded;
    do {
      loaded = false;
      loaded = asset::AssetManager<lev2::XgmAnimAsset>::AutoLoad() || loaded;
      loaded = asset::AssetManager<lev2::FxShaderAsset>::AutoLoad() || loaded;
      loaded = asset::AssetManager<lev2::XgmModelAsset>::AutoLoad() || loaded;
      loaded = asset::AssetManager<lev2::TextureAsset>::AutoLoad() || loaded;
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
