////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <orktool/manip/manip.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>

////////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {

GfxMaterialManip::GfxMaterialManip(GfxTarget* pTARG,CManipManager&mgr)
	: mbNoDepthTest( false )
	, mManager(mgr)
{
	miNumPasses = 1;

	hModFX = asset::AssetManager<FxShaderAsset>::Load( "orkshader://manip" )->GetFxShader();

	GfxMaterial::SetTexture( ETEXDEST_DIFFUSE, 0 );

	Init( pTARG );
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialManip::Init(ork::lev2::GfxTarget *pTarg)
{
	hTekStd = pTarg->FXI()->GetTechnique( hModFX, "std" );
	hTekPick = pTarg->FXI()->GetTechnique( hModFX, "pick" );
	hMVP = pTarg->FXI()->GetParameterH( hModFX, "mvp" );
	hCOLOR = pTarg->FXI()->GetParameterH( hModFX, "modcolor" );
	//hTekCurrent = hTekModColor;
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialManip::BeginBlock( GfxTarget* pTarg,const RenderContextInstData &MatCtx )
{
	int imode = mManager.GetDrawMode();
	bool bpick = pTarg->FBI()->IsPickState();

	pTarg->FXI()->BindTechnique( hModFX, bpick ? hTekPick : hTekStd );
	int inumpasses = pTarg->FXI()->BeginBlock( hModFX, MatCtx );
	return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialManip::EndBlock( GfxTarget* pTarg )
{
	pTarg->FXI()->EndBlock( hModFX );
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterialManip::BeginPass( GfxTarget* pTarg, int ipass )
{
	pTarg->FXI()->BindPass( hModFX, ipass );
	pTarg->FXI()->BindParamMatrix( hModFX, hMVP, pTarg->MTXI()->RefMVPMatrix() );

	CColor4 Color = pTarg->RefModColor();
	pTarg->FXI()->BindParamVect4( hModFX, hCOLOR, Color );

	pTarg->FXI()->CommitParams();

	return true;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialManip::UpdateMVPMatrix( GfxTarget *pTARG )
{
	pTARG->FXI()->BindParamMatrix( hModFX, hMVP, pTARG->MTXI()->RefMVPMatrix() );
	pTARG->FXI()->CommitParams();
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialManip::EndPass( GfxTarget* pTarg )
{
 	pTarg->FXI()->EndPass( hModFX );
}

} } //namespace ork::lev2
