////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
#include <pkg/ent/Compositor.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.h>
#include <pkg/ent/PerfController.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
CompositingMaterial::CompositingMaterial()
	: hModFX(nullptr)
	, hMapA(nullptr)
	, hMapB(nullptr)
	, hMapC(nullptr)
	, hTekBoverAplusC(nullptr)
	, hTekAplusBplusC(nullptr)
	, hTekAlerpBwithC(nullptr)
	, hTekCurrent(nullptr)
	, mCurrentTextureA(nullptr)
	, mCurrentTextureB(nullptr)
	, mCurrentTextureC(nullptr)
{
}
/////////////////////////////////////////////////
CompositingMaterial::~CompositingMaterial()
{
}
/////////////////////////////////////////////////
void CompositingMaterial::Init( lev2::GfxTarget* pTarg )
{
	if( 0 == hModFX )
	{
		hModFX = asset::AssetManager<lev2::FxShaderAsset>::Load( "orkshader://compositor" )->GetFxShader();

		hMatMVP = pTarg->FXI()->GetParameterH( hModFX, "MatMVP" );
		hLevels	= pTarg->FXI()->GetParameterH( hModFX, "Levels" );

		hMapA = pTarg->FXI()->GetParameterH( hModFX, "MapA" );
		hMapB = pTarg->FXI()->GetParameterH( hModFX, "MapB" );
		hMapC = pTarg->FXI()->GetParameterH( hModFX, "MapC" );

		hTekBoverAplusC		= pTarg->FXI()->GetTechnique( hModFX, "BoverAplusC" );
		hTekAplusBplusC		= pTarg->FXI()->GetTechnique( hModFX, "AplusBplusC" );
		hTekAlerpBwithC		= pTarg->FXI()->GetTechnique( hModFX, "AlerpBwithC" );
		
		hTekAsolo			= pTarg->FXI()->GetTechnique( hModFX, "Asolo" );
		hTekBsolo			= pTarg->FXI()->GetTechnique( hModFX, "Bsolo" );
		hTekCsolo			= pTarg->FXI()->GetTechnique( hModFX, "Csolo" );

		mRasterState.SetCullTest( ork::lev2::ECULLTEST_OFF );
		mRasterState.SetAlphaTest( ork::lev2::EALPHATEST_OFF );
		mRasterState.SetDepthTest( ork::lev2::EDEPTHTEST_OFF );

	}
}
/////////////////////////////////////////////////
void CompositingMaterial::SetTechnique( const std::string& tek )
{
	if( tek == "BoverAplusC" )
		hTekCurrent = hTekBoverAplusC;
	if( tek == "AplusBplusC" )
		hTekCurrent = hTekAplusBplusC;
	if( tek == "AlerpBwithC" )
		hTekCurrent = hTekAlerpBwithC;

	if( tek == "Asolo" )
		hTekCurrent = hTekAsolo;
	if( tek == "Bsolo" )
		hTekCurrent = hTekBsolo;
	if( tek == "Csolo" )
		hTekCurrent = hTekCsolo;
}
/////////////////////////////////////////////////
bool CompositingMaterial::BeginPass( lev2::GfxTarget* pTarg, int iPass )
{
	//printf("CompositorMtl draw\n");

	pTarg->RSI()->BindRasterState( mRasterState );
	pTarg->FXI()->BindPass( hModFX, iPass );
	pTarg->FXI()->BindParamMatrix( hModFX, hMatMVP, pTarg->MTXI()->RefMVPMatrix() );

	pTarg->FXI()->BindParamVect3( hModFX, hLevels, mWeights );

	if( mCurrentTextureA && hMapA ) 
	{
		pTarg->FXI()->BindParamCTex ( hModFX, hMapA, mCurrentTextureA );
	}
	if( mCurrentTextureB && hMapB ) 
	{
		pTarg->FXI()->BindParamCTex ( hModFX, hMapB, mCurrentTextureB );
	}
	if( mCurrentTextureC && hMapC ) 
	{
		pTarg->FXI()->BindParamCTex ( hModFX, hMapC, mCurrentTextureC );
	}

	pTarg->FXI()->CommitParams();
	return true;
}
/////////////////////////////////////////////////
void CompositingMaterial::EndPass( lev2::GfxTarget* pTarg )
{
	pTarg->FXI()->EndPass( hModFX );
}
/////////////////////////////////////////////////
int CompositingMaterial::BeginBlock( lev2::GfxTarget* pTarg, const lev2::RenderContextInstData &MatCtx )
{
	pTarg->FXI()->BindTechnique( hModFX, hTekCurrent );
	int inumpasses = pTarg->FXI()->BeginBlock( hModFX, MatCtx );
	return inumpasses;
}
/////////////////////////////////////////////////
void CompositingMaterial::EndBlock( lev2::GfxTarget* pTarg )
{
	pTarg->FXI()->EndBlock( hModFX );
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
