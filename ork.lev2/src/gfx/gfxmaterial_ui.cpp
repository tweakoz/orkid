////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterialUI, "MaterialUI" )

namespace ork { namespace lev2 {

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::Describe()
{

}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUI::~GfxMaterialUI()
{
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUI::GfxMaterialUI(GfxTarget *pTarg)
	: meType( ETYPE_STANDARD )
	, hTekMod( 0 )
	, hTekVtx( 0 )
	, hTekModVtx( 0 )
	, hTekCircle( 0 )
	, hTransform( 0 )
	, hModColor( 0 )
	, meUIColorMode( EUICOLOR_MOD )
{
	miNumPasses = 1;
	mRasterState.SetShadeModel( ESHADEMODEL_SMOOTH );
	mRasterState.SetAlphaTest( EALPHATEST_OFF );
	mRasterState.SetBlending( EBLENDING_OFF );
	mRasterState.SetDepthTest( EDEPTHTEST_LEQUALS );
	mRasterState.SetZWriteMask( false );
	mRasterState.SetCullTest( ECULLTEST_OFF );

	hModFX = asset::AssetManager<FxShaderAsset>::Load( "orkshader://ui" )->GetFxShader();

	//printf( "HMODFX<%p>\n", hModFX );
	OrkAssertI( hModFX!=0, "did you copy the shaders folder!\n" );

	if( pTarg )
	{
		Init(pTarg);
	}
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::Init(ork::lev2::GfxTarget *pTarg)
{
	//printf( "hModFX<%p>\n", hModFX );

	hTekMod = pTarg->FXI()->GetTechnique( hModFX, "uidev_modcolor" );

	//OrkAssert(hTekMod);
	hTekVtx = pTarg->FXI()->GetTechnique( hModFX, "ui_vtx" );
	hTekModVtx = pTarg->FXI()->GetTechnique( hModFX, "ui_vtxmod" );
	hTekCircle = pTarg->FXI()->GetTechnique( hModFX, "uicircle" );

	hTransform = pTarg->FXI()->GetParameterH( hModFX, "mvp" );
	hModColor = pTarg->FXI()->GetParameterH( hModFX, "ModColor" );
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUI::BeginBlock( GfxTarget *pTarg, const RenderContextInstData &MatCtx )
{
	const FxShaderTechnique* htek = 0;

	htek = hTekMod;
	switch( meType )
	{
		case ETYPE_STANDARD:
		{
			switch( meUIColorMode )
			{
				default:
				case EUICOLOR_MOD:
					htek = hTekMod;
					break;
				case EUICOLOR_VTX:
					htek = hTekVtx;
					break;
				case EUICOLOR_MODVTX:
					htek = hTekModVtx;
					break;
			}
			break;
		}
		case ETYPE_CIRCLE:
			htek = hTekCircle;
			break;
		default:
			OrkAssert(false);
	}

	pTarg->FXI()->BindTechnique( hModFX, htek );
	int inumpasses = pTarg->FXI()->BeginBlock( hModFX, MatCtx );
	return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::EndBlock( GfxTarget *pTarg )
{
	pTarg->FXI()->EndBlock( hModFX );
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUI::EndPass( GfxTarget *pTarg )
{
	pTarg->FXI()->EndPass( hModFX );
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterialUI::BeginPass( GfxTarget *pTarg, int iPass )
{
	///////////////////////////////
	pTarg->RSI()->BindRasterState( mRasterState );
	///////////////////////////////

	const CMatrix4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

	///////////////////////////////

	pTarg->FXI()->BindPass( hModFX, iPass );
	pTarg->FXI()->BindParamMatrix( hModFX, hTransform, MatMVP );
	pTarg->FXI()->BindParamVect4( hModFX, hModColor, pTarg->RefModColor() );
	pTarg->FXI()->CommitParams();

	return true;

}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUIText::GfxMaterialUIText(GfxTarget *pTarg)
	: hTek(0)
	, hTransform(0)
	, hModColor(0)
	, hColorMap(0)
{
	mRasterState.SetAlphaTest( EALPHATEST_GREATER, 0.0f );
	mRasterState.SetBlending( EBLENDING_OFF );
	mRasterState.SetDepthTest( EDEPTHTEST_ALWAYS );
	mRasterState.SetZWriteMask( false );
	mRasterState.SetCullTest( ECULLTEST_OFF );

	miNumPasses = 1;

	hModFX = asset::AssetManager<FxShaderAsset>::Load( "orkshader://ui" )->GetFxShader();

	if( pTarg )
	{
		Init(pTarg);
	}
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::Init(ork::lev2::GfxTarget *pTarg)
{
	hTek = pTarg->FXI()->GetTechnique( hModFX, "uitext" );

	hTransform = pTarg->FXI()->GetParameterH( hModFX, "mvp" );
	hModColor = pTarg->FXI()->GetParameterH( hModFX, "ModColor" );
	hColorMap = pTarg->FXI()->GetParameterH( hModFX, "ColorMap" );

	mRasterState.SetDepthTest( ork::lev2::EDEPTHTEST_OFF );
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUIText::BeginBlock( GfxTarget *pTarg, const RenderContextInstData &MatCtx )
{
	pTarg->FXI()->BindTechnique( hModFX, hTek );
	int inumpasses = pTarg->FXI()->BeginBlock( hModFX, MatCtx );
	return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::EndBlock( GfxTarget *pTarg )
{
	pTarg->FXI()->EndBlock( hModFX );
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUIText::EndPass( GfxTarget *pTarg )
{
	pTarg->FXI()->EndPass( hModFX );
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterialUIText::BeginPass( GfxTarget *pTarg, int iPass )
{
	pTarg->FXI()->BindPass( hModFX, iPass );

	///////////////////////////////
	SRasterState & RasterState = mRasterState; //pTarg->RSI()->RefUIRasterState();

	//RasterState.SetAlphaTest( EALPHATEST_GREATER, 0.0f );
	//pTarg->RSI()->BindRasterState( RasterState );

	///////////////////////////////

	const CMatrix4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

	pTarg->FXI()->BindParamMatrix( hModFX, hTransform, MatMVP );

	///////////////////////////////

	pTarg->FXI()->BindParamCTex( hModFX, hColorMap, GetTexture( ETEXDEST_DIFFUSE ).mpTexture );
	pTarg->FXI()->BindParamVect4( hModFX, hModColor, pTarg->RefModColor() );
	pTarg->FXI()->CommitParams();

	return true;
}

/////////////////////////////////////////////////////////////////////////

GfxMaterialUITextured::GfxMaterialUITextured( GfxTarget *pTarg, const std::string & Technique )
	: hTek(0)
	, mTechniqueName( Technique )
	, hTransform(0)
	, hModColor(0)
	, hColorMap(0)
{
	miNumPasses = 1;
	mRasterState.SetShadeModel( ESHADEMODEL_SMOOTH );
	mRasterState.SetAlphaTest( EALPHATEST_OFF );
	mRasterState.SetBlending( EBLENDING_OFF );
	mRasterState.SetDepthTest( EDEPTHTEST_LEQUALS );
	mRasterState.SetCullTest( ECULLTEST_OFF );

	mTechniqueName = Technique;

	if( pTarg )
	{
		Init(pTarg);
	}
}

void GfxMaterialUITextured::ClassInit()
{
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EffectInit( void )
{
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::Init(ork::lev2::GfxTarget *pTarg)
{
	hModFX = asset::AssetManager<FxShaderAsset>::Load( "orkshader://ui" )->GetFxShader();

	hTek = pTarg->FXI()->GetTechnique( hModFX, mTechniqueName );

	hTransform = pTarg->FXI()->GetParameterH( hModFX, "mvp" );
	hModColor = pTarg->FXI()->GetParameterH( hModFX, "ModColor" );
	hColorMap = pTarg->FXI()->GetParameterH( hModFX, "ColorMap" );
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::Init( ork::lev2::GfxTarget *pTarg, const std::string & Technique )
{
	mTechniqueName = Technique;
	Init(pTarg);
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterialUITextured::BeginBlock( GfxTarget *pTarg, const RenderContextInstData &MatCtx )
{
	pTarg->FXI()->BindTechnique( hModFX, hTek );
	int inumpasses = pTarg->FXI()->BeginBlock( hModFX, MatCtx );
	return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EndBlock( GfxTarget *pTarg )
{
	pTarg->FXI()->EndBlock( hModFX );
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterialUITextured::EndPass( GfxTarget *pTarg )
{
	pTarg->FXI()->EndPass( hModFX );
}

////////////////////////////////////////////////////////////2/////////////

bool GfxMaterialUITextured::BeginPass( GfxTarget *pTarg, int iPass )
{
	///////////////////////////////
	pTarg->RSI()->BindRasterState( mRasterState );
	///////////////////////////////

	const CMatrix4& MatMVP = pTarg->MTXI()->RefMVPMatrix();

	pTarg->FXI()->BindPass( hModFX, iPass );
	pTarg->FXI()->BindParamMatrix( hModFX, hTransform, MatMVP );
	pTarg->FXI()->BindParamCTex( hModFX, hColorMap, GetTexture( ETEXDEST_DIFFUSE ).mpTexture );
	pTarg->FXI()->BindParamVect4( hModFX, hModColor, pTarg->RefModColor() );
	pTarg->FXI()->CommitParams();
	return true;

}

} }





