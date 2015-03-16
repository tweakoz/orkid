////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterial3DSolid, "MaterialSolid" )

namespace ork { namespace lev2 {

/////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::Describe()
{

}

/////////////////////////////////////////////////////////////////////////

bool gearlyhack = true;

GfxMaterial3DSolid::GfxMaterial3DSolid(GfxTarget* pTARG)
	: meColorMode( EMODE_MOD_COLOR )
	, hTekVertexColor( 0 )
	, hTekVertexModColor( 0 )
	, hTekTexColor( 0 )
	, hTekTexModColor( 0 )
	, hTekTexTexModColor(0)
	, hTekModColor( 0 )
	, hTekTexVertexColor( 0 )
	, hMatMVP( 0 )
	, hMatMV( 0 )
	, hMatM( 0 )
	, hMatV( 0 )
	, hMatP( 0 )
	, hParamModColor( 0 )
	, mVolumeTexture( 0 )
	, mCurrentTexture( 0 )
	, mCurrentTexture2( 0 )
	, mCurrentTexture3( 0 )
	, mCurrentTexture4( 0 )
	, hVolumeMap(0)
	, hColorMap(0)
	, hColorMap2(0)
	, hColorMap3(0)
	, hColorMap4(0)
	, hParamUser0(0)
	, hParamTime( 0 )
	, hParamNoiseShift(0)
	, hParamNoiseFreq(0)
	, hParamNoiseAmp(0)
	, hModFX( 0 )
{
	mRasterState.SetShadeModel( ESHADEMODEL_SMOOTH );
	mRasterState.SetAlphaTest( EALPHATEST_OFF );
	mRasterState.SetBlending( EBLENDING_OFF );
	mRasterState.SetDepthTest( EDEPTHTEST_LEQUALS );
	mRasterState.SetZWriteMask( true );
	mRasterState.SetCullTest( ECULLTEST_OFF );

	miNumPasses = 1;

	if( false == gearlyhack )
	{
		hModFX = asset::AssetManager<FxShaderAsset>::Load( "orkshader://solid" )->GetFxShader();
	}

	if( pTARG )
	{
		Init(pTARG);
	}
}

GfxMaterial3DSolid::GfxMaterial3DSolid(GfxTarget* pTARG, const char* puserfx, const char* pusertek, bool allowcompilefailure, bool unmanaged )
	: meColorMode( EMODE_USER )
	, hTekVertexColor( 0 )
	, hTekVertexModColor( 0 )
	, hTekTexColor( 0 )
	, hTekTexModColor( 0 )
	, hTekTexTexModColor(0)
	, hTekModColor( 0 )
	, hTekTexVertexColor( 0 )
	, hMatMVP( 0 )
	, hMatMV( 0 )
	, hMatM( 0 )
	, hMatV( 0 )
	, hMatP( 0 )
	, hParamModColor( 0 )
	, mVolumeTexture( 0 )
	, mCurrentTexture( 0 )
	, mCurrentTexture2( 0 )
	, mCurrentTexture3( 0 )
	, mCurrentTexture4( 0 )
	, mUserFxName(puserfx)
	, mUserTekName(pusertek)
	, hVolumeMap(0)
	, hColorMap(0)
	, hColorMap2(0)
	, hColorMap3(0)
	, hColorMap4(0)
	, hParamUser0(0)
	, hParamNoiseShift(0)
	, hParamNoiseFreq(0)
	, hParamNoiseAmp(0)
	, hParamTime( 0 )
	, hModFX( 0 )
	, mUnManaged(unmanaged)
	, mAllowCompileFailure(allowcompilefailure)
{

	mRasterState.SetShadeModel( ESHADEMODEL_SMOOTH );
	mRasterState.SetAlphaTest( EALPHATEST_OFF );
	mRasterState.SetBlending( EBLENDING_OFF );
	mRasterState.SetDepthTest( EDEPTHTEST_LEQUALS );
	mRasterState.SetZWriteMask( true );
	mRasterState.SetCullTest( ECULLTEST_OFF );

	miNumPasses = 1;
	
	if( pTARG )
	{
		Init(pTARG);
	}
	else
	{
		FxShaderAsset* passet = nullptr;

		if( mUnManaged )
			passet = asset::AssetManager<FxShaderAsset>::LoadUnManaged( mUserFxName.c_str() );
		else
			passet = asset::AssetManager<FxShaderAsset>::Load( mUserFxName.c_str() );

		hModFX = passet ? passet->GetFxShader() : 0;

		if( hModFX )
			hModFX->SetAllowCompileFailure(mAllowCompileFailure);

	}
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::Init(ork::lev2::GfxTarget *pTarg)
{

	if( mUserFxName.length() )
	{
		FxShaderAsset* passet = nullptr;

		if( mUnManaged )
			passet = asset::AssetManager<FxShaderAsset>::LoadUnManaged( mUserFxName.c_str() );
		else
			passet = asset::AssetManager<FxShaderAsset>::Load( mUserFxName.c_str() );

		hModFX = passet ? passet->GetFxShader() : 0;

		if( hModFX )
			hModFX->SetAllowCompileFailure(mAllowCompileFailure);

	}
	else
	{
		//orkprintf( "Attempting to Load Shader<orkshader://solid>\n" );
		hModFX = asset::AssetManager<FxShaderAsset>::Load("orkshader://solid")->GetFxShader();
	}
	if( 0 == hModFX )
	{
		return;
	}
	if( mUserTekName.length() )
	{
		hTekUser = pTarg->FXI()->GetTechnique( hModFX, mUserTekName.c_str() );		

	}
	if( meColorMode != EMODE_USER )
	{
		hTekVertexColor = pTarg->FXI()->GetTechnique( hModFX, "vtxcolor" );
		hTekVertexModColor = pTarg->FXI()->GetTechnique( hModFX, "vtxmodcolor" );
		hTekModColor = pTarg->FXI()->GetTechnique( hModFX, "mmodcolor" );
		hTekTexColor = pTarg->FXI()->GetTechnique( hModFX, "texcolor" );
		hTekTexModColor = pTarg->FXI()->GetTechnique( hModFX, "texmodcolor" );
		hTekTexTexModColor = pTarg->FXI()->GetTechnique( hModFX, "textexmodcolor" );
		hTekTexVertexColor = pTarg->FXI()->GetTechnique( hModFX, "texvtxcolor" );
	}

	hMatAux = pTarg->FXI()->GetParameterH( hModFX, "MatAux" );

	hMatMVP = pTarg->FXI()->GetParameterH( hModFX, "MatMVP" );
	hMatMV = pTarg->FXI()->GetParameterH( hModFX, "MatMV" );
	hMatV = pTarg->FXI()->GetParameterH( hModFX, "MatV" );
	hMatM = pTarg->FXI()->GetParameterH( hModFX, "MatM" );
	hMatP = pTarg->FXI()->GetParameterH( hModFX, "MatP" );
	hParamModColor = pTarg->FXI()->GetParameterH( hModFX, "modcolor" );

	hVolumeMap = pTarg->FXI()->GetParameterH( hModFX, "VolumeMap" );
	hColorMap = pTarg->FXI()->GetParameterH( hModFX, "ColorMap" );
	hColorMap2 = pTarg->FXI()->GetParameterH( hModFX, "ColorMap2" );
	hColorMap3 = pTarg->FXI()->GetParameterH( hModFX, "ColorMap3" );
	hColorMap4 = pTarg->FXI()->GetParameterH( hModFX, "ColorMap4" );

	hParamUser0 = pTarg->FXI()->GetParameterH( hModFX, "User0" );
	hParamUser1 = pTarg->FXI()->GetParameterH( hModFX, "User1" );
	hParamUser2 = pTarg->FXI()->GetParameterH( hModFX, "User2" );
	hParamUser3 = pTarg->FXI()->GetParameterH( hModFX, "User3" );

	hParamTime = pTarg->FXI()->GetParameterH( hModFX, "Time" );
	
	hParamNoiseAmp = pTarg->FXI()->GetParameterH( hModFX, "NoiseAmp" );
	hParamNoiseFreq = pTarg->FXI()->GetParameterH( hModFX, "NoiseFreq" );
	hParamNoiseShift = pTarg->FXI()->GetParameterH( hModFX, "NoiseShift" );
}

/////////////////////////////////////////////////////////////////////////

bool GfxMaterial3DSolid::IsUserFxOk() const
{
	if( meColorMode == EMODE_USER )
		return (hTekUser!=nullptr);
	return false;
}

/////////////////////////////////////////////////////////////////////////

int GfxMaterial3DSolid::BeginBlock( GfxTarget *pTarg, const RenderContextInstData &MatCtx )
{
	switch( meColorMode )
	{
		case EMODE_VERTEX_COLOR:
			pTarg->FXI()->BindTechnique( hModFX, hTekVertexColor );
			break;
		case EMODE_VERTEXMOD_COLOR:
			pTarg->FXI()->BindTechnique( hModFX, hTekVertexModColor );
			break;
		case EMODE_MOD_COLOR:
			pTarg->FXI()->BindTechnique( hModFX, hTekModColor );
			break;
		case EMODE_INTERNAL_COLOR:
			pTarg->FXI()->BindTechnique( hModFX, hTekModColor );
			break;
		case EMODE_TEX_COLOR:
			pTarg->FXI()->BindTechnique( hModFX, hTekTexColor );
			break;
		case EMODE_TEXMOD_COLOR:
			pTarg->FXI()->BindTechnique( hModFX, hTekTexModColor );
			break;
		case EMODE_TEXTEXMOD_COLOR:
			pTarg->FXI()->BindTechnique( hModFX, hTekTexTexModColor );
			break;
		case EMODE_TEXVERTEX_COLOR:
			pTarg->FXI()->BindTechnique( hModFX, hTekTexVertexColor );
			break;
		case EMODE_USER:
			pTarg->FXI()->BindTechnique( hModFX, hTekUser );
			break;
	}
	int inumpasses = pTarg->FXI()->BeginBlock( hModFX, MatCtx );
	return inumpasses;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::EndBlock( GfxTarget *pTarg )
{
	pTarg->FXI()->EndBlock( hModFX );
}

/////////////////////////////////////////////////////////////////////////

static bool gbskip = false;

bool GfxMaterial3DSolid::BeginPass( GfxTarget *pTarg, int iPass )
{
	if( gbskip ) return false;
	
	const RenderContextInstData* rdata = pTarg->GetRenderContextInstData();
	const RenderContextFrameData* rfdata = pTarg->GetRenderContextFrameData();
	const CCameraData* camdata = rfdata ? rfdata->GetCameraData() : 0;
	bool bforcenoz = rdata->IsForceNoZWrite();

	//mRasterState.SetZWriteMask( ! bforcenoz );

	pTarg->RSI()->BindRasterState( mRasterState );
	pTarg->FXI()->BindPass( hModFX, iPass );

	if( hModFX->GetFailedCompile() )
		return false;

	pTarg->FXI()->BindParamMatrix( hModFX, hMatM, pTarg->MTXI()->RefMMatrix() );
	pTarg->FXI()->BindParamMatrix( hModFX, hMatMV, pTarg->MTXI()->RefMVMatrix() );
	pTarg->FXI()->BindParamMatrix( hModFX, hMatP, pTarg->MTXI()->RefPMatrix() );
	pTarg->FXI()->BindParamMatrix( hModFX, hMatMVP, pTarg->MTXI()->RefMVPMatrix() );

	pTarg->FXI()->BindParamMatrix( hModFX, hMatAux, mMatAux );

	if( hMatV )
	{
		pTarg->FXI()->BindParamMatrix( hModFX, hMatV, pTarg->MTXI()->RefVMatrix() );
	}
	
	if( pTarg->FBI()->IsPickState() )
	{
		pTarg->FXI()->BindParamVect4( hModFX, hParamModColor, pTarg->RefModColor() );
	}
	else
	{
		if( meColorMode == EMODE_INTERNAL_COLOR )
		{
			pTarg->FXI()->BindParamVect4( hModFX, hParamModColor, Color );
		}
		else
		{
			pTarg->FXI()->BindParamVect4( hModFX, hParamModColor, pTarg->RefModColor() );
		}
	}
	
	if( hParamNoiseAmp )
	{
		pTarg->FXI()->BindParamVect4( hModFX, hParamNoiseAmp, mNoiseAmp );
	}
	if( hParamNoiseFreq )
	{
		pTarg->FXI()->BindParamVect4( hModFX, hParamNoiseFreq, mNoiseFreq );
	}
	if( hParamNoiseShift )
	{
		pTarg->FXI()->BindParamVect4( hModFX, hParamNoiseShift, mNoiseShift );
	}
	
	if( hParamTime )
	{
		float reltime = std::fmod( CSystem::GetRef().GetLoResRelTime(), 300.0f );
		//printf( "reltime<%f>\n", reltime );
		pTarg->FXI()->BindParamFloat( hModFX, hParamTime, reltime );
	}

	if( hParamUser0 )
	{
		pTarg->FXI()->BindParamVect4( hModFX, hParamUser0, mUser0 );
	}
	if( hParamUser1 )
	{
		pTarg->FXI()->BindParamVect4( hModFX, hParamUser1, mUser1 );
	}
	if( hParamUser2 )
	{
		pTarg->FXI()->BindParamVect4( hModFX, hParamUser2, mUser2 );
	}
	if( hParamUser3 )
	{
		pTarg->FXI()->BindParamVect4( hModFX, hParamUser3, mUser3 );
	}

	if( mVolumeTexture && hVolumeMap )
	{
		pTarg->FXI()->BindParamCTex ( hModFX, hVolumeMap, mVolumeTexture );
	}

	if( mCurrentTexture && hColorMap )
	{
		if( IsDebug() )
			printf( "Binding texmap<%p> to param<%p>\n", mCurrentTexture, hColorMap );
		pTarg->FXI()->BindParamCTex ( hModFX, hColorMap, mCurrentTexture );
	}
	if( mCurrentTexture2 && hColorMap2 )
	{
		pTarg->FXI()->BindParamCTex ( hModFX, hColorMap2, mCurrentTexture2 );
	}

	if( mCurrentTexture3 && hColorMap3 ) 
	{
		pTarg->FXI()->BindParamCTex ( hModFX, hColorMap3, mCurrentTexture3 );
	}

	if( mCurrentTexture4 && hColorMap4 ) 
	{
		pTarg->FXI()->BindParamCTex ( hModFX, hColorMap4, mCurrentTexture4 );
	}

	pTarg->FXI()->CommitParams();
	return true;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::EndPass( GfxTarget *pTarg )
{
	if(false == gbskip)
		pTarg->FXI()->EndPass( hModFX );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

void GfxMaterial3DSolid::SetMaterialProperty( const char* prop, const char* val ) // virtual
{
	////////////////////////////////////////////////
	// colormode
	////////////////////////////////////////////////
	if( 0 == strcmp(prop,"colormode") )
	{
		if( 0 == strcmp(val,"EMODE_INTERNAL_COLOR") )
		{
			meColorMode = EMODE_INTERNAL_COLOR;
		}
	}
	////////////////////////////////////////////////
	// colormode
	////////////////////////////////////////////////
	if( 0 == strcmp(prop,"color") )
	{
		if( (strlen(val)==9) && (val[0]=='#') )
		{
			struct hexchar2int
			{
				static int doit( const char ch )
				{
					if( (ch>='a') && (ch<='f') )
					{
						return 10+(ch-'a');
					}
					else if( (ch>='0') && (ch<='9') )
					{
						return (ch-'0');
					}
					else
					{
						OrkAssert(false);
						return -1;
					}
				}
			};
			char hexd0 = val[1]; 
			char hexd1 = val[2]; 
			char hexd2 = val[3]; 
			char hexd3 = val[4]; 
			char hexd4 = val[5]; 
			char hexd5 = val[6]; 
			char hexd6 = val[7]; 
			char hexd7 = val[8]; 
			
			u32 ucolor = 0;
			ucolor |= hexchar2int::doit(hexd7)<<0;
			ucolor |= hexchar2int::doit(hexd6)<<4;
			ucolor |= hexchar2int::doit(hexd5)<<8;
			ucolor |= hexchar2int::doit(hexd4)<<12;
			ucolor |= hexchar2int::doit(hexd3)<<16;
			ucolor |= hexchar2int::doit(hexd2)<<20;
			ucolor |= hexchar2int::doit(hexd1)<<24;
			ucolor |= hexchar2int::doit(hexd0)<<28;
			printf( "color<0x%08x>\n", ucolor );
			
			Color = CVector4(ucolor);
			printf( "color<%f %f %f %f>\n", Color.GetX(), Color.GetY(), Color.GetZ(), Color.GetW() );
		}

	}


}

////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////

} }
