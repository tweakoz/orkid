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

///////////////////////////////////////////////////////////////////////////////

bool sbExit = false;

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CTXBASE, "Lev2CTXBASE");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxTarget, "GfxTarget")

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

void CTXBASE::Describe()
{
	RegisterAutoSlot( ork::lev2::CTXBASE, Repaint );
}

///////////////////////////////////////////////////////////////////////////////

CTXBASE::CTXBASE( GfxWindow* pwin )
	: mbInitialize( true )
	, mpGfxWindow( pwin )
	, mpTarget( 0 )
	, UIEvent( new CUIEvent )
	, ConstructAutoSlot(Repaint)

{
	SetupSignalsAndSlots();
    mpGfxWindow->mpCTXBASE = this;
}

///////////////////////////////////////////////////////////////////////////////

void GfxTarget::BeginFrame( void )
{
	FBI()->BeginFrame();
	RSI()->ClearOverrides();
	GBI()->BeginFrame();
	FXI()->BeginFrame();
//	IMI()->BeginFrame();

	if( GfxEnv::GetRef().GetDefaultUIMaterial() )
		BindMaterial( GfxEnv::GetRef().GetDefaultUIMaterial() );

	PushModColor( CColor4::White() );
	MTXI()->PushMMatrix( CMatrix4::Identity );
	MTXI()->PushVMatrix( CMatrix4::Identity );
	MTXI()->PushPMatrix( CMatrix4::Identity );

	mpCurrentObject = 0;

	mRenderContextInstData = 0;

	DoBeginFrame();

}

///////////////////////////////////////////////////////////////////////////////

void GfxTarget::EndFrame( void )
{
//	IMI()->EndFrame();
	GBI()->EndFrame();
	MTXI()->PopMMatrix();
	MTXI()->PopVMatrix();
	MTXI()->PopPMatrix();
	FBI()->EndFrame();

	BindMaterial(0);
	PopModColor();
	mbPostInitializeContext = false;
	DoEndFrame();
}

/////////////////////////////////////////////////////////////////////////

GfxTarget::GfxTarget()
	: miModColorStackIndex( 0 )
	, mbPostInitializeContext(true)
	, mCtxBase( 0 )
	, mpCurrentObject( 0 )
	, mFramePerfItem( CreateFormattedString( "<target:%p>", this ) )
	, miX( 0 )
	, miY( 0 )
	, miW( 0 )
	, miH( 0 )
	, miTargetFrame( 0 )
	, mRenderContextInstData( 0 )
	, mbDeviceAvailable(true)
	, miDrawLock(0)
	, mPlatformHandle(0)
{
	//CPerformanceTracker::GetRef().AddItem( mFramePerfItem );
	ork::lev2::GfxEnv::GetRef().SetLoaderTarget( this ) ;
}

///////////////////////////////////////////////////////////////////////////////

GfxTarget::~GfxTarget()
{
}

///////////////////////////////////////////////////////////////////////////////

orkvector<DisplayMode *> GfxTarget::mDisplayModes;

bool GfxTarget::SetDisplayMode(unsigned int index)
{
	if(index < mDisplayModes.size())
		return SetDisplayMode(mDisplayModes[index]);
	return false;
}

///////////////////////////////////////////////////////////////////////////////

RtGroup::RtGroup( GfxBuffer* ppar, int iW, int iH, int iSamples )
	: mParent( static_cast<GfxBuffer*>( ppar ) )
	, miW( iW )
	, miH( iH )
	, mNumMrts( 0 )
	, mInternalHandle( 0 )
	, miSamples(iSamples)
	, mDepth( 0 )
	, mbSizeDirty(true)
{
	for( int i=0; i<kmaxmrts; i++ )
	{
		mMrt[i] = 0;
	}
}


///////////////////////////////////////////////////////////////////////////////

RtGroup::~RtGroup()
{
	for( int i=0; i<kmaxmrts; i++ )
	{
		if( mMrt[i] )
			delete mMrt[i];
	}
}

///////////////////////////////////////////////////////////////////////////////

void RtGroup::SetMrt( int idx, GfxBuffer *Buffer )
{
	OrkAssert( (idx>=0) && (idx<kmaxmrts) );						// ensure our mrt index is in range
	//OrkAssert( (ETGTTYPE_MRT0+idx) == Buffer->GetTargetType() );	// ensure our mrt type matches the index
	OrkAssert( idx==mNumMrts );										// ensure we add mrt's sequentially
	mMrt[mNumMrts] = Buffer;
	mNumMrts++;
	Buffer->SetParentMrt( this );
}

void RtGroup::Resize( int iw, int ih )
{
	if( (iw!=miW) || (ih!=miH) )
	{
		miW=iw;
		miH=ih;
		mbSizeDirty = true;
		for( int i=0; i<kmaxmrts; i++ )
		{
			if( mMrt[i] )
			{
				mMrt[i]->SetBufferWidth(miW);
				mMrt[i]->SetBufferHeight(miH);
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////

void GfxTarget::BindMaterial( GfxMaterial* pmtl )
{	if( nullptr == pmtl )	pmtl = GetCurMaterial();
	mpCurMaterial = pmtl;
	//OrkAssert( pMat );
}
void GfxTarget::PushMaterial( GfxMaterial* pmtl )
{
	mMaterialStack.push(mpCurMaterial);
	mpCurMaterial = pmtl;
}
void GfxTarget::PopMaterial()
{
	mpCurMaterial = mMaterialStack.top();
	mMaterialStack.pop();
}


void* GfxTarget::BeginLoad()
{
	return DoBeginLoad();
}
void GfxTarget::EndLoad(void*ploadtok)
{
	DoEndLoad(ploadtok);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} }
