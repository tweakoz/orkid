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

INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::FxShaderAsset, "FxShader" );
INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::TextureAsset, "lev2tex" );
INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::XgmModelAsset, "xgmodel" );
INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::XgmAnimAsset, "xganim" );


template class ork::orklut<ork::PoolString,ork::lev2::FxShaderAsset*>;
template class ork::asset::AssetManager<ork::lev2::FxShaderAsset>;
template class ork::asset::AssetManager<ork::lev2::XgmModelAsset>;
template class ork::asset::AssetManager<ork::lev2::TextureAsset>;
template class ork::asset::AssetManager<ork::lev2::XgmAnimAsset>;
template class ork::asset::AssetManager<ork::lev2::AudioStream>;
template class ork::asset::AssetManager<ork::lev2::AudioBank>;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

XgmModelAsset::~XgmModelAsset()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class XgmModelLoader : public ork::asset::FileAssetLoader
{
public:

	XgmModelLoader()
	{
#if defined(WII)
		AddFileExtension(".ggm");
#else
		AddFileExtension(".xgm");
#endif
	}

	bool LoadFileAsset(asset::Asset *pAsset, ConstString filename)
	{
		XgmModelAsset* pmodel = rtti::safe_downcast<XgmModelAsset*>(pAsset);

		bool bOK = true;

		//lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
		{
			bool bOK = XgmModel::LoadUnManaged( pmodel->GetModel(), filename.c_str() );
			asset::AssetManager<lev2::TextureAsset>::AutoLoad();
			OrkAssert( bOK );
		}
		//lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();

		return true;
	}

	void DestroyAsset(asset::Asset *pAsset)
	{
		XgmModelAsset* modelasset = rtti::safe_downcast<XgmModelAsset*>(pAsset);
	//	delete modelasset;
	}
};

///////////////////////////////////////////////////////////////////////////////

static XgmModelLoader loader;
static ork::asset::FileAssetNamer namer;
void XgmModelAsset::Describe()
{
	GetClassStatic()->AddLoader(loader);
	GetClassStatic()->SetAssetNamer(&namer);
	GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("xgmodel"));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class TexLoader : public ork::asset::FileAssetLoader
{
public:

	TexLoader()
	{
		AddFileExtension(".dds");
		AddFileExtension(".qtz");
		AddFileExtension(".vds");
	}

	bool LoadFileAsset(asset::Asset *pAsset, ConstString filename)
	{
		ork::file::Path pth(filename.c_str());
		//printf( "Loading Texture url<%s> abs<%s>\n", filename.c_str(), pth.ToAbsolute().c_str() );
		
		//GfxEnv::GetRef().GetGlobalLock().Lock();
		{
			TextureAsset* ptex = rtti::safe_downcast<TextureAsset*>(pAsset);

			while( 0 == GfxEnv::GetRef().GetLoaderTarget() )
			{
				ork::msleep(100);
			}
			bool bOK = GfxEnv::GetRef().GetLoaderTarget()->TXI()->LoadTexture( pAsset->GetName(), ptex->GetTexture() );
			OrkAssert( bOK );
		}
		//GfxEnv::GetRef().GetGlobalLock().UnLock();
		return true;
	} 

	void DestroyAsset(asset::Asset *pAsset)
	{
		TextureAsset* ptex = rtti::safe_downcast<TextureAsset*>(pAsset);
		GfxEnv::GetRef().GetLoaderTarget()->TXI()->VRamDeport(ptex->GetTexture());
	//	delete ptex;
	}
};

///////////////////////////////////////////////////////////////////////////////

static TexLoader gloader;
static ork::asset::FileAssetNamer gnamer;

TextureAsset::~TextureAsset()
{
}

void TextureAsset::Describe()
{
	GetClassStatic()->AddLoader(gloader);
	GetClassStatic()->SetAssetNamer(&gnamer);
	GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("lev2tex"));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class XgmAnimLoader : public ork::asset::FileAssetLoader
{
public:

	XgmAnimLoader()
	{
		AddFileExtension(".xga");
		AddFileExtension(".gga");
	}

	bool LoadFileAsset(asset::Asset *pAsset, ConstString filename)
	{
		//lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
		{
			XgmAnimAsset* panim = rtti::safe_downcast<XgmAnimAsset*>(pAsset);

			bool bOK = XgmAnim::LoadUnManaged( panim->GetAnim(), filename.c_str() );
			OrkAssert( bOK );
		}
		//lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
		return true;
	}

	void DestroyAsset(asset::Asset *pAsset)
	{
#if defined(ORKCONFIG_ASSET_UNLOAD)
		XgmAnimAsset* compasset = rtti::safe_downcast<XgmAnimAsset*>(pAsset);
		bool bOK = XgmAnim::UnLoadUnManaged( compasset->GetAnim() );
		OrkAssert( bOK );
#endif
	}
};

///////////////////////////////////////////////////////////////////////////////

static XgmAnimLoader ganimloader;
static ork::asset::FileAssetNamer ganimnamer;

void XgmAnimAsset::Describe()
{
	GetClassStatic()->AddLoader(ganimloader);
	GetClassStatic()->SetAssetNamer(&ganimnamer);
	GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("xganim"));

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FxShaderLoader : public ork::asset::FileAssetLoader
{
public:

	FxShaderLoader();

	/*virtual*/ bool LoadFileAsset(asset::Asset *pAsset, ConstString filename);
	/*virtual*/ void DestroyAsset(asset::Asset *pAsset)
	{
		FxShaderAsset* compasset = rtti::safe_downcast<FxShaderAsset*>(pAsset);
		//delete compasset->GetComponent();
		//compasset->SetComponent(NULL);
	}
};

FxShaderLoader::FxShaderLoader()
{
	/////////////////////
	// hmm, this wants to be target dependant, hence dynamically switchable
	/////////////////////

#if defined( WII )
	AddFileExtension(".fxml");
#elif defined(_IOS)
	AddFileExtension(".glfx");		// for glsl targets
#elif defined(IX)
	AddFileExtension(".cgfx");		// for gl and dx targets
	AddFileExtension(".glfx");		// for gl and dx targets
	AddFileExtension(".fxml");	// for the dummy target
#else
	AddFileExtension(".glfx");		// for gl and dx targets
	AddFileExtension(".fx");		// for gl and dx targets
	AddFileExtension(".fxml");	// for the dummy target
#endif

}

bool FxShaderLoader::LoadFileAsset(asset::Asset *pAsset, ConstString filename)
{
	//lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	{	
		ork::file::Path pth(filename.c_str());
		printf( "Loading Effect url<%s> abs<%s>\n", filename.c_str(), pth.ToAbsolute().c_str() );
		FxShaderAsset* pshader = rtti::safe_downcast<FxShaderAsset*>(pAsset);
		bool bOK = GfxEnv::GetRef().GetLoaderTarget()->FXI()->LoadFxShader( filename.c_str(), pshader->GetFxShader() );
		OrkAssert( bOK );
		if(bOK)
			pshader->GetFxShader()->SetName(filename.c_str());
	}
	//lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	return true;
}

static FxShaderLoader gfxloader;
static ork::asset::FileAssetNamer gfxnamer("orkshader://");

void FxShaderAsset::Describe()
{
	printf( "Registering FxShaderAsset\n" );
	
	GetClassStatic()->AddLoader(gfxloader);
	GetClassStatic()->SetAssetNamer(&gfxnamer);
	GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("fxshader"));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//template class asset::AssetManager<FxShaderAsset>;

}}
