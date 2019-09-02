////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <ork/dataflow/dataflow.h>
#include <ork/math/misc_math.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/kernel/prop.h>
#include <ork/dataflow/scheduler.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/asset/AssetManager.h>
#include <ork/lev2/lev2_asset.h>

#include "terrain_synth.h"
#if 0

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace terrain {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int hmap_perlin_module::GetNumInputs() const { return 0; }
///////////////////////////////////////////////////////////////////////////////
int hmap_perlin_module::GetNumOutputs() const {	return 1; }
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* hmap_perlin_module::GetInput(int idx) {	return 0; }
///////////////////////////////////////////////////////////////////////////////
const dataflow::outplugbase* hmap_perlin_module::GetOutput(int idx) const
{	OrkAssert( idx==0 );
	return &mOutputPlug;
}
///////////////////////////////////////////////////////////////////////////////
hmap_perlin_module::datablock::datablock()
	: HeightMap_datablock()
	, minumoct(1)
	, mfampsca(0.5f)
	, mfampbas(1.0f)
	, mffrqsca(2.0f)
	, mffrqbas(1.0f)
	, mfmapfrq(1.0f)
	, mfmapamp(100.0f)
	, mfrotate(0.0f)
{
}
void hmap_perlin_module::datablock::Copy( const datablock& oth )
{
	this->HeightMap_datablock::Copy(oth);

	minumoct = oth.minumoct;
	mfampsca = oth.mfampsca;
	mfampbas = oth.mfampbas;
	mffrqsca = oth.mffrqsca;
	mffrqbas = oth.mffrqbas;
	mfmapfrq = oth.mfmapfrq;
	mfmapamp = oth.mfmapamp;
	mfrotate = oth.mfrotate;
}
///////////////////////////////////////////////////////////////////////////////
static const int kvbmax = 512;
hmap_perlin_module::hmap_perlin_module()
	//: //mOutputPlug(this,&mDefDataBlock.mHeightMap)
	: mVertexBuffer( kvbmax, 0, lev2::EPRIM_POINTS )
	, mpDepthModTexture(0)
	, mpNoiseModTexture(0)
{	

	mpDepthModTexture =  asset::AssetManager<ork::lev2::TextureAsset>::Load( "lev2://textures/noise01" )->GetTexture();
	mpNoiseModTexture =  asset::AssetManager<ork::lev2::TextureAsset>::Load( "lev2://textures/noise2d1_seamless.tga" )->GetTexture();
}
///////////////////////////////////////////////////////////////////////////////
void hmap_perlin_module::ComputeCPU(dataflow::workunit* wu) const
{
	#if 0
	datablock* hcw = (datablock*) wu->GetContextData();
	HeightMap& hm = hcw->mHeightMap;

	int ix1 = hcw->miX1;
	int iz1 = hcw->miZ1;
	int ix2 = hcw->miX2;
	int iz2 = hcw->miZ2;
	float frqbase = hcw->mffrqbas/hm.GetWorldSize();
	const float fSca0 = hm.GetWorldSize()/float(hm.GetGridSize());
	const float fBasX = 0.0f;
	const float fBasZ = 0.0f;
	for( int iZ=iz1; iZ<=iz2; iZ++ )
	{	F32 fz = fBasZ + ((F32) iZ * fSca0);
		for( int iX=ix1; iX<=ix2; iX++ )
		{	F32 fx = fBasX + ((F32) iX * fSca0);
			f32 fy = 0.0f;
			for( int iOctave=0; iOctave<hcw->minumoct; iOctave++ )
			{	f32 fascal = hcw->mfampbas * powf( hcw->mfampsca, (f32) (iOctave) );
				f32 ffscal = frqbase * powf( hcw->mffrqsca, (f32) (iOctave) );
				fy += CPerlin2D::PlaneNoiseFunc( fx, fz, 0.0f, 0.0f, fascal, ffscal );
			}
			hm.SetHeight(iX,iZ,fy);
		}
	}
	#endif
}
///////////////////////////////////////////////////////////////////////////
class Perlin3DMaterial : public lev2::GfxMaterial
{	public:
	
	Perlin3DMaterial(lev2::GfxTarget* pTARG, const char* puserfx, const char* pusertek );
	
	virtual ~Perlin3DMaterial() {};
	virtual void Update( void ) {}
	virtual void Init( lev2::GfxTarget *pTarg );

	void SetHeightTexture( lev2::Texture* ptex ) { mHeightTexture=ptex; }

	////////////////////////////////////////////

	virtual bool BeginPass( lev2::GfxTarget* pTARG, int iPass=0 );
	virtual void EndPass( lev2::GfxTarget* pTARG );
	virtual int BeginBlock( lev2::GfxTarget* pTARG, const lev2::RenderContextInstData &MatCtx );
	virtual void EndBlock( lev2::GfxTarget* pTARG );
		
	lev2::FxShader*		hModFX;
	lev2::Texture*		mHeightTexture;
	std::string			mUserFxName;
	std::string			mUserTekName;
	float				mMapAmp;
	float				mMapFreq;
	float				mRotate;

	const lev2::FxShaderTechnique*	hTekUser;
	
	const lev2::FxShaderParam*		hMatMV;
	const lev2::FxShaderParam*		hMatP;
	const lev2::FxShaderParam*		hMatMVP;

	const lev2::FxShaderParam*		hHeightMap;

	const lev2::FxShaderParam*		hMapFreq;
	const lev2::FxShaderParam*		hMapAmp;
	const lev2::FxShaderParam*		hModColor;

	const lev2::FxShaderParam*		hRotationMtx;
};

Perlin3DMaterial::Perlin3DMaterial(lev2::GfxTarget* pTARG, const char* puserfx, const char* pusertek )
/////////////////
	: hTekUser( 0 )
	, hMatMVP( 0 )
	, hMatMV( 0 )
	, hMatP( 0 )
/////////////////
	, hHeightMap( 0 )
	, hMapFreq( 0 )
	, hMapAmp( 0 )
	, hModColor( 0 )
	, hRotationMtx( 0 )
/////////////////
	, mHeightTexture( 0 )
	, mUserFxName(puserfx)
	, mUserTekName(pusertek)
	, mMapAmp(0.0f)
	, mMapFreq(0.0f)
	, mRotate(0.0f)
{

	mRasterState.SetShadeModel( lev2::ESHADEMODEL_SMOOTH );
	mRasterState.SetAlphaTest( lev2::EALPHATEST_OFF );
	mRasterState.SetBlending( lev2::EBLENDING_OFF );
	mRasterState.SetDepthTest( lev2::EDEPTHTEST_LEQUALS );
	mRasterState.SetZWriteMask( true );
	mRasterState.SetCullTest( lev2::ECULLTEST_OFF );

	miNumPasses = 1;

	hModFX = asset::AssetManager<lev2::FxShaderAsset>::Load( mUserFxName.c_str() )->GetFxShader();

	if( pTARG )
	{
		Init(pTARG);
	}
}

/////////////////////////////////////////////////////////////////////////

void Perlin3DMaterial::Init(ork::lev2::GfxTarget *pTarg)
{
	hTekUser = pTarg->FXI()->GetTechnique( hModFX, mUserTekName.c_str() );		

	hMatMVP = pTarg->FXI()->GetParameterH( hModFX, "MatMVP" );
	hMatMV = pTarg->FXI()->GetParameterH( hModFX, "MatMV" );
	hMatP = pTarg->FXI()->GetParameterH( hModFX, "MatP" );
	hModColor = pTarg->FXI()->GetParameterH( hModFX, "modcolor" );

	hHeightMap = pTarg->FXI()->GetParameterH( hModFX, "ColorMap" );
	hMapFreq = pTarg->FXI()->GetParameterH( hModFX, "MapFreq" );
	hMapAmp = pTarg->FXI()->GetParameterH( hModFX, "MapAmp" );

	hRotationMtx = pTarg->FXI()->GetParameterH( hModFX, "RotMtx" );
}
/////////////////////////////////////////////////////////////////////////
int Perlin3DMaterial::BeginBlock( lev2::GfxTarget *pTarg, const lev2::RenderContextInstData &MatCtx )
{
	pTarg->FXI()->BindTechnique( hModFX, hTekUser );
	int inumpasses = pTarg->FXI()->BeginBlock( hModFX, MatCtx );
	return inumpasses;
}
/////////////////////////////////////////////////////////////////////////
void Perlin3DMaterial::EndBlock( lev2::GfxTarget *pTarg )
{
	pTarg->FXI()->EndBlock( hModFX );
}
/////////////////////////////////////////////////////////////////////////
bool Perlin3DMaterial::BeginPass( lev2::GfxTarget *pTarg, int iPass )
{
	//pTarg->MTXI()->MatrixRefresh();
	pTarg->RSI()->BindRasterState( mRasterState );
	pTarg->FXI()->BindPass( hModFX, iPass );
	pTarg->FXI()->BindParamMatrix( hModFX, hMatMV, pTarg->MTXI()->RefMVMatrix() );
	pTarg->FXI()->BindParamMatrix( hModFX, hMatP, pTarg->MTXI()->RefPMatrix() );
	pTarg->FXI()->BindParamMatrix( hModFX, hMatMVP, pTarg->MTXI()->RefMVPMatrix() );

	pTarg->FXI()->BindParamFloat( hModFX, hMapAmp, mMapAmp );
	pTarg->FXI()->BindParamFloat( hModFX, hMapFreq, mMapFreq );

	pTarg->FXI()->BindParamVect4( hModFX, hModColor, pTarg->RefModColor() );
	
	float frot0 = cosf(mRotate);
	float frot1 = -sinf(mRotate);
	float frot2 = sinf(mRotate);
	float frot3 = cosf(mRotate);

	pTarg->FXI()->BindParamVect4( hModFX, hRotationMtx, fvec4(frot0,frot1,frot2,frot3) );

	if( mHeightTexture )
	{
		pTarg->FXI()->BindParamCTex ( hModFX, hHeightMap, mHeightTexture );
	}
	pTarg->FXI()->CommitParams();
	return true;
}
/////////////////////////////////////////////////////////////////////////
void Perlin3DMaterial::EndPass( lev2::GfxTarget *pTarg )
{
	pTarg->FXI()->EndPass( hModFX );
}
///////////////////////////////////////////////////////////////////////////////
void hmap_perlin_module::ComputeGPU(dataflow::workunit* wu) const
{
	#if 0
	datablock* hcw = (datablock*) wu->GetContextData();
	HeightMap& hm = hcw->mHeightMap;

	const int kWUW = hcw->miX2;
	const int kWUH = hcw->miZ2;

	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	//////////////////////////////////////////////////////////

	heightfield_compute_buffer& ComputeBuffer = HeightMapGPGPUComputeBuffer();

	ComputeBuffer.GetContext()->SetClearColor( fcolor4::Black() );
	ComputeBuffer.GetContext()->SetAutoClear(true);

	//const int BUFW = heightfield_compute_buffer::kw;
	//const int BUFH = heightfield_compute_buffer::kh;

	//hm.SetGridSize( BUFW );

	const int BUFW = hm.GetGridSize();
	const int BUFH = hm.GetGridSize();

	for( int ix1=0; ix1<kWUW; ix1+=BUFW )
	{
		int ix2 = ix1+(BUFW-1);
		if( ix2>=kWUW ) ix2 = (kWUW-1);
		const int iw = (ix2-ix1);

		float fX1 = float(ix1)/float(kWUW);
		float fX2 = float(ix2)/float(kWUW);

		for( int iz1=0; iz1<kWUH; iz1+=BUFH )
		{
			int iz2 = iz1+(BUFH-1);
			if( iz2>=kWUH ) iz2 = (kWUH-1);
			const int ih = (iz2-iz1);
		
			float fZ1 = float(iz1)/float(kWUH);
			float fZ2 = float(iz2)/float(kWUH);

			ComputeBuffer.BeginFrame();
			{
				fmtx4 MatP = ComputeBuffer.GetContext()->Ortho( 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f );		
				
				SRect VPRECT( 0, 0, BUFW, BUFH );

				Perlin3DMaterial matsolid( ComputeBuffer.GetContext(), "miniorkshader://heightmap_edit", "texdepthcolor" );
				
				matsolid.mMapAmp = hcw->mfmapamp;
				matsolid.mMapFreq = hcw->mfmapfrq;

				matsolid.Init( ComputeBuffer.GetContext() );
				//matsolid.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_TEXDEPTH_COLOR );
				matsolid.mRasterState.SetBlending( lev2::EBLENDING_ADDITIVE ); // EBLENDING_ADDITIVE
				matsolid.mRasterState.SetDepthTest( lev2::EDEPTHTEST_OFF );

				ComputeBuffer.GetContext()->PushPMatrix( MatP );
				ComputeBuffer.GetContext()->PushVMatrix( fmtx4::Identity );
				ComputeBuffer.GetContext()->PushMMatrix( fmtx4::Identity );
				ComputeBuffer.GetContext()->BindMaterial( & matsolid );
				ComputeBuffer.GetContext()->PushViewport(VPRECT);
				ComputeBuffer.GetContext()->PushScissor(VPRECT);
				{
					int idx = 0;

					lev2::SVtxV12C4T8 v0;
					lev2::SVtxV12C4T8 v1;
					lev2::SVtxV12C4T8 v2;
					lev2::SVtxV12C4T8 v3;

					v0.miX = 0.0f;
					v0.miY = 0.0f;

					v1.miX = 1.0f;
					v1.miY = 0.0f;

					v2.miX = 1.0f;
					v2.miY = 1.0f;

					v3.miX = 0.0f;
					v3.miY = 1.0f;

					float frqbase = hcw->mffrqbas; ///hm.GetWorldSize();
					const float fSca0 = hm.GetWorldSize()/float(hm.GetGridSize());
					const float fBasX = 0.0f;
					const float fBasZ = 0.0f;

					float fsca = 0.5f;

					v0.miZ = 0.5f;
					v1.miZ = 0.5f;
					v2.miZ = 0.5f;
					v3.miZ = 0.5f;
					v0.muColor = 0xffffffff;
					v1.muColor = 0xffffffff;
					v2.muColor = 0xffffffff;
					v3.muColor = 0xffffffff;

					//////////////////////////////////////////////////////////////
					//////////////////////////////////////////////////////////////
					// draw 1st base layer

					//////////////////////////////////////////////////////////////
					// set texsize for texture filters for depthmap
					float texw( mpDepthModTexture ? mpDepthModTexture->GetWidth() : 1.0f );
					fcolor4 CubicFilterParams( float(ix2), float(iz2), texw, 1.0f/texw );
					//////////////////////////////////////////////////////////////
					ComputeBuffer.GetContext()->PushModColor( CubicFilterParams );
					matsolid.mHeightTexture=mpDepthModTexture;
					if( (mpNoiseModTexture!=0) && (mpDepthModTexture!=0) )
					{
						v0.mfU = fX1;
						v0.mfV = fZ1;
						v1.mfU = fX2;
						v1.mfV = fZ1;
						v2.mfU = fX2;
						v2.mfV = fZ2;
						v3.mfU = fX1;
						v3.mfV = fZ2;
						
						ComputeBuffer.GetContext()->VtxBuf_Lock( mVertexBuffer );
						{
							mVertexBuffer.AddVertex( v0 );
							mVertexBuffer.AddVertex( v1 );
							mVertexBuffer.AddVertex( v2 );

							mVertexBuffer.AddVertex( v0 );
							mVertexBuffer.AddVertex( v2 );
							mVertexBuffer.AddVertex( v3 );
						}
						ComputeBuffer.GetContext()->VtxBuf_UnLock( mVertexBuffer );
						ComputeBuffer.GetContext()->DrawPrimitive( mVertexBuffer, lev2::EPRIM_TRIANGLES );
					}
					ComputeBuffer.GetContext()->PopModColor();

					//////////////////////////////////////////////////////////////
					//////////////////////////////////////////////////////////////
					// draw detail layers

					matsolid.mHeightTexture = mpNoiseModTexture;

					//////////////////////////////////////////////////////////////
					// set texsize for texture filters for noismap
					texw = mpNoiseModTexture ? mpNoiseModTexture->GetWidth() : 1.0f;
					CubicFilterParams = fcolor4( float(ix2), float(iz2), texw, 1.0f/texw );
					//////////////////////////////////////////////////////////////
					ComputeBuffer.GetContext()->PushModColor( CubicFilterParams );
					{
						for( int iOctave=0; iOctave<hcw->minumoct; iOctave++ )
						{	
							matsolid.mRotate = mDefDataBlock.mfrotate*float(iOctave);
	
							f32 fascal = hcw->mfampbas * powf( hcw->mfampsca, (f32) (iOctave) );
							f32 ffscal = frqbase * powf( hcw->mffrqsca, (f32) (iOctave) );
					
							matsolid.mMapAmp = fascal;
							matsolid.mMapFreq = ffscal;

							v0.mfU = fX1;
							v0.mfV = fZ1;
							v1.mfU = fX2;
							v1.mfV = fZ1;
							v2.mfU = fX2;
							v2.mfV = fZ2;
							v3.mfU = fX1;
							v3.mfV = fZ2;
							
							ComputeBuffer.GetContext()->VtxBuf_Lock( mVertexBuffer );
							{
								mVertexBuffer.AddVertex( v0 );
								mVertexBuffer.AddVertex( v1 );
								mVertexBuffer.AddVertex( v2 );

								mVertexBuffer.AddVertex( v0 );
								mVertexBuffer.AddVertex( v2 );
								mVertexBuffer.AddVertex( v3 );
							}
							ComputeBuffer.GetContext()->VtxBuf_UnLock( mVertexBuffer );
							ComputeBuffer.GetContext()->DrawPrimitive( mVertexBuffer, lev2::EPRIM_TRIANGLES );
						}
					}
					ComputeBuffer.GetContext()->PopModColor();
					
					//////////////////////////////////////////////////////////////
					//////////////////////////////////////////////////////////////

				}
				ComputeBuffer.GetContext()->PopScissor();
				ComputeBuffer.GetContext()->PopViewport();
				ComputeBuffer.GetContext()->PopMMatrix();
				ComputeBuffer.GetContext()->PopVMatrix();
				ComputeBuffer.GetContext()->PopPMatrix();
			}
			ComputeBuffer.EndFrame();

			lev2::CaptureBuffer MyCaptureBuffer;
			ComputeBuffer.GetContext()->Capture( MyCaptureBuffer );

			const fvec4* VecBuffer = (const fvec4*) MyCaptureBuffer.GetData();

			///////////////////////////////////////////

			for( int iZ=0; iZ<BUFW; iZ++ )
			{	
				for( int iX=0; iX<BUFH; iX++ )
				{	
					int index = MyCaptureBuffer.CalcDataIndex( iX, iZ );

					const fvec4& v = VecBuffer[ index ];
					float fy = v.GetX();
					hm.SetHeight(iX+ix1,iZ+iz1,fy);
				}
			}

			///////////////////////////////////////////

		}	// for( int iz1=0; (iz1+BUFH)<kWUH; iz1+=BUFH )
	}		// for( int ix1=0; (ix1+BUFW)<kWUW; ix1+=BUFW )
	

	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	//////////////////////////////////////////////////////////
#endif
}
///////////////////////////////////////////////////////////////////////////////
void hmap_perlin_module::Compute(dataflow::workunit* wu)
{
	#if 0
	bool bGPU = (wu->GetAffinity()&dataflow::scheduler::GpuAffinity) != 0;

	if( bGPU )
	{
		ComputeGPU( wu );
	}
	else
	{
		ComputeCPU( wu );
	}
	#endif
}
///////////////////////////////////////////////////////////////////////////////
void hmap_perlin_module::SetNumOctaves( int inumo )
{	if( inumo!=mDefDataBlock.minumoct ) mOutputPlug.SetDirty(true);
	mDefDataBlock.minumoct = inumo;
}
void hmap_perlin_module::SetOctaveAmpScale( float oas )
{	if( oas!=mDefDataBlock.mfampsca ) mOutputPlug.SetDirty(true);
	mDefDataBlock.mfampsca=oas;
}
void hmap_perlin_module::SetOctaveFrqScale( float ofs )
{	if( ofs!=mDefDataBlock.mffrqsca ) mOutputPlug.SetDirty(true);
	mDefDataBlock.mffrqsca=ofs;
}
void hmap_perlin_module::SetAmpBase( float ab )
{	if( ab!=mDefDataBlock.mfampbas ) mOutputPlug.SetDirty(true);
	mDefDataBlock.mfampbas=ab;
}
void hmap_perlin_module::SetFrqBase( float fb )
{	if( fb!=mDefDataBlock.mffrqbas ) mOutputPlug.SetDirty(true);
	mDefDataBlock.mffrqbas=fb;
}
void hmap_perlin_module::SetMapFrq( float fb )
{	if( fb!=mDefDataBlock.mfmapfrq ) mOutputPlug.SetDirty(true);
	mDefDataBlock.mfmapfrq=fb;
}
void hmap_perlin_module::SetMapAmp( float fb )
{	if( fb!=mDefDataBlock.mfmapamp ) mOutputPlug.SetDirty(true);
	mDefDataBlock.mfmapamp=fb;
}
///////////////////////////////////////////////////////////////////////////////
void hmap_perlin_module::DoDivideWork( const dataflow::scheduler& sch, dataflow::cluster* clus ) const
{	
	#if 0
	int inumcpu_processors = sch.GetNumProcessors( dataflow::scheduler::CpuAffinity );
	int inumgpu_processors = sch.GetNumProcessors( dataflow::scheduler::GpuAffinity );

	int isiz = mDefDataBlock.mHeightMap.GetGridSize();

	////////////////////////////////////////////
	if( inumgpu_processors ) // set up on the GPU
	////////////////////////////////////////////
	{
		datablock* hcw = OrkNew datablock;
		hcw->Copy( mDefDataBlock );

		hcw->miX1 = 0;
		hcw->miZ1 = 0;
		hcw->miX2 = isiz-1;
		hcw->miZ2 = isiz-1;

		dataflow::workunit* wu = OrkNew dataflow::workunit(this,clus,0);
		wu->SetContextData(hcw);
		wu->SetAffinity( dataflow::scheduler::GpuAffinity );
		clus->AddWorkUnit(wu);

	}
	////////////////////////////////////////////
	else
	////////////////////////////////////////////
	switch( inumcpu_processors )
	{	case 1:
		{	datablock* hcw = OrkNew datablock;
			hcw->Copy( mDefDataBlock );

			hcw->miX1 = 0;
			hcw->miZ1 = 0;
			hcw->miX2 = isiz-1;
			hcw->miZ2 = isiz-1;

			dataflow::workunit* wu = OrkNew dataflow::workunit(this,clus,0);
			wu->SetContextData(hcw);
			wu->SetAffinity( dataflow::scheduler::CpuAffinity );
			clus->AddWorkUnit(wu);
			break;
		}
		case 2:
		case 3:
		{	datablock* hcw1 = OrkNew datablock;
			{
				hcw1->miX1 = 0;			hcw1->miZ1 = 0;
				hcw1->miX2 = (isiz-1);	hcw1->miZ2 = (isiz>>1)-1;
			}
			datablock* hcw2 = OrkNew datablock;
			{
				hcw2->miX1 = 0;			hcw2->miZ1 = (isiz>>1);
				hcw2->miX2 = (isiz-1);	hcw2->miZ2 = (isiz-1);
			}
			hcw1->Copy( mDefDataBlock );
			hcw2->Copy( mDefDataBlock );
			dataflow::workunit* wu1 = OrkNew dataflow::workunit(this,clus,0);
			dataflow::workunit* wu2 = OrkNew dataflow::workunit(this,clus,1);
			wu1->SetContextData(hcw1);
			wu2->SetContextData(hcw2);
			wu1->SetAffinity( dataflow::scheduler::CpuAffinity );
			wu2->SetAffinity( dataflow::scheduler::CpuAffinity );
			clus->AddWorkUnit(wu1);
			clus->AddWorkUnit(wu2);
			break;
		}
		default:
			OrkAssert(false);
	}
	#endif
}
///////////////////////////////////////////////////////////////////////////////
void hmap_perlin_module::CombineWork( const dataflow::cluster* clus )
{
	#if 0
	const LockedResource< orkvector<dataflow::workunit*> >& WorkUnits = clus->GetWorkUnits();
	
	const orkvector<dataflow::workunit*>& wuvect = WorkUnits.LockForRead();

	int inumwu = wuvect.size();

	for( int i=0; i<inumwu; i++ )
	{
		dataflow::workunit* wu = wuvect[i];

		if( wu->GetModule() == this )
		{
			int wuidx = wu->GetModuleWuIndex();

			datablock* hcw = (datablock*) wu->GetContextData();

			if( 0 == wuidx ) // set my size from this
			{
				mDefDataBlock.mHeightMap.SetGridSize( hcw->mHeightMap.GetGridSize() );
				mDefDataBlock.mHeightMap.SetWorldSize( hcw->mHeightMap.GetWorldSize() );
				mDefDataBlock.mHeightMap.ResetMinMax();
			}

			for( int iz=hcw->miZ1; iz<=hcw->miZ2; iz++ )
			{
				for( int ix=hcw->miX1; ix<=hcw->miX2; ix++ )
				{
					float fh = hcw->mHeightMap.GetHeight( ix, iz );
					mDefDataBlock.mHeightMap.SetHeight( ix, iz, fh );
				}
			}
		}
	}
	WorkUnits.UnLock();
	mOutputPlug.SetDirty(false);
	#endif
}
///////////////////////////////////////////////////////////////////////////////
void hmap_perlin_module::ReleaseWorkUnit( dataflow::workunit* wu )
{
	//HeightMap_datablock* hcw = (HeightMap_datablock*) wu->GetContextData();
	//OrkDelete hcw;
	//dgmodule::ReleaseWorkUnit( wu );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void hmap_perlin_module::SetDepthTexture(lev2::Texture*ptex)
{
	mOutputPlug.SetDirty(true);
	mpDepthModTexture=ptex;
}
void hmap_perlin_module::SetNoiseTexture(lev2::Texture*ptex)
{
	mOutputPlug.SetDirty(true);
	mpNoiseModTexture=ptex;
}
void hmap_perlin_module::SetRotate(float fv)
{
	mOutputPlug.SetDirty(true);
	mDefDataBlock.mfrotate=fv;
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
#endif