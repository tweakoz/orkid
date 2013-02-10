////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxanim.h>
#include <ork/lev2/gfx/gfxmaterial_basic.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/camera/cameraman.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/kernel/Array.hpp>
#include <ork/kernel/orkpool.h>
#include <ork/gfx/camera.h>
#include <ork/lev2/lev2_asset.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterialWiiBasic, "MaterialBasic" )
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::WiiMatrixApplicator, "WiiMatrixApplicator" )
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::WiiMatrixBlockApplicator, "WiiMatrixBlockApplicator" )

namespace ork { namespace lev2 {

void WiiMatrixApplicator::Describe() {}
void WiiMatrixBlockApplicator::Describe() {}

void GfxMaterialWiiBasic::Describe()
{

}

static bool gbenable = true; // disable since wii port is gone

/////////////////////////////////////////////////////////////////////////

GfxMaterialWiiBasic::GfxMaterialWiiBasic( const char* bastek )
	: mSpecularPower( 1.0f )
	, mBasicTechName( bastek )
	, mEmissiveColor( 0.0f, 0.0f, 0.0f, 0.0f )
	, hTekModVtxTex( 0 )
	, hTekMod( 0 )
	, hMatMV( 0 )
	, hMatP( 0 )
	, hWVPMatrix( 0 )
	, hVPMatrix( 0 )
	, hWMatrix( 0 )
	, hIWMatrix( 0 )
	, hVMatrix(0)
	, hWRotMatrix( 0 )
	, hDiffuseMapMatrix( 0 )
	, hNormalMapMatrix( 0 )
	, hSpecularMapMatrix( 0 )
	, hBoneMatrices( 0 )
	, hDiffuseTEX( 0 )
	, hSpecularTEX( 0 )
	, hAmbientTEX( 0 )
	, hNormalTEX( 0 )
	, hEmissiveColor( 0 )
	, hWCamLoc( 0 )
	, hSpecularPower( 0 )
	, hMODCOLOR( 0 )
	, hTIME( 0 )
	, hTopEnvTEX(0)
	, hBotEnvTEX(0)
{
	if( gbenable )
	{
		miNumPasses = 1;

		mRasterState.SetShadeModel( ESHADEMODEL_SMOOTH );
		mRasterState.SetAlphaTest( EALPHATEST_GREATER, 0.5f );
		mRasterState.SetBlending( EBLENDING_OFF );
		mRasterState.SetDepthTest( EDEPTHTEST_LEQUALS );
		mRasterState.SetZWriteMask( true );
		mRasterState.SetCullTest( ECULLTEST_PASS_FRONT );
	}
}

///////////////////////////////////////////////////////////////////////////////

const orkmap<std::string,std::string> GfxMaterialWiiBasic::mBasicTekMap;
const orkmap<std::string,std::string> GfxMaterialWiiBasic::mPickTekMap;

void GfxMaterialWiiBasic::StaticInit( )
{
	if( gbenable )
	{
		orkmap<std::string,std::string>& BasicTekMap = const_cast<orkmap<std::string,std::string>&>(mBasicTekMap);
		orkmap<std::string,std::string>& PickTekMap = const_cast<orkmap<std::string,std::string>&>(mPickTekMap);

		BasicTekMap["/pick"] = "tek_modcolor";
		BasicTekMap["/modvtx"] = "tek_wnormal";
		PickTekMap["/modvtx"] = "tek_modcolor";
		BasicTekMap["/lambert/tex/skinned"] = "tek_wnormal";
		PickTekMap["/lambert/tex/skinned"] = "tek_modcolor";
		BasicTekMap["/lambert/tex"] = "tek_wnormal";
		PickTekMap["/lambert/tex"] = "tek_modcolor";
	}
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::Init(ork::lev2::GfxTarget* pTarg)
{
	if( gbenable )
	{
		hModFX = asset::AssetManager<FxShaderAsset>::Load( "orkshader://basic" )->GetFxShader();

		//////////////////////////////////////////////////////////

		if( mBasicTechName == "" )
		{
			std::string& btek = const_cast<std::string&>(mBasicTechName);
			btek = "/modvtx";
		}

		printf( "GfxMaterialWiiBasic<%p> mBasicTechName<%s>\n", this, mBasicTechName.c_str() );

		orkmap<std::string,std::string>::const_iterator itb = mBasicTekMap.find( mBasicTechName );
		orkmap<std::string,std::string>::const_iterator itp = mPickTekMap.find( mBasicTechName );

		OrkAssert( itb!=mBasicTekMap.end() );
		OrkAssert( itp!=mPickTekMap.end() );
		
		printf( "GfxMaterialWiiBasic<%p> btek<%s> ptek<%s>\n", this, itb->second.c_str(), itp->second.c_str() );

		hTekModVtxTex = pTarg->FXI()->GetTechnique( hModFX, itb->second.c_str() );
		hTekMod = pTarg->FXI()->GetTechnique( hModFX, itp->second.c_str() );

		//////////////////////////////////////////
		// matrices

		hMatMV = pTarg->FXI()->GetParameterH( hModFX, "WVMatrix" );
		hMatP = pTarg->FXI()->GetParameterH( hModFX, "PMatrix" );

		hWVPMatrix = pTarg->FXI()->GetParameterH( hModFX, "WVPMatrix" );
		hVPMatrix = pTarg->FXI()->GetParameterH( hModFX, "VPMatrix" );

		hWMatrix = pTarg->FXI()->GetParameterH( hModFX, "WMatrix" );
		hIWMatrix = pTarg->FXI()->GetParameterH( hModFX, "IWMatrix" );

		hVMatrix = pTarg->FXI()->GetParameterH( hModFX, "VMatrix" );
		hWRotMatrix = pTarg->FXI()->GetParameterH( hModFX, "WRotMatrix" );
		hDiffuseMapMatrix = pTarg->FXI()->GetParameterH( hModFX, "DiffuseMapMatrix" );
		hNormalMapMatrix = pTarg->FXI()->GetParameterH( hModFX, "NormalMapMatrix" );
		hSpecularMapMatrix = pTarg->FXI()->GetParameterH( hModFX, "SpecularMapMatrix" );

		hBoneMatrices = pTarg->FXI()->GetParameterH( hModFX, "BoneMatrices" );

		//////////////////////////////////////////
		// Textures

		hDiffuseTEX = pTarg->FXI()->GetParameterH( hModFX, "DiffuseMap" );
		hSpecularTEX = pTarg->FXI()->GetParameterH( hModFX, "SpecularMap" );
		hAmbientTEX = pTarg->FXI()->GetParameterH( hModFX, "AmbientMap" );
		hNormalTEX = pTarg->FXI()->GetParameterH( hModFX, "NormalMap" );

		hTopEnvTEX = pTarg->FXI()->GetParameterH( hModFX, "TopEnvMap" );
		hBotEnvTEX = pTarg->FXI()->GetParameterH( hModFX, "BotEnvMap" );

		//////////////////////////////////////////
		// data driven material props

		hEmissiveColor = pTarg->FXI()->GetParameterH( hModFX, "EmissiveColor" );

		//////////////////////////////////////////
		// misc

		hWCamLoc = pTarg->FXI()->GetParameterH( hModFX, "WCamLoc" );
		hSpecularPower = pTarg->FXI()->GetParameterH( hModFX, "SpecularPower" );
		hMODCOLOR = pTarg->FXI()->GetParameterH( hModFX, "modcolor" );
		hTIME = pTarg->FXI()->GetParameterH( hModFX, "time" );

		mLightingInterface.Init( hModFX );
	}
}

///////////////////////////////////////////////////////////////////////////////

static CMatrix4 BuildTextureMatrix( const TextureContext & Ctx )
{
	CMatrix4 MapMatrix;
	MapMatrix.Scale( Ctx.mfRepeatU, Ctx.mfRepeatV, 1.0f );
	return MapMatrix;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static ork::fixed_pool<WiiMatrixBlockApplicator,1> MtxBlockApplicators;
static ork::fixed_pool<WiiMatrixApplicator,256> MtxApplicators;

///////////////////////////////////////////////////////////////////////////////

WiiMatrixBlockApplicator::WiiMatrixBlockApplicator( MaterialInstItemMatrixBlock*	mtxblockitem, const GfxMaterialWiiBasic* pmat )
	: mMatrixBlockItem( mtxblockitem )
	, mMaterial( pmat )
{
}

///////////////////////////////////////////////////////////////////////////////

void WiiMatrixBlockApplicator::ApplyToTarget( GfxTarget *pTARG ) // virtual
{
	size_t inumbones = mMatrixBlockItem->GetNumMatrices();
	const CMatrix4* Matrices =  mMatrixBlockItem->GetMatrices();
	FxShader* hshader = mMaterial->hModFX;

	pTARG->FXI()->BindParamMatrix( hshader, mMaterial->hMatMV, pTARG->MTXI()->RefMVMatrix() );
	pTARG->FXI()->BindParamMatrix( hshader, mMaterial->hWVPMatrix, pTARG->MTXI()->RefMVPMatrix() );
	pTARG->FXI()->BindParamMatrix( hshader, mMaterial->hWMatrix, pTARG->MTXI()->RefMMatrix() );

	CMatrix4 iwmat;
	iwmat.GEMSInverse(pTARG->MTXI()->RefMVMatrix());
	pTARG->FXI()->BindParamMatrix( hshader, mMaterial->hIWMatrix, iwmat );

	pTARG->FXI()->BindParamMatrixArray( hshader, mMaterial->hBoneMatrices, Matrices, (int) inumbones );
	pTARG->FXI()->CommitParams();
}

///////////////////////////////////////////////////////////////////////////////

WiiMatrixApplicator::WiiMatrixApplicator( MaterialInstItemMatrix* mtxitem, const GfxMaterialWiiBasic* pmat )
	: mMatrixItem(mtxitem)
	, mMaterial( pmat )
{
}

void WiiMatrixApplicator::ApplyToTarget( GfxTarget *pTARG )
{
	const CMatrix4& mtx = mMatrixItem->GetMatrix();
	FxShader* hshader = mMaterial->hModFX;
	pTARG->FXI()->BindParamMatrix( hshader, mMaterial->hDiffuseMapMatrix, mtx );
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::BindMaterialInstItem( MaterialInstItem* pitem ) const
{
	///////////////////////////////////

	MaterialInstItemMatrixBlock* mtxblockitem = rtti::autocast(pitem);

	if( mtxblockitem )
	{
		if( hBoneMatrices->GetPlatformHandle() )
		{
			WiiMatrixBlockApplicator* pyo = MtxBlockApplicators.allocate();
			OrkAssert( pyo!= 0 );
			new (pyo) WiiMatrixBlockApplicator( mtxblockitem, this );
			mtxblockitem->SetApplicator( pyo );
		}
		return;
	}

	///////////////////////////////////

	MaterialInstItemMatrix* mtxitem = rtti::autocast(pitem);

	if( mtxitem )
	{
		WiiMatrixApplicator* pyo =  MtxApplicators.allocate();
		OrkAssert( pyo!= 0 );
		new (pyo) WiiMatrixApplicator( mtxitem, this );
		mtxitem->SetApplicator( pyo );
		return;
	}

	///////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::UnBindMaterialInstItem( MaterialInstItem* pitem ) const
{
	///////////////////////////////////

	MaterialInstItemMatrixBlock* mtxblockitem = rtti::autocast(pitem);

	if( mtxblockitem )
	{
		if( hBoneMatrices->GetPlatformHandle() )
		{
			WiiMatrixBlockApplicator* wiimtxblkapp = rtti::autocast(mtxblockitem->mApplicator);
			if( wiimtxblkapp )
			{
				MtxBlockApplicators.deallocate( wiimtxblkapp );
			}
		}
		return;
	}

	///////////////////////////////////

	MaterialInstItemMatrix* mtxitem = rtti::autocast(pitem);

	if( mtxitem )
	{
		WiiMatrixApplicator* wiimtxapp = rtti::autocast(mtxitem->mApplicator);
		if( wiimtxapp )
		{
			MtxApplicators.deallocate( wiimtxapp );
		}
		return;
	}

	///////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::UpdateMVPMatrix( GfxTarget *pTARG )
{
	pTARG->FXI()->BindParamMatrix( hModFX, hMatMV, pTARG->MTXI()->RefMVMatrix() );
	//pTarg->FXI()->BindParamMatrix( hModFX, hMatP, pTarg->MTXI()->RefPMatrix() );
	pTARG->FXI()->BindParamMatrix( hModFX, hWVPMatrix, pTARG->MTXI()->RefMVPMatrix() );
	//pTarg->FXI()->BindParamMatrix( hModFX, hVPMatrix, pTarg->MTXI()->RefVPMatrix() );
	//pTarg->FXI()->BindParamMatrix( hModFX, hVMatrix, pTarg->MTXI()->RefVMatrix() );
	pTARG->FXI()->BindParamMatrix( hModFX, hWMatrix, pTARG->MTXI()->RefMMatrix() );
	pTARG->FXI()->BindParamMatrix( hModFX, hWRotMatrix, pTARG->MTXI()->RefR3Matrix() );
	pTARG->FXI()->CommitParams();
}

bool GfxMaterialWiiBasic::BeginPass( GfxTarget *pTarg, int iPass )
{
	//if( 0 ) return false;

	const RenderContextInstData* rdata = pTarg->GetRenderContextInstData();
	const RenderContextFrameData* rfdata = pTarg->GetRenderContextFrameData();
	const CCameraData* camdata = rfdata ? rfdata->GetCameraData() : 0;

	bool bforcenoz = rdata->IsForceNoZWrite();

	const ork::lev2::XgmMaterialStateInst* matinst = rdata->GetMaterialInst();

	bool bpick = pTarg->FBI()->IsPickState();

	const TextureContext & DiffuseCtx = GetTexture( ETEXDEST_DIFFUSE );

	const Texture* diftexture = DiffuseCtx.mpTexture;

	//CMatrix4 ivmat = pTarg->MTXI()->RefVMatrix();

	CColor4 ModColor = pTarg->RefModColor();

	mRasterState.SetZWriteMask( ! bforcenoz );

	pTarg->RSI()->BindRasterState( mRasterState );

	pTarg->FXI()->BindPass( hModFX, iPass );
	pTarg->FXI()->BindParamMatrix( hModFX, hMatMV, pTarg->MTXI()->RefMVMatrix() );
	pTarg->FXI()->BindParamMatrix( hModFX, hMatP, pTarg->MTXI()->RefPMatrix() );
	pTarg->FXI()->BindParamMatrix( hModFX, hWVPMatrix, pTarg->MTXI()->RefMVPMatrix() );
	pTarg->FXI()->BindParamMatrix( hModFX, hVPMatrix, pTarg->MTXI()->RefVPMatrix() );
	pTarg->FXI()->BindParamMatrix( hModFX, hVMatrix, pTarg->MTXI()->RefVMatrix() );
	pTarg->FXI()->BindParamMatrix( hModFX, hWMatrix, pTarg->MTXI()->RefMMatrix() );

	pTarg->FXI()->BindParamMatrix( hModFX, hWRotMatrix, pTarg->MTXI()->RefR3Matrix() );
	pTarg->FXI()->BindParamMatrix( hModFX, hDiffuseMapMatrix, BuildTextureMatrix(DiffuseCtx) );

	pTarg->FXI()->BindParamVect4( hModFX, hMODCOLOR, ModColor );

	pTarg->FXI()->BindParamCTex( hModFX, hDiffuseTEX, diftexture );

	////////////////////////////////////////////////////////////

	if( matinst )
	{
		int inumitems = matinst->GetNumItems();

		if( inumitems )
		{
			for( int ii=0; ii<inumitems; ii++ )
			{
				MaterialInstItem* item = matinst->GetItem(ii);
				MaterialInstApplicator*	appl = item->mApplicator;
				if( appl )
				{
					appl->ApplyToTarget( pTarg );
				}
			}
		}
	}

	////////////////////////////////////////////////////////////

	mLightingInterface.ApplyLighting( pTarg, iPass );

	pTarg->FXI()->CommitParams();

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::EndPass( GfxTarget *pTarg )
{
	pTarg->FXI()->EndPass( hModFX );
}

///////////////////////////////////////////////////////////////////////////////

int GfxMaterialWiiBasic::BeginBlock( GfxTarget *pTarg, const RenderContextInstData &MatCtx )
{
	bool bpick = pTarg->FBI()->IsPickState();

	pTarg->FXI()->BindTechnique( hModFX, bpick ? hTekMod : hTekModVtxTex );
	int inumpasses = pTarg->FXI()->BeginBlock( hModFX, MatCtx );

	mRenderContexInstData = & MatCtx;

	const ork::lev2::RenderContextFrameData* framedata = pTarg->GetRenderContextFrameData();
	const ork::CCameraData* cdata = framedata->GetCameraData();

	mScreenZDir = cdata->GetZNormal();

	return inumpasses;
}


///////////////////////////////////////////////////////////////////////////////

void GfxMaterialWiiBasic::EndBlock( GfxTarget *pTarg )
{
	pTarg->FXI()->EndBlock( hModFX );
	mRenderContexInstData = 0;
}

} }
