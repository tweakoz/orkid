////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/rtgroup.h>


#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/renderable.h>
#include <ork/lev2/gfx/lev2renderer.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/any.h>
#include <ork/application/application.h>

template class ork::orklut<ork::PoolString,anyp>;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

const RenderContextInstData RenderContextInstData::Default;

///////////////////////////////////////////////////////////////////////////////

RenderContextInstData::RenderContextInstData()
	: miMaterialIndex( 0 )
	, miMaterialPassIndex( 0 )
	, mpActiveRenderer( 0 )
	, mpDagRenderable( 0 )
	, mpLightingGroup( 0 )
	, mDPTopEnvMap( 0 )
	, mDPBotEnvMap( 0 )
	, mMaterialInst( 0 )
	, mbIsSkinned(false)
	, mbForzeNoZWrite(false)
	, mLightMap(0)
	, mbVertexLit(false)
	, mRenderGroupState( ERGST_NONE )
{
	for(int i = 0; i < kMaxEngineParamFloats; i++)
		mEngineParamFloats[i] = 0.0f;
}

void RenderContextInstData::SetEngineParamFloat(int idx, float fv)
{
	OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

	mEngineParamFloats[idx] = fv;
}

float RenderContextInstData::GetEngineParamFloat(int idx) const
{
	OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

	return mEngineParamFloats[idx];
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderFrustum( const FrustumRenderable & FrusRen ) const
{
/*	mpTarget->IMI()->QueFlush();

	bool bpickbuffer = ( mpTarget->FBI()->IsOffscreenTarget() );

	fcolor4 ObjColor;
	ObjColor.SetRGBAU32( (U32) mpCurrentQueueObject );

	fvec3 vScreenUp, vScreenRight;
	fvec2 vpdims(float(mpTarget->GetW()),float(mpTarget->GetH()));

	bool objspace = FrusRen.IsObjSpace();

	if( false == mpTarget->FBI()->IsOffscreenTarget() )
	{
		ork::lev2::GfxMaterial3DSolid StdMaterial(mpTarget);
		StdMaterial.SetColorMode( ork::lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR );
		StdMaterial.mRasterState.SetBlending( ork::lev2::EBLENDING_OFF );
		StdMaterial.mRasterState.SetDepthTest( ork::lev2::EDEPTHTEST_LEQUALS );
		StdMaterial.mRasterState.SetCullTest( ork::lev2::ECULLTEST_OFF );
		StdMaterial.mRasterState.SetZWriteMask( false );

		fcolor4 AlphaYellow( 1.0f, 1.0f, 0.0f, 0.3f );
		if( false == objspace ) mpTarget->MTXI()->PushMMatrix( fmtx4::Identity );
		{
			mpTarget->PushModColor( fvec4( FrusRen.GetColor().xyz(), 1.0f ) );
			mpTarget->BindMaterial( & StdMaterial );
			for( int i=0; i<4; i++ )
			{
				fvec4 points[2];
				points[0] = FrusRen.GetFrustum()->mNearCorners[i];
				points[1] = FrusRen.GetFrustum()->mFarCorners[i];
				//mpTarget->DrawLine( vFrom, vTo );
				mpTarget->IMI()->DrawPrim( points, 2, EPRIM_LINES );
			}
			for( int i=0; i<4; i++ )
			{
				int i2 = (i+1)%4;

				const fvec4 & vFromN		= FrusRen.GetFrustum()->mNearCorners[i];
				const fvec4 & vToN		= FrusRen.GetFrustum()->mNearCorners[i2];
				const fvec4 & vFromF		= FrusRen.GetFrustum()->mFarCorners[i];
				const fvec4 & vToF		= FrusRen.GetFrustum()->mFarCorners[i2];
				mpTarget->IMI()->DrawLine( vFromN, vToN );
				mpTarget->IMI()->DrawLine( vFromF, vToF );
				mpTarget->IMI()->QueFlush();
			}
			mpTarget->PopModColor();
			fcolor4 AlphaYellow2( 1.0f, 1.0f, 0.0f, 0.1f );
			mpTarget->PushModColor( fvec4( AlphaYellow2.xyz(), 0.3f ) );
			//mpTarget->PushModColor( fvec4( FrusRen.GetColor().xyz(), 0.3f ) );
			{
				const fvec3 * vn = FrusRen.GetFrustum()->mNearCorners;
				const fvec3 * vf = FrusRen.GetFrustum()->mFarCorners;
				fvec4 VQuad[36];

				/////////////////////////////////////////////////////////////////////////
				// NEAR
				VQuad[0] = vn[0];		VQuad[1] = vn[1];			VQuad[2] = vn[2];
				VQuad[3] = vn[3];		VQuad[4] = vn[2];			VQuad[5] = vn[0];

				/////////////////////////////////////////////////////////////////////////
				// FAR
				VQuad[6] = vf[0];		VQuad[7] = vf[1];			VQuad[8] = vf[2];
				VQuad[9] = vf[3];		VQuad[10] = vf[2];			VQuad[11] = vf[0];

				/////////////////////////////////////////////////////////////////////////
				// TOP
				VQuad[12] = vn[0];		VQuad[13] = vf[0];			VQuad[14] = vf[1];
				VQuad[15] = vn[0];		VQuad[16] = vn[1];			VQuad[17] = vf[1];

				/////////////////////////////////////////////////////////////////////////
				// L
				VQuad[18] = vn[1];		VQuad[19] = vf[1];			VQuad[20] = vf[2];
				VQuad[21] = vn[1];		VQuad[22] = vn[2];			VQuad[23] = vf[2];

				/////////////////////////////////////////////////////////////////////////
				// R
				VQuad[24] = vn[3];		VQuad[25] = vf[3];			VQuad[26] = vf[0];
				VQuad[27] = vn[3];		VQuad[28] = vn[0];			VQuad[29] = vf[0];

				/////////////////////////////////////////////////////////////////////////
				// B
				VQuad[30] = vn[2];		VQuad[31] = vf[2];			VQuad[32] = vf[3];
				VQuad[33] = vn[2];		VQuad[34] = vn[3];			VQuad[35] = vf[3];

				mpTarget->IMI()->DrawPrim( VQuad, 36, ork::lev2::EPRIM_TRIANGLES );

				mpTarget->IMI()->QueFlush();
			}
			mpTarget->PopModColor();
		}
		if( false == objspace ) mpTarget->MTXI()->PopMMatrix();
	}
	*/
}


void Renderer::RenderSphere( const SphereRenderable & SphereRen ) const
{
	/*
	mpTarget->IMI()->QueFlush();

	const fvec3& pos = SphereRen.GetPosition();
	float radius = SphereRen.GetRadius();

	bool bpickbuffer = ( mpTarget->FBI()->IsOffscreenTarget() );

	fcolor4 ObjColor;
	ObjColor.SetRGBAU32( (U32) mpCurrentQueueObject );

	fvec3 vScreenUp, vScreenRight;
	fvec2 vpdims(float(mpTarget->GetW()),float(mpTarget->GetH()));

	if( false == mpTarget->FBI()->IsOffscreenTarget() )
	{
		ork::lev2::GfxMaterial3DSolid StdMaterial(mpTarget);
		StdMaterial.SetColorMode( ork::lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR );
		StdMaterial.mRasterState.SetBlending( ork::lev2::EBLENDING_OFF );
		StdMaterial.mRasterState.SetDepthTest( ork::lev2::EDEPTHTEST_LEQUALS );
		StdMaterial.mRasterState.SetCullTest( ork::lev2::ECULLTEST_OFF );
		StdMaterial.mRasterState.SetZWriteMask( false );

		//fcolor4 AlphaYellow( 1.0f, 1.0f, 0.0f, 1.0f );

		fmtx4 M;
		M.SetScale( radius, radius, radius );
		M.SetTranslation( pos );

		mpTarget->MTXI()->PushMMatrix( M );
		mpTarget->PushModColor( fvec4( SphereRen.GetColor().xyz(), 0.3f ) );
		mpTarget->BindMaterial( & StdMaterial );
		{
			GfxPrimitives::GetRef().RenderTriCircle( mpTarget );
		}
		mpTarget->PopModColor();
		mpTarget->MTXI()->PopMMatrix();
//		fcolor4 AlphaYellow2( 1.0f, 1.0f, 0.0f, 0.1f );
//		mpTarget->PushModColor( AlphaYellow2 );
//		{
//
//		}
//		mpTarget->PopModColor();
	}

	*/
}

///////////////////////////////////////////////////////////////////////////////

/*U32 Renderer::ComposeSortKey( U32 texIndex, U32 depthIndex, U32 passIndex, U32 transIndex ) const
{
	static const u32 ktexbits = 10;
	static const u32 kpassbits = 3;
	static const u32 kdepthbits = 18;
	static const u32 ktransbits = 1;

	static const u32 ktexmask = (1<<ktexbits)-1;
	static const u32 kpassmask = (1<<kpassbits)-1;
	static const u32 kdepthmask = (1<<kdepthbits)-1;
	static const u32 ktransmask = (1<<ktransbits)-1;

	static const u32 kdepthshift = 0;
	static const u32 ktexshift = kdepthshift+kdepthbits;
	static const u32 kpassshift = ktexshift+ktexbits;
	static const u32 ktransshift = kpassshift+kpassbits;

	U32 uval	= ((transIndex & ktransmask) << ktransshift)
				| ((passIndex & kpassmask) << kpassshift)
				| ((texIndex & ktexmask) << ktexshift)
				| ((depthIndex & kdepthmask) << kdepthshift)
				;

	return 0xffffffff-uval;
}*/


RenderContextFrameData::RenderContextFrameData()
	: mpShadowBuffer( 0 )
	, meMode( ERENDMODE_STANDARD )
	, mCameraData(0)
	, mPickCameraData(0)
	, mLightManager(0)
	, mpTarget(0)
{
}

void RenderContextFrameData::SetUserProperty( const char* prop, anyp val )
{
	PoolString PSprop = AddPooledString(prop);
	orklut<PoolString,anyp>::iterator it = mUserProperties.find(PSprop);
	if( it==mUserProperties.end() )
		mUserProperties.AddSorted(PSprop,val);
	else
		it->second = val;
}
anyp RenderContextFrameData::GetUserProperty( const char* prop )
{
	PoolString PSprop = AddPooledString(prop);
	orklut<PoolString,anyp>::const_iterator it = mUserProperties.find(PSprop);
	if( it!=mUserProperties.end() )
	{
		return it->second;
	}
	anyp rval;
	return rval;
}

bool RenderContextFrameData::IsPickMode() const
{
	return mpTarget ? mpTarget->FBI()->IsPickState() : false;
}


void RenderContextFrameData::SetTarget( GfxTarget* ptarg )
{
	mpTarget = ptarg;
	if( ptarg )
	{
		ptarg->SetRenderContextFrameData( this );
	}

}

void RenderContextFrameData::ClearLayers()
{
	mLayers.clear();
}
void RenderContextFrameData::AddLayer( const PoolString& layername )
{
	mLayers.insert( layername );
}
bool RenderContextFrameData::HasLayer( const PoolString& layername ) const
{
	return (mLayers.find(layername)!=mLayers.end());
}

///////////////////////////////////////////////////////////////////////////////

void RenderContextFrameData::PushRenderTarget( IRenderTarget* ptarg )
{
	mRenderTargetStack.push(ptarg);
}
IRenderTarget* RenderContextFrameData::GetRenderTarget()
{
	IRenderTarget* pt = mRenderTargetStack.top();
	return pt;
}
void RenderContextFrameData::PopRenderTarget()
{
	mRenderTargetStack.pop();
}

///////////////////////////////////////////////////////////////////////////////

IRenderTarget::IRenderTarget()
{
}

///////////////////////////////////////////////////////////////////////////////

RtGroupRenderTarget::RtGroupRenderTarget(RtGroup* prtgroup)
	: mpRtGroup(prtgroup)
{
}
int RtGroupRenderTarget::GetW()
{
	//printf( "RtGroup W<%d> H<%d>\n", mpRtGroup->GetW(), mpRtGroup->GetH() );
	return mpRtGroup->GetW();
}
int RtGroupRenderTarget::GetH()
{
	return mpRtGroup->GetH();
}
void RtGroupRenderTarget::BeginFrame(FrameRenderer&frenderer)
{
}
void RtGroupRenderTarget::EndFrame(FrameRenderer&frenderer)
{
}

///////////////////////////////////////////////////////////////////////////////

UiViewportRenderTarget::UiViewportRenderTarget(ui::Viewport* pVP)
	: mpViewport(pVP)
{
}
int UiViewportRenderTarget::GetW()
{
	return mpViewport->GetW();
}
int UiViewportRenderTarget::GetH()
{
	return mpViewport->GetH();
}
void UiViewportRenderTarget::BeginFrame(FrameRenderer&frenderer)
{
	RenderContextFrameData&	FrameData = frenderer.GetFrameData();
	GfxTarget *pTARG = FrameData.GetTarget();
	mpViewport->BeginFrame( pTARG );
}
void UiViewportRenderTarget::EndFrame(FrameRenderer&frenderer)
{
	RenderContextFrameData&	FrameData = frenderer.GetFrameData();
	GfxTarget *pTARG = FrameData.GetTarget();
	mpViewport->EndFrame( pTARG );
}

///////////////////////////////////////////////////////////////////////////////

UiSurfaceRenderTarget::UiSurfaceRenderTarget(ui::Surface* pVP)
	: mSurface(pVP)
{
}
int UiSurfaceRenderTarget::GetW()
{
	return mSurface->GetW();
}
int UiSurfaceRenderTarget::GetH()
{
	return mSurface->GetH();
}
void UiSurfaceRenderTarget::BeginFrame(FrameRenderer&frenderer)
{
	mSurface->BeginSurface( frenderer );
}
void UiSurfaceRenderTarget::EndFrame(FrameRenderer&frenderer)
{
	mSurface->EndSurface( frenderer );
}

///////////////////////////////////////////////////////////////////////////////

static const int kFINALW = 512;
static const int kFINALH = 512;

FrameTechniqueBase::FrameTechniqueBase( int iW, int iH )
	: miW( iW )
	, miH( iH )
	, mpMrtFinal(0)
{

}

void FrameTechniqueBase::Init(GfxTarget* targ)
{
	static const int kmultisamples = 1;

	auto fbi = targ->FBI();
	GfxBuffer* parent = fbi->GetThisBuffer();
	targ = parent ? parent->GetContext() : targ;
	auto clear_color = fbi->GetClearColor();

	mpMrtFinal = new RtGroup( targ, kFINALW, kFINALH, kmultisamples );

	mpMrtFinal->SetMrt( 0, new RtBuffer(		mpMrtFinal,
												lev2::ETGTTYPE_MRT0,
												lev2::EBUFFMT_RGBA32,
												kFINALW, kFINALH ) );

	//mpMrtFinal->GetMrt(0)->RefClearColor() = clear_color;
	//mpMrtFinal->GetMrt(0)->SetContext( targ );

	DoInit(targ);
}


///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
