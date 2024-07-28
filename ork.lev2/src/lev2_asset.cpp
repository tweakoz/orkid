////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/asset/Asset.inl>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/FileAssetNamer.h>
#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/opq.h>
#include <ork/util/logger.h>
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

static logchannel_ptr_t logchan_l2asso = logger()->createChannel("lev2ass",fvec3(1,1,.9),false);

XgmModelAsset::~XgmModelAsset() {
}


XgmAnimAsset::XgmAnimAsset(){
  _animation = std::make_shared<XgmAnim>();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

XgmModelLoader::XgmModelLoader()
    : FileAssetLoader(XgmModelAsset::GetClassStatic()) {
  initLoadersForUriProto("data://");
  initLoadersForUriProto("src://");
}

ork::asset::asset_ptr_t XgmModelLoader::_doLoadAsset(ork::asset::loadrequest_ptr_t loadreq) {
  auto absolutepath = loadreq->_asset_path.toAbsolute();
  auto modelasset   = std::make_shared<XgmModelAsset>();
  modelasset->GetModel()->_asset = modelasset.get();
  logchan_l2asso->log("LoadModelAsset<%s>", absolutepath.c_str());
  bool OK = false;
  if (absolutepath.getExtension() == "xgm" or //
      absolutepath.getExtension() == "dae" or //
      absolutepath.getExtension() == "orkscene" or //
      absolutepath.getExtension() == "obj" or //
      absolutepath.getExtension() == "glb" or //
      absolutepath.getExtension() == "orkemdl" or //
      absolutepath.getExtension() == "fbx" or //
      absolutepath.getExtension() == "gltf") {
    modelasset->clearModel();
    OK = XgmModel::LoadUnManaged(modelasset->GetModel(), //
                                 loadreq->_asset_path, //
                                 loadreq->_asset_vars);

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
  addLocation(ctx, ".orkemdl");
  addLocation(ctx, ".orkscene");
  addLocation(ctx, ".gltf");
  addLocation(ctx, ".dae");
  addLocation(ctx, ".obj");
  addLocation(ctx, ".fbx");
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelAsset::describeX(class_t* clazz) {
  auto loader = std::make_shared<XgmModelLoader>();
  registerLoader<XgmModelAsset>(loader);
  AssetLoader::registerLoaderForExtension("xgm",loader);
  AssetLoader::registerLoaderForExtension("obj",loader);
  AssetLoader::registerLoaderForExtension("gltf",loader);
  AssetLoader::registerLoaderForExtension("glb",loader);
  AssetLoader::registerLoaderForExtension("orkemdl",loader);
  AssetLoader::registerLoaderForExtension("fbx",loader);
  AssetLoader::registerLoaderForExtension("dae",loader);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

StaticTexFileLoader::StaticTexFileLoader()
    : FileAssetLoader(TextureAsset::GetClassStatic()) {
  initLoadersForUriProto("data://");
  initLoadersForUriProto("lev2://");
}

asset_ptr_t StaticTexFileLoader::_doLoadAsset(ork::asset::loadrequest_ptr_t loadreq) {
  auto texture_asset = std::make_shared<TextureAsset>();
  texture_asset->_varmap               = *loadreq->_asset_vars;
  texture_asset->GetTexture()->_vars   = loadreq->_asset_vars;
  texture_asset->_load_request          = loadreq;
  if (loadreq->_asset_vars->hasKey("postproc")){ //
    logchan_l2asso->log("texasset<%p:%s> has postproc", texture_asset.get(), loadreq->_asset_path.c_str());
  }
  auto context = lev2::contextForCurrentThread();

  auto txi = context->TXI();
  bool bOK = txi->LoadTexture(loadreq->_asset_path, texture_asset->GetTexture());
  OrkAssert(bOK);
  return texture_asset;
}

void StaticTexFileLoader::destroy(asset_ptr_t asset) {
  auto texture_asset = std::dynamic_pointer_cast<TextureAsset>(asset);
}

void StaticTexFileLoader::initLoadersForUriProto(const std::string& uriproto){
  auto ctx = FileEnv::contextForUriProto(uriproto);
  addLocation(ctx, ".vds");
  addLocation(ctx, ".qtz");
  addLocation(ctx, ".dds");
  addLocation(ctx, ".png");
  addLocation(ctx, ".dds");
}

///////////////////////////////////////////////////////////////////////////

TextureChoices::TextureChoices() {
  EnumerateChoices();
}

///////////////////////////////////////////////////////////////////////////////

TextureAsset::TextureAsset() {
  _texture = std::make_shared<Texture>(this);
  _texture->_asset = this;
}
TextureAsset::~TextureAsset() {
  if (not opq::TrackCurrent::is(opq::mainSerialQueue())){
    auto copy_of_tex = _texture;
    opq::mainSerialQueue()->enqueue([copy_of_tex]() mutable {
       copy_of_tex = nullptr;
    });
  }
  _texture = nullptr;
}

void TextureAsset::describeX(class_t* clazz) {
  auto loader = std::make_shared<StaticTexFileLoader>();
  AssetLoader::registerLoaderForExtension("tga",loader);
  AssetLoader::registerLoaderForExtension("png",loader);
  AssetLoader::registerLoaderForExtension("dds",loader);
  registerLoader<TextureAsset>(loader);
}

void TextureAsset::SetTexture(texture_ptr_t pt) {
  _texture = pt;
  _texture->_asset = this;
}

///////////////////////////////////////////////////////////////////////////////

void TextureChoices::EnumerateChoices(bool bforcenocache) {
  clear();
  auto loader = getLoader<TextureAsset>();
  auto items = loader->EnumerateExisting();
  for (const auto& i : items)
    add(util::AttrChoiceValue(i.c_str(), i.c_str()));
}
svar64_t TextureChoices::provideSelection(const std::string& key) const {
  auto asset = ork::asset::AssetManager<ork::lev2::TextureAsset>::load(key);
  svar64_t rval;
  rval.set<asset::asset_ptr_t>(asset);
  return rval;
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
    addLocation(datactx, ".gltf");
    addLocation(datactx, ".fbx");
  }

  asset_ptr_t _doLoadAsset(asset::loadrequest_ptr_t loadreq) final {
    auto animasset = std::make_shared<XgmAnimAsset>();
    bool bOK       = XgmAnim::LoadUnManaged(animasset->_animation.get(), loadreq->_asset_path);
    OrkAssert(bOK);
    return animasset;
  }

  void destroy(asset_ptr_t asset) final {
#if defined(ORKCONFIG_ASSET_UNLOAD)
    auto animasset = std::dynamic_pointer_cast<XgmAnimAsset>(asset);
    bool bOK       = XgmAnim::unloadUnManaged(animasset->_animation.get());
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

  asset_ptr_t _doLoadAsset(asset::loadrequest_ptr_t loadreq) override;
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

asset_ptr_t FxShaderLoader::_doLoadAsset(asset::loadrequest_ptr_t loadreq) {
  auto pshader = std::make_shared<FxShaderAsset>();
  auto context = lev2::contextForCurrentThread();
  auto fxi     = context->FXI();
  auto path = loadreq->_asset_path;
  bool bOK     = fxi->LoadFxShader(path, pshader->GetFxShader());
  OrkAssert(bOK);
  if (bOK)
    pshader->GetFxShader()->SetName(path.c_str());
  return pshader;
}

///////////////////////////////////////////////////////////////////////////////

void FxShaderAsset::describeX(class_t* clazz) {
  auto loader = std::make_shared<FxShaderLoader>();
  registerLoader<FxShaderAsset>(loader);
}

///////////////////////////////////////////////////////////////////////////////

void autoloadAssets(bool wait) {
  auto autoload_op = []{
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
