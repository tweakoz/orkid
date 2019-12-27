////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/FileAssetNamer.h>
#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/asset/AssetManager.hpp>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/audiobank.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::FxShaderAsset, "FxShader");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::TextureAsset, "lev2tex");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::XgmModelAsset, "xgmodel");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::XgmAnimAsset, "xganim");

template class ork::orklut<ork::PoolString, ork::lev2::FxShaderAsset*>;
template class ork::asset::AssetManager<ork::lev2::FxShaderAsset>;
template class ork::asset::AssetManager<ork::lev2::XgmModelAsset>;
template class ork::asset::AssetManager<ork::lev2::TextureAsset>;
template class ork::asset::AssetManager<ork::lev2::XgmAnimAsset>;
template class ork::asset::AssetManager<ork::lev2::AudioStream>;
template class ork::asset::AssetManager<ork::lev2::AudioBank>;

///////////////////////////////////////////////////////////////////////////////

using namespace ork::asset;

namespace ork { namespace lev2 {

XgmModelAsset::~XgmModelAsset() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class XgmModelLoader : public ork::asset::FileAssetLoader {
public:
  XgmModelLoader()
      : FileAssetLoader(XgmModelAsset::GetClassStatic()) {
    AddLocation("data://", ".xgm");
  }

  bool LoadFileAsset(asset::Asset* pAsset, ConstString filename) {
    XgmModelAsset* pmodel = rtti::safe_downcast<XgmModelAsset*>(pAsset);

    bool bOK = XgmModel::LoadUnManaged(pmodel->GetModel(), filename.c_str());
    asset::AssetManager<lev2::TextureAsset>::AutoLoad();
    OrkAssert(bOK);

    return true;
  }

  void DestroyAsset(asset::Asset* pAsset) {
    XgmModelAsset* modelasset = rtti::safe_downcast<XgmModelAsset*>(pAsset);
    //	delete modelasset;
  }
};

///////////////////////////////////////////////////////////////////////////////

ork::asset::FileAssetLoader* modelLoader() {
  static XgmModelLoader* _loader = new XgmModelLoader;
  return _loader;
}

///////////////////////////////////////////////////////////////////////////////

void XgmModelAsset::Describe() {
  auto loader = modelLoader();
  GetClassStatic()->AddLoader(loader);
  GetClassStatic()->SetAssetNamer("");
  GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("xgmodel"));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class StaticTexFileLoader : public ork::asset::FileAssetLoader {
public:
  StaticTexFileLoader()
      : FileAssetLoader(TextureAsset::GetClassStatic()) {
    AddLocation("data://", ".vds");
    AddLocation("data://", ".qtz");
    AddLocation("data://", ".dds");
    // AddLocation( "data://", ".png" );
    // AddLocation( "lev2://", ".tga" );
    // AddLocation( "lev2://", ".png" );
    AddLocation("lev2://", ".dds");
  }

  bool LoadFileAsset(asset::Asset* pAsset, ConstString filename) {
    ork::file::Path pth(filename.c_str());
    TextureAsset* tex_asset          = rtti::safe_downcast<TextureAsset*>(pAsset);
    tex_asset->GetTexture()->_varmap = tex_asset->_varmap;
    if (tex_asset->_varmap.hasKey("postproc")) {
      printf("texasset<%p> has postproc\n", tex_asset);
    } else {
      printf("texasset<%p> does NOT have postproc\n", tex_asset);
    }

    // OrkAssert(false == tex_asset->GetTexture()->_varmap.hasKey("preproc"));

    while (0 == GfxEnv::GetRef().GetLoaderTarget()) {
      ork::msleep(100);
    }
    auto p   = file::Path(pAsset->GetName());
    bool bOK = GfxEnv::GetRef().GetLoaderTarget()->TXI()->LoadTexture(p, tex_asset->GetTexture());
    OrkAssert(bOK);
    return true;
  }

  void DestroyAsset(asset::Asset* pAsset) {
    TextureAsset* ptex = rtti::safe_downcast<TextureAsset*>(pAsset);
  }
};

///////////////////////////////////////////////////////////////////////////////

TextureAsset::TextureAsset() {
  mData = new Texture;
}
TextureAsset::~TextureAsset() {
  auto i = mData.exchange(nullptr);
  if (i)
    delete i;
}

void TextureAsset::Describe() {
  GetClassStatic()->AddLoader(new StaticTexFileLoader);
  GetClassStatic()->SetAssetNamer("");
  GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("lev2tex"));
}

void TextureAsset::SetTexture(Texture* pt) {
  auto prev = mData.exchange(pt);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class XgmAnimLoader : public ork::asset::FileAssetLoader {
public:
  XgmAnimLoader()
      : FileAssetLoader(XgmAnimAsset::GetClassStatic()) {
    AddLocation("data://", ".xga");
  }

  bool LoadFileAsset(asset::Asset* pAsset, ConstString filename) {
    XgmAnimAsset* panim = rtti::safe_downcast<XgmAnimAsset*>(pAsset);
    bool bOK            = XgmAnim::LoadUnManaged(panim->GetAnim(), filename.c_str());
    OrkAssert(bOK);
    return true;
  }

  void DestroyAsset(asset::Asset* pAsset) {
#if defined(ORKCONFIG_ASSET_UNLOAD)
    XgmAnimAsset* compasset = rtti::safe_downcast<XgmAnimAsset*>(pAsset);
    bool bOK                = XgmAnim::UnLoadUnManaged(compasset->GetAnim());
    OrkAssert(bOK);
#endif
  }
};

///////////////////////////////////////////////////////////////////////////////

void XgmAnimAsset::Describe() {
  auto loader = new XgmAnimLoader;
  GetClassStatic()->AddLoader(loader);
  GetClassStatic()->SetAssetNamer("");
  GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("xganim"));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FxShaderLoader : public ork::asset::FileAssetLoader {
public:
  FxShaderLoader();

  bool LoadFileAsset(asset::Asset* pAsset, ConstString filename) final;
  void DestroyAsset(asset::Asset* pAsset) final {
    FxShaderAsset* compasset = rtti::safe_downcast<FxShaderAsset*>(pAsset);
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

  AddLocation("orkshader://", ".glfx"); // for glsl targets
  AddLocation("orkshader://", ".fxml"); // for the dummy target
}

///////////////////////////////////////////////////////////////////////////////

bool FxShaderLoader::LoadFileAsset(asset::Asset* pAsset, ConstString filename) {
  ork::file::Path pth(filename.c_str());
  // printf("Loading Effect url<%s> abs<%s>\n", filename.c_str(), pth.ToAbsolute().c_str());
  auto pshader = rtti::safe_downcast<FxShaderAsset*>(pAsset);
  bool bOK     = GfxEnv::GetRef().GetLoaderTarget()->FXI()->LoadFxShader(filename.c_str(), pshader->GetFxShader());
  OrkAssert(bOK);
  if (bOK)
    pshader->GetFxShader()->SetName(filename.c_str());
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void FxShaderAsset::Describe() {
  auto loader = shaderLoader();
  // printf( "Registering FxShaderAsset\n" );

  GetClassStatic()->AddLoader(loader);
  GetClassStatic()->SetAssetNamer("orkshader://");
  GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("fxshader"));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// template class asset::AssetManager<FxShaderAsset>;

}} // namespace ork::lev2
