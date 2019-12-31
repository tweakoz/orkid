////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/lev2_asset.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

FxInterface::FxInterface()
	: mpActiveFxShader( 0 )
	, mpLastFxMaterial( 0 )
	, mpGroupCurMaterial(0)
	, mpGroupMaterial(0)
{

}

void FxInterface::BindParamTex(FxShader* hfx, const FxShaderParam* hpar, const lev2::TextureAsset* texasset)
{
  auto texture = (texasset != nullptr) ? texasset->GetTexture() : nullptr;
  if(texture)
    BindParamCTex(hfx,hpar,texture);
}

///////////////////////////////////////////////////////////////////////////////

void FxInterface::InvalidateStateBlock( void )
{
	mpLastFxMaterial = 0;
	mpGroupCurMaterial = 0;
	mpGroupMaterial = 0;
}

///////////////////////////////////////////////////////////////////////////////

void FxInterface::BeginMaterialGroup( GfxMaterial* pmtl )
{
	mpGroupMaterial=pmtl;
}

///////////////////////////////////////////////////////////////////////////////

void FxInterface::EndMaterialGroup()
{

}

///////////////////////////////////////////////////////////////////////////////

void FxInterface::BeginFrame()
{
	InvalidateStateBlock();
	_doBeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void FxInterface::Reset()
{
	GfxTarget* pTARG = GfxEnv::GetRef().GetLoaderTarget();
	pTARG->FXI()->DoOnReset();
}

///////////////////////////////////////////////////////////////////////////////

} }
