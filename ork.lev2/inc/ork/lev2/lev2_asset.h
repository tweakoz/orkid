////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// level2 assets
///////////////////////////////////////////////////////////////////////////////

#ifndef _LEV2_ASSET_H
#define _LEV2_ASSET_H

///////////////////////////////////////////////////////////////////////////////

#include <ork/asset/Asset.h>
#include <ork/asset/AssetManager.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxanim.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

class TextureAsset : public ork::asset::Asset
{
	RttiDeclareConcrete(TextureAsset,ork::asset::Asset);

	static const char *GetAssetTypeNameStatic( void ) { return "lev2tex"; }

	ork::atomic<Texture*> mData;

public: //

	TextureAsset();
	~TextureAsset() override;

	Texture* GetTexture() const { return mData; }
	void SetTexture( Texture* pt );
};

typedef TextureAsset* textureassetptr_t; // prep for shared_ptr

///////////////////////////////////////////////////////////////////////////////

class XgmModelAsset : public ork::asset::Asset
{
	RttiDeclareConcrete(XgmModelAsset,ork::asset::Asset);
	static const char *GetAssetTypeNameStatic( void ) { return "xgmodel"; }
	XgmModel	mData;

public: //

	~XgmModelAsset() override;

	XgmModel* GetModel() { return & mData; }
	const XgmModel* GetModel() const { return & mData; }

};

typedef XgmModelAsset* xgmmodelassetptr_t; // prep for shared_ptr

///////////////////////////////////////////////////////////////////////////////

class XgmAnimAsset : public ork::asset::Asset
{
	RttiDeclareConcrete(XgmAnimAsset,ork::asset::Asset);
	static const char *GetAssetTypeNameStatic( void ) { return "xganim"; }
	XgmAnim	mData;

public: //

	XgmAnim* GetAnim() { return & mData; }

};

typedef XgmAnimAsset* xgmanimassetptr_t; // prep for shared_ptr

///////////////////////////////////////////////////////////////////////////////

class FxShaderAsset : public ork::asset::Asset
{
	RttiDeclareConcrete(FxShaderAsset,ork::asset::Asset);
	static const char *GetAssetTypeNameStatic( void ) { return "fxshader"; }
	FxShader	mData;

public: //

	FxShader* GetFxShader() { return & mData; }

};

typedef FxShaderAsset* fxshaderassetptr_t; // prep for shared_ptr

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////


#endif //_LEV2_ASSET_H
