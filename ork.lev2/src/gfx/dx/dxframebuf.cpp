////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#if defined( ORK_CONFIG_DIRECT3D )
#include "dx.h"
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/dbgfontman.h>

#ifdef _XBOX
#include <Xgraphics.h>
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

DxFrameBufferInterface::DxFrameBufferInterface( ork::lev2::GfxTargetDX& target )
	: FrameBufferInterface(target)
	, mpDefaultRenderTarget(0)
	, mTargetDX(target)
	, mpDepthBuffer(0)
	, mpRenderTexture( 0 )
	, m_pBackBuffer(0)
#if ! defined(_XBOX)
	, mpSwapChain( 0 )
#endif
{
}

DxFrameBufferInterface::~DxFrameBufferInterface()
{
	HRESULT hr;
#if ! defined(_XBOX)
	if( mpSwapChain )
	{
		hr = mpSwapChain->Release();
		mpSwapChain = 0;
		OrkAssert( SUCCEEDED(hr) );
	}
#endif

	if( mpDepthBuffer )
	{	hr = mpDepthBuffer->Release();
		mpDepthBuffer = 0;
		OrkAssert( SUCCEEDED(hr) );
	}

	if( mpRenderTexture )
	{
		hr = mpRenderTexture->Release();
		mpRenderTexture = 0;
		OrkAssert( SUCCEEDED(hr) );
	}

	/*if( mpReadTexture )
	{
		hr = mpReadTexture->Release();
		mpReadTexture = 0;
		OrkAssert( SUCCEEDED(hr) );
	}*/

	if( m_pBackBuffer )
	{
		hr = m_pBackBuffer->Release();
		m_pBackBuffer = 0;
		OrkAssert( SUCCEEDED(hr) );
	}
}

D3DGfxDevice DxFrameBufferInterface::GetD3DDevice()
{
	return mTargetDX.GetD3DDevice();
}

void DxFrameBufferInterface::SetAsRenderTarget( void ) 
{
	HRESULT hr;
#if defined(_XBOX)
	hr = GetD3DDevice()->SetRenderTarget( 0, mpDefaultRenderTarget );
	OrkAssert(SUCCEEDED(hr));
	for( int it=1; it<4; it++ )
	{	hr = GetD3DDevice()->SetRenderTarget( it, 0 );
		OrkAssert(SUCCEEDED(hr));
	}
    hr = GetD3DDevice()->SetDepthStencilSurface( mpDepthBuffer );
	OrkAssert(SUCCEEDED(hr));
#else
	if( mpSwapChain )
	{
		IDirect3DSurface9 *pBackBuffer = 0;
		hr = mpSwapChain->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, & pBackBuffer );
		OrkAssert( SUCCEEDED( hr ) );
		hr = GetD3DDevice()->SetRenderTarget( 0, pBackBuffer );
		OrkAssert( SUCCEEDED( hr ) );
		hr = pBackBuffer->Release();
		OrkAssert( SUCCEEDED( hr ) );
		hr = GetD3DDevice()->SetDepthStencilSurface( mpDepthBuffer );
		OrkAssert( SUCCEEDED( hr ) );

		for( int it=1; it<4; it++ )
		{
			hr = GetD3DDevice()->SetRenderTarget( it, 0 );
		}

	}
	else // mpSwapChain == 0
	{
		OrkAssert( mpDefaultRenderTarget!= 0 );
		hr = GetD3DDevice()->SetRenderTarget( 0, mpDefaultRenderTarget );
		
		for( int it=1; it<4; it++ )
		{
			hr = GetD3DDevice()->SetRenderTarget( it, 0 );
			OrkAssert(SUCCEEDED(hr));
		}
		// might be coming in here from a SetRtGroup(0)
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::DoBeginFrame( void )
{
	MCheckPointContext( "GfxTargetDX::BeginFrame" );
	HRESULT hr;
	
	if( GetD3DDevice() )
	{
#if ! defined(_XBOX)
		hr = GetD3DDevice()->TestCooperativeLevel();

		switch( hr )
		{
			case D3DERR_DEVICELOST:
				
				mTargetDX.SetDeviceAvailable(false);
				break;
			case D3DERR_DRIVERINTERNALERROR:
				MessageBox( 0, "D3DERR_DRIVERINTERNALERROR", "D3DERR_DRIVERINTERNALERROR", MB_OK );
				exit(-1);
				break;
			case D3DERR_DEVICENOTRESET:
			case D3D_OK:
						
				if( false == mTargetDX.IsDeviceAvailable() )
				{
					if( IsOffscreenTarget() )
					{
						//InitializeContext( mpThisBuffer );
					}
					else
					{
						hr = GetD3DDevice()->Reset( & mTargetDX.GetPresentations()[0] );

						if( hr ==  D3D_OK )
						{
							FxInterface::Reset();
							mTargetDX.SetDeviceAvailable(true);
						}
						//InitializeContext( (GfxWindow*) mpThisBuffer, mCtxBase );
					}

				}

				break;
		}
#endif
	}

	if( false == mTargetDX.IsDeviceAvailable() ) return;

	hr = GetD3DDevice()->BeginScene();

	if( mTargetDX.FBI()->GetRtGroup() )
	{
		//CFontMan::Lock(&mTarget);
	}
	///////////////////////////////////////
	else if( IsOffscreenTarget() )
	///////////////////////////////////////
	{
		OrkAssert( SUCCEEDED(hr) );
		hr = GetD3DDevice()->SetRenderTarget( 0, m_pBackBuffer );
		OrkAssert( SUCCEEDED(hr) );
		hr = GetD3DDevice()->SetDepthStencilSurface( mpDepthBuffer );
		OrkAssert( SUCCEEDED(hr) );
	}
	/////////////////////////////////////////////////
	else // window (On Screen Target)
	/////////////////////////////////////////////////
	{
		//CFontMan::Lock(&mTarget);
		SetAsRenderTarget();
	}

	mTargetDX.DXGBI().BindVertexDeclaration(EVTXSTREAMFMT_V12C4T16);

	SRect extents( mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
	PushViewport(extents);
	PushScissor(extents);

	/////////////////////////////////////////////////

	if( GetAutoClear() )
	{
		if( mFORMAT == D3DFMT_R32F )
		{
			hr = GetD3DDevice()->Clear( 0, NULL, D3DCLEAR_ZBUFFER|D3DCLEAR_TARGET, 0xffffffff, 1.0f, 0L );
			OrkAssert( SUCCEEDED(hr) );
		}
		else
		{
			U32 ClearColorU = mTarget.CColor4ToU32(GetClearColor());
			hr = GetD3DDevice()->Clear( 0, NULL, D3DCLEAR_ZBUFFER|D3DCLEAR_TARGET, ClearColorU, 1.0f, 0L );
			OrkAssert( SUCCEEDED(hr) );
		}
	}

	/////////////////////////////////////////////////
	// Set Initial Rendering States

	static SRasterState defstate;
	mTarget.RSI()->BindRasterState( defstate, true );

#if defined(_XBOX)
	GetD3DDevice()->SetRenderState( D3DRS_PRESENTIMMEDIATETHRESHOLD, 50 );
#endif

	/////////////////////////////////////////////////
	//GfxTarget::BeginFrame();

}

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::DoEndFrame( void )
{
	if( false == mTarget.IsDeviceAvailable() ) return;

	MCheckPointContext( "GfxTargetDX::EndFrame" );
	HRESULT hr;

	///////////////////////////////////////////
	// release all resources for this frame
	///////////////////////////////////////////

	hr = GetD3DDevice()->SetIndices( 0 );
	OrkAssert( SUCCEEDED(hr) );
	hr = GetD3DDevice()->SetStreamSource( 0, 0, 0, 0 );
	OrkAssert( SUCCEEDED(hr) );
	hr = GetD3DDevice()->SetVertexDeclaration( 0 );
	OrkAssert( SUCCEEDED(hr) );

	for( int i=0; i<8; i++ )
	{
		hr = GetD3DDevice()->SetTexture( i, 0 );
		OrkAssert( SUCCEEDED(hr) );
	}

	///////////////////////////////////////////

	hr = GetD3DDevice()->EndScene();
	//OrkAssert( SUCCEEDED(hr) );

	if( mTargetDX.FBI()->GetRtGroup() )
	{
	}
	else if( IsOffscreenTarget() )
	{
#if defined(_XBOX)
		hr = GetD3DDevice()->Resolve(	
			D3DRESOLVE_RENDERTARGET0,	// Flags
			NULL,						// pSourceRect
			mpRenderTexture,			// pDestTexture
            NULL,						// pDestPoint
			0,							// DestLevel
			0,							// DestSliceOrFace
			NULL,						// pClearColor
			0,							// ClearZ
			0,							// ClearStencil
			NULL						// pParameters
			);
		OrkAssert( SUCCEEDED(hr) );
#endif
		GfxBuffer* pbuf = GetThisBuffer();
		pbuf->GetTexture()->SetDirty(false);
		pbuf->SetDirty(false);
	}
	else // window
	{
#if defined(_XBOX)
		//hr = GetD3DDevice()->SynchronizeToPresentationInterval();
		//OrkAssert( SUCCEEDED(hr) );
		hr = GetD3DDevice()->Present( NULL, NULL, NULL, NULL );
		OrkAssert( SUCCEEDED(hr) );
#else
		if( mpSwapChain )
		{
			hr = mpSwapChain->Present( NULL, NULL, mTargetDX.GetMainWindow(), NULL, 0 );
			//OrkAssert( SUCCEEDED(hr) );
			if(!SUCCEEDED(hr))
				orkprintf("SwapChain->Present FAILED!\n");
		}
		else
		{
			hr = GetD3DDevice()->Present( NULL, NULL, mTargetDX.GetMainWindow(), NULL );
			//OrkAssert( SUCCEEDED(hr) );
			if(!SUCCEEDED(hr))
				orkprintf("GetD3DDevice()->Present FAILED!\n");
		}
#endif
	}

	PopViewport();
	PopScissor();
}

///////////////////////////////////////////////////////////////////////////////

#if defined(_XBOX)
void DxFrameBufferInterface::BeginMrt( RtGroup* Base )
{
	//////////////////////////////////////////////////
	// lazy create mrt's
	//////////////////////////////////////////////////

	FBOObject *FboObj = (FBOObject *) Base->GetInternalHandle();

	int inumtargets = Base->GetNumTargets();

	D3DMULTISAMPLE_TYPE sampletype;
	switch (Base->GetSamples()) {
		case 2:
			sampletype = D3DMULTISAMPLE_2_SAMPLES;
			break;
		case 4:
			sampletype = D3DMULTISAMPLE_4_SAMPLES;
			break;
		default:
			sampletype = D3DMULTISAMPLE_NONE;
			break;
	}

	if( 0 == FboObj )
	{
		FboObj = new FBOObject;

		Base->SetInternalHandle( FboObj );

		int inumtiles = 0;
		int totalsize;
		int tileoffsets[FBOObject::kmaxtiles];
		
		const int kMrtWidth = Base->GetW();
		const int kMrtHeight = Base->GetH();

		/////////////////////////////////////////////////////////////////////////////

		bool bwillfit = false;

		const int ActiveMrtWidth = int(XGNextMultiple(kMrtWidth,160));
		int ActiveMrtHeight = int(XGNextMultiple(kMrtHeight,32));
		int iNumVerticalStripes = 1;
		while( false == bwillfit )
		{	
			/////////////////////////////////////////
			// start out with Z Buffer
			/////////////////////////////////////////

			int ioffset_edramblocks = XGSurfaceSize(ActiveMrtWidth, ActiveMrtHeight, D3DFMT_D24S8, sampletype);
			
			for( int it=0; it<inumtargets; it++ )
			{
				D3DFORMAT efmt = D3DFMT_A8R8G8B8;

				switch( Base->GetMrt(it)->GetBufferFormat() )
				{
					case EBUFFMT_RGBA32: efmt = D3DFMT_A8R8G8B8; break;
					case EBUFFMT_RGBA64: efmt = D3DFMT_A16B16G16R16F; break;
					case EBUFFMT_RGBA128: efmt = D3DFMT_A32B32G32R32F; break;
				}
				int isurfsize = XGSurfaceSize(ActiveMrtWidth, ActiveMrtHeight, efmt, sampletype);
				ioffset_edramblocks += isurfsize;
			}

			////////////////////////
			// will this setup fit?
			////////////////////////

			bwillfit = (ioffset_edramblocks<GPU_EDRAM_TILES);

			//////////////////////////////////////////////
			// if so, create setup
			//////////////////////////////////////////////
			if( bwillfit )
			{
				//////////////////////////////////////////////////////
				// ok we have a valid setup, make sure it fits in our data structure
				OrkAssert(inumtargets<FBOObject::kmaxtiles);
				//////////////////////////////////////////////////////

				ioffset_edramblocks = XGSurfaceSize(ActiveMrtWidth, ActiveMrtHeight, D3DFMT_D24S8, sampletype);
	
				inumtiles = 0;
				for( int itarg=0; itarg<inumtargets; itarg++ )
				{
					D3DFORMAT efmt = D3DFMT_A8R8G8B8;

					switch( Base->GetMrt(itarg)->GetBufferFormat() )
					{
						case EBUFFMT_RGBA32: efmt = D3DFMT_A8R8G8B8; break;
						case EBUFFMT_RGBA64: efmt = D3DFMT_A16B16G16R16F; break;
						case EBUFFMT_RGBA128: efmt = D3DFMT_A32B32G32R32F; break;
					}
					int isurfsize = XGSurfaceSize(ActiveMrtWidth, ActiveMrtHeight, efmt, sampletype);
					tileoffsets[inumtiles++] = ioffset_edramblocks;
					ioffset_edramblocks += isurfsize;
					totalsize = ioffset_edramblocks;
				}
				//////////////////////////////////////////
				// Generate Tiling Rects
				//////////////////////////////////////////
				FboObj->mNumStripes = iNumVerticalStripes;
				{
					int iyA = 0;
					for( int it=0; it<iNumVerticalStripes ; it++)
					{
						int iyB = iyA+ActiveMrtHeight;
						if( iyB > kMrtHeight ) iyB=kMrtHeight;
						FboObj->mStripeRects[it].x1 = 0;
						FboObj->mStripeRects[it].y1 = iyA;
						FboObj->mStripeRects[it].x2 = ActiveMrtWidth;
						FboObj->mStripeRects[it].y2 = iyB;
						iyA = iyB; 
					}
				}
				//////////////////////////////////////////
			}
			//////////////////////////////////////////////
			// if not, reduce the height and try again
			//////////////////////////////////////////////
			else
			{
				iNumVerticalStripes++;
				ActiveMrtHeight = int(XGNextMultiple(kMrtHeight/iNumVerticalStripes,32));
			}
			//////////////////////////////////////////////
		}

		///////////////////////////////////////////////////////////////
		// Setup Depth Target
		///////////////////////////////////////////////////////////////

		D3DSURFACE_PARAMETERS SurfaceParameters;
		ZeroMemory( &SurfaceParameters , sizeof( D3DSURFACE_PARAMETERS ) );

		IDirect3DSurface9* pDXDepthBuffer = 0;

		SurfaceParameters.Base = 0;
		SurfaceParameters.HierarchicalZBase = 0;
		HRESULT hr = GetD3DDevice()->CreateDepthStencilSurface(ActiveMrtWidth, ActiveMrtHeight, D3DFMT_D24S8, sampletype, 0, TRUE, & pDXDepthBuffer, &SurfaceParameters );

		FboObj->mDSBO = pDXDepthBuffer;

		///////////////////////////////////////////////////////////////
		// Setup Other Targets
		///////////////////////////////////////////////////////////////

		for( int it=0; it<inumtargets; it++ )
		{
			D3DFORMAT efmt = D3DFMT_A8R8G8B8;

			switch( Base->GetMrt(it)->GetBufferFormat() )
			{
				case EBUFFMT_RGBA32: efmt = D3DFMT_A8R8G8B8; break;
				case EBUFFMT_RGBA64: efmt = D3DFMT_A16B16G16R16F; break;
				case EBUFFMT_RGBA128: efmt = D3DFMT_A32B32G32R32F; break;
			}

			IDirect3DTexture9* pDXTexture = 0;
			IDirect3DSurface9* pDXColorBuffer = 0;

			hr = GetD3DDevice()->CreateTexture( Base->GetW(),Base->GetH(),1,D3DUSAGE_RENDERTARGET,efmt,D3DPOOL_DEFAULT,&pDXTexture,NULL);
			OrkAssert(SUCCEEDED(hr));

			SurfaceParameters.Base = tileoffsets[it];
			hr = GetD3DDevice()->CreateRenderTarget( ActiveMrtWidth, ActiveMrtHeight, efmt, sampletype, 0, TRUE, &pDXColorBuffer, &SurfaceParameters);

			OrkAssert(SUCCEEDED(hr));

			Base->GetMrt(it)->mpTexture = new Texture;
			Base->GetMrt(it)->mpTexture->SetWidth( Base->GetW() );
			Base->GetMrt(it)->mpTexture->SetHeight( Base->GetH() );
			//Base->GetMrt(it)->mpTexture->SetTexIH( (void*) pDXTexture );
			Base->GetMrt(it)->mpMaterial = new ork::lev2::GfxMaterialUITextured( & mTarget ); 
			Base->GetMrt(it)->mpMaterial->SetTexture( ETEXDEST_DIFFUSE, Base->GetMrt(it)->mpTexture );

			Base->GetMrt(it)->mpTexture->SetTexClass( ork::lev2::Texture::ETEXCLASS_RENDERTARGET );

			D3DXTexturePackage* package = D3DXTexturePackage::CreateRenderTargetPackage( D3DXTexturePackage::ETYPE_2D, Base->GetMrt(it)->mpTexture, pDXTexture );
			Base->GetMrt(it)->mpTexture->SetTexIH( (void*) package );

			FboObj->mTEX[it] = pDXTexture;
			FboObj->mFBO[it] = pDXColorBuffer;
		}
	}

	//////////////////////////////////////////////////
	// enable mrts
	//////////////////////////////////////////////////

	HRESULT hr = GetD3DDevice()->SetDepthStencilSurface( FboObj->mDSBO );

	for( int it=0; it<inumtargets; it++ )
	{
		hr = GetD3DDevice()->SetRenderTarget( it, FboObj->mFBO[it] );
		OrkAssert(SUCCEEDED(hr));
	}
	for( int it=inumtargets; it<4; it++ )
	{
		hr = GetD3DDevice()->SetRenderTarget( it, 0 );
		OrkAssert(SUCCEEDED(hr));
	}

	PushViewport( SRect(0,0, Base->GetW(), Base->GetH()) );
	PushScissor( SRect(0,0, Base->GetW(), Base->GetH()) );

	static SRasterState defstate;
	mTarget.RSI()->BindRasterState( defstate, true );

	// do our own clear for tile 0, BeginTiling only would handle MRT0
	GetD3DDevice()->BeginTiling(D3DTILING_SKIP_FIRST_TILE_CLEAR, 
		FboObj->mNumStripes, FboObj->mStripeRects, NULL, 1.0f, 0); // clear values aren't used.
		GetD3DDevice()->SetPredication(D3DPRED_TILE(0));
		GetD3DDevice()->ClearF(D3DCLEAR_ALLTARGETS|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, NULL, 0, 1.0f, 0);
		GetD3DDevice()->SetPredication(0);
		mCurrentRtGroup = Base;
}

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::EndMrt()
{
	FBOObject *FboObj = (FBOObject *) mCurrentRtGroup->GetInternalHandle();

	////////////////////////////////////////////////
	// copy mrt surface to read surface (if the read surface was created, by a pick operation)
	////////////////////////////////////////////////
	for( int i=0; i<FBOObject::kmaxrt; i++ )
	{
		IDirect3DSurface9* pmrtsurf = FboObj->mFBO[ i ];

		if ( 0 == pmrtsurf ) continue;

		DWORD flags = 0;

		flags = D3DRESOLVE_ALLFRAGMENTS | D3DRESOLVE_CLEARRENDERTARGET;
		
		switch(i)
		{
			case 0: flags |= D3DRESOLVE_RENDERTARGET0|D3DRESOLVE_CLEARDEPTHSTENCIL; break;
			case 1: flags |= D3DRESOLVE_RENDERTARGET1; break;
			case 2: flags |= D3DRESOLVE_RENDERTARGET2; break;
			case 3: flags |= D3DRESOLVE_RENDERTARGET3; break;
		}
		
		//////////////////////////////////////////////////////////////////
		// Resolve Stripes
		//////////////////////////////////////////////////////////////////

		for(int istripe=0 ; istripe < FboObj->mNumStripes ; istripe++)
		{
			D3DPOINT dst;
			dst.x = FboObj->mStripeRects[istripe].x1;
			dst.y = FboObj->mStripeRects[istripe].y1;
			
			GetD3DDevice()->SetPredication(D3DPRED_TILE(istripe));

			HRESULT hr = GetD3DDevice()->Resolve( 
				flags,
				& FboObj->mStripeRects[istripe],	// pSourceRect
				FboObj->mTEX[i],					// pDestTexture
				& dst,								// pDestPoint
				0,									// DestLevel
				0,									// DestSliceOrFace
				0,									// clear color
				1.0f,								// clear z
				0,									// clear stencil
				NULL
				);
			OrkAssert(SUCCEEDED(hr));
		}
	}
	GetD3DDevice()->SetPredication(0);
	GetD3DDevice()->EndTiling(0, NULL, NULL, NULL, 0, 0, NULL); // results were already copied out
}

void DxFrameBufferInterface::SetRtGroup( RtGroup* Base )
{
	if( 0 == Base )
	{
		//////////////////////////////////////////////////
		// Leaving Mrt Mode, disable mrts,
		//   copy to read surfaces if present
		//////////////////////////////////////////////////
		if( mCurrentRtGroup )
		{
			EndMrt(); // on xbox this will resolve to the offscreen buffers
		}
		////////////////////////////////////////////////
		// disable mrt
		//  pop viewport/scissor that was pushed by SetRtGroup( nonzero )
		// on xbox, happens after resolve
		////////////////////////////////////////////////
		PopViewport();
		PopScissor();
		SetAsRenderTarget();
		mCurrentRtGroup = 0;
		return;
	}

	BeginMrt(Base);
}

#else

void DxFrameBufferInterface::SetRtGroup( RtGroup* Base )
{
	HRESULT hr;

	//////////////////////////////////////////////////
	// Leaving Mrt Mode, disable mrts,
	//   copy to read surfaces if present
	//////////////////////////////////////////////////

	if( 0 == Base )
	{
		////////////////////////////////////////////////
		// disable mrt
		//  pop viewport/scissor that was pushed by SetRtGroup( nonzero )
		// on xbox, happens after resolve
		////////////////////////////////////////////////
		SetAsRenderTarget();
		PopViewport();
		PopScissor();

		if( mCurrentRtGroup )
		{
			DxFboObject *FboObj = (DxFboObject *) mCurrentRtGroup->GetInternalHandle();

			////////////////////////////////////////////////
			// copy mrt surface to read surface (if the read surface was created, by a pick operation)
			////////////////////////////////////////////////
			for( int i=0; i<DxFboObject::kmaxrt; i++ )
			{
				IDirect3DSurface9* preadsurf = FboObj->mFBOREAD[ i ];
				IDirect3DSurface9* pmrtsurf = FboObj->mFBO[ i ];
				if (!pmrtsurf) continue;

				if( preadsurf )
				{
					hr = GetD3DDevice()->GetRenderTargetData( pmrtsurf, preadsurf );
					OrkAssert(SUCCEEDED(hr));
				}

			}
		}
		mCurrentRtGroup = 0;
		return;
	}

	//////////////////////////////////////////////////
	// lazy create mrt's
	//////////////////////////////////////////////////

	DxFboObject *FboObj = (DxFboObject *) Base->GetInternalHandle();

	int inumtargets = Base->GetNumTargets();

	D3DMULTISAMPLE_TYPE sampletype;
	switch (Base->GetSamples()) {
		case 2:
			sampletype = D3DMULTISAMPLE_2_SAMPLES;
			break;
		case 4:
			sampletype = D3DMULTISAMPLE_4_SAMPLES;
			break;
		default:
			sampletype = D3DMULTISAMPLE_NONE;
			break;
	}

	if( 0 == FboObj )
	{
		FboObj = new DxFboObject;

		Base->SetInternalHandle( FboObj );

		for( int it=0; it<inumtargets; it++ )
		{
			GfxBuffer* pB = Base->GetMrt(it);
			pB->SetSizeDirty(true);
			Texture* ptex = new Texture;
			ptex->SetTexClass( ork::lev2::Texture::ETEXCLASS_RENDERTARGET );
			pB->SetTexture(ptex);
			ptex->SetWidth( Base->GetW() );
			ptex->SetHeight( Base->GetH() );
			//ptex->SetTexIH( (void*) pDXTexture );
			GfxMaterialUITextured* pmtl = new ork::lev2::GfxMaterialUITextured( & mTarget );
			pB->SetMaterial(pmtl);
			D3DXTexturePackage* ppkg = D3DXTexturePackage::CreateRenderTargetPackage( D3DXTexturePackage::ETYPE_2D, ptex, 0 );

			ptex->SetTexIH( (void*) ppkg );
		}
		Base->SetSizeDirty(true);
	}
	
	if( Base->IsSizeDirty() )
	{
		/////////////////////
		// create depth buffer
		/////////////////////
		if( FboObj->mDSBO )
		{	hr = FboObj->mDSBO->Release();
			OrkAssert( SUCCEEDED(hr) );
		}
		FboObj->mDSBO = 0;
		hr = GetD3DDevice()->CreateDepthStencilSurface( Base->GetW(), Base->GetH(), D3DFMT_D24S8, sampletype, 0, TRUE, & FboObj->mDSBO, 0 );
		/////////////////////
		for( int it=0; it<inumtargets; it++ )
		{
			GfxBuffer* pB = Base->GetMrt(it);
			Texture* ptex = pB->GetTexture();
			ptex->SetWidth( Base->GetW() );
			ptex->SetHeight( Base->GetH() );
			D3DXTexturePackage* package = (D3DXTexturePackage*) ptex->GetTexIH();
			///// trash old surface/texture /////
			if( FboObj->mTEX[it] )
			{
				hr = FboObj->mTEX[it]->Release();
				OrkAssert( SUCCEEDED(hr) );
			}
			if( FboObj->mFBO[it] )
			{
				hr = FboObj->mFBO[it]->Release();
				OrkAssert( SUCCEEDED(hr) );
			}
			///// get format /////
			D3DFORMAT efmt = D3DFMT_A8R8G8B8;
			switch( pB->GetBufferFormat() )
			{
				case EBUFFMT_RGBA32: efmt = D3DFMT_A8R8G8B8; break;
				case EBUFFMT_RGBA64: efmt = D3DFMT_A16B16G16R16F; break;
				case EBUFFMT_RGBA128: efmt = D3DFMT_A32B32G32R32F; break;
			}
			///// create new surface/texture /////
			IDirect3DTexture9* pDXTexture = 0;
			IDirect3DSurface9* pDXColorBuffer = 0;
			hr = GetD3DDevice()->CreateTexture( Base->GetW(),Base->GetH(),1,D3DUSAGE_RENDERTARGET,efmt,D3DPOOL_DEFAULT,&pDXTexture,NULL);
			OrkAssert(SUCCEEDED(hr));
			hr = pDXTexture->GetSurfaceLevel( 0, & pDXColorBuffer );
			OrkAssert(SUCCEEDED(hr));
			FboObj->mTEX[it] = pDXTexture;
			FboObj->mFBO[it] = pDXColorBuffer;
			package->mD3dTexureHandle = pDXTexture;
			///////////////////////////////////
			pB->SetSizeDirty(false);
		}

		Base->SetSizeDirty(false);
	}
	//////////////////////////////////////////////////
	// enable mrts
	//////////////////////////////////////////////////

	hr = GetD3DDevice()->SetDepthStencilSurface( FboObj->mDSBO );

	for( int it=0; it<inumtargets; it++ )
	{
		if (FboObj->mFBO[it] == FboObj->mDSBO)
			hr = GetD3DDevice()->SetRenderTarget( it, 0 ); // depth buffer copy
		else
			hr = GetD3DDevice()->SetRenderTarget( it, FboObj->mFBO[it] );
		OrkAssert(SUCCEEDED(hr));
	}
	for( int it=inumtargets; it<4; it++ )
	{
		hr = GetD3DDevice()->SetRenderTarget( it, 0 );
		OrkAssert(SUCCEEDED(hr));
	}

	PushViewport( SRect(0,0, Base->GetW(), Base->GetH()) );
	PushScissor( SRect(0,0, Base->GetW(), Base->GetH()) );

	static SRasterState defstate;
	mTarget.RSI()->BindRasterState( defstate, true );

	U32 ucolor = mcClearColor.GetRGBAU32();
	GetD3DDevice()->Clear( 0, NULL, D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL|D3DCLEAR_TARGET, 0, 1.0f, 0L );

	mCurrentRtGroup = Base;

}

#endif

///////////////////////////////////////////////////////////////////////////////

void GfxTargetDX::SetSize( int ix, int iy, int iw, int ih )
{
	miX=ix;
	miY=iy;
	miW=iw;
	miH=ih;
	mFbI.DeviceReset(ix,iy,iw,ih );
}

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::DeviceReset( int inx, int iny, int inw, int inh )
{	
	miOldW = mTarget.GetW();
	miOldH = mTarget.GetH();
	miNewW=inw;
	miNewH=inh;
	D3DPRESENT_PARAMETERS &rPP = mTargetDX.RefPresentationParams(0);
	rPP.BackBufferWidth	= inw;
	rPP.BackBufferHeight	= inh;

#if ! defined(_XBOX)
	HRESULT hr;

	//////////////////////////////////////////////

	if( mpSwapChain )
	{
		hr = mpSwapChain->Release();
		mpSwapChain = 0;
		OrkAssert( SUCCEEDED(hr) );
	}

	if( mpDepthBuffer )
	{	hr = mpDepthBuffer->Release();
		mpDepthBuffer = 0;
		OrkAssert( SUCCEEDED(hr) );
	}
	//////////////////////////////////////////////
	// coming back from a zero area reset

	if( (0 == mpSwapChain) && (inw>0) && false == mbEnableFullScreen )
	{
		hr = GetD3DDevice()->CreateDepthStencilSurface( mTarget.GetW(), mTarget.GetH(), D3DFMT_D24S8, MULTSAMPTYPE, 0, TRUE, &mpDepthBuffer, 0 );
		OrkAssert( SUCCEEDED(hr) );
		hr = GetD3DDevice()->CreateAdditionalSwapChain( &rPP, & mpSwapChain );
		OrkAssert( SUCCEEDED(hr) );
	}

	//////////////////////////////////////////////

	else if( 0==inw )
	{
		mpSwapChain = 0;
		mpDepthBuffer = 0;
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::Capture(const ork::file::Path &pth)
{
	const char* pname = pth.ToAbsolute().c_str();

	if( mpRenderTexture )
	{
		D3DXIMAGE_FILEFORMAT format = D3DXIFF_TGA;
		if( pth.GetExtension() == "tga" )
		{
			format = D3DXIFF_TGA;
		}
		else if( pth.GetExtension() == "dds" )
		{
			format = D3DXIFF_DDS;
		}

		HRESULT hr = D3DXSaveTextureToFile( pname, format, mpRenderTexture, 0 );
		//orkprintf( "GfxTargetDX::Capture<%s> succeeded<%d>\n", pname, int(SUCCEEDED(hr)) );
	}
	
}

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::Capture(CaptureBuffer& capbuf)
{
#if ! defined(_XBOX)

	if( mpRenderTexture )
	{
		IDirect3DSurface9* psurface = 0;

		HRESULT hr = mpRenderTexture->GetSurfaceLevel( 0, & psurface );
		OrkAssert( SUCCEEDED(hr) );

		D3DSURFACE_DESC RdDesc;
		hr = psurface->GetDesc( & RdDesc );

		capbuf.SetWidth(RdDesc.Width);
		capbuf.SetHeight(RdDesc.Height);

		D3DFORMAT tofmt = RdDesc.Format;
		switch( RdDesc.Format )
		{
			case D3DFMT_R32F:
				capbuf.SetFormat( EBUFFMT_F32 );
				break;
			case D3DFMT_A8R8G8B8:
				capbuf.SetFormat( EBUFFMT_RGBA32 );
				break;
			case D3DFMT_A16B16G16R16F:
				capbuf.SetFormat( EBUFFMT_RGBA128 );
				tofmt = D3DFMT_A32B32G32R32F;
				break;
			case D3DFMT_A32B32G32R32F:
				capbuf.SetFormat( EBUFFMT_RGBA128 );
				break;
			default:
				OrkAssert(false);
		}
		
		D3DLOCKED_RECT MyLockRect;

		IDirect3DSurface9 *preadsurf = 0;

		hr = GetD3DDevice()->CreateOffscreenPlainSurface( RdDesc.Width,
														RdDesc.Height,
														RdDesc.Format,
														D3DPOOL_SYSTEMMEM,
														& preadsurf,
														NULL );

		OrkAssert( SUCCEEDED(hr) );

		//FboObj->mFBOREAD[ MrtIndex ] = preadsurf;

		hr = GetD3DDevice()->GetRenderTargetData( psurface, preadsurf );
		OrkAssert( SUCCEEDED(hr) );

		hr = preadsurf->LockRect( & MyLockRect, 0, D3DLOCK_READONLY );
		OrkAssert( SUCCEEDED(hr) );
		{
			int inumpix = RdDesc.Width*RdDesc.Height;
			int idxbsize = MyLockRect.Pitch*RdDesc.Height;
			const int ibufsize = inumpix * 16;
			
			if( D3DFMT_A16B16G16R16F == RdDesc.Format )
			{
				char* pf32ary = (char*) malloc(ibufsize);
				D3DXFloat16To32Array( (float*) pf32ary, (const D3DXFLOAT16*) MyLockRect.pBits, inumpix*4 );
				capbuf.CopyData( (const void*) pf32ary, inumpix*capbuf.GetStride() );
				free(pf32ary);
			}
			else
			{
				capbuf.CopyData( (const void*) MyLockRect.pBits, inumpix*capbuf.GetStride() );
			}
		}
		hr = preadsurf->UnlockRect();
		OrkAssert( SUCCEEDED(hr) );

		preadsurf->Release();


	}
	else
	{
		capbuf.SetWidth( 0 );
		capbuf.SetHeight( 0 );
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::GetPixel( const CVector4 &rAt, GetPixelContext& ctx )
{
#if ! defined(_XBOX)
	HRESULT hr;

	CColor4 Color( 0.0f, 0.0f, 0.0f, 0.0f );

	int sx = int((rAt.GetX()) * CReal(mTarget.GetW()));
	int sy = int((rAt.GetY()) * CReal(mTarget.GetH()));

	bool bInBounds = ( (sx<mTarget.GetW()) && (sy<mTarget.GetH()) && (sx>0) && (sy>0) );
	if( IsOffscreenTarget() && bInBounds )
	{
		RECT rect = { sx-1, sy-1, sx+1, sy+1 };
		D3DLOCKED_RECT lrect;

		IDirect3DSurface9 *ReadSurfaces[ GetPixelContext::kmaxitems ] = { 0,0,0,0 };

		if( ctx.mRtGroup )
		{
			int MrtMask = ctx.miMrtMask;

			int MrtIndex = 0;

			while( MrtMask )
			{
				MrtMask &= ~ (1<<MrtIndex);

				OrkAssert( MrtIndex<ctx.mRtGroup->GetNumTargets() );

				DxFboObject *FboObj = (DxFboObject *) ctx.mRtGroup->GetInternalHandle();
				
				if( FboObj )
				{
					IDirect3DSurface9 *preadsurf = FboObj->mFBOREAD[ MrtIndex ];

					////////////////////////////////////////////////
					// create mrt read surface( currently here so we only create ones that are requested)
					////////////////////////////////////////////////
					{
						if( 0 == preadsurf )
						{
							IDirect3DSurface9* pmrtsurf = FboObj->mFBO[ MrtIndex ];

							D3DSURFACE_DESC RdDesc;
							hr = pmrtsurf->GetDesc( & RdDesc );

							hr = GetD3DDevice()->CreateOffscreenPlainSurface( RdDesc.Width,
																			RdDesc.Height,
																			RdDesc.Format,
																			D3DPOOL_SYSTEMMEM,
																			& preadsurf,
																			NULL );

							OrkAssert( SUCCEEDED(hr) );

							FboObj->mFBOREAD[ MrtIndex ] = preadsurf;

							/////////////////
							// 1st time copy
							//  subsequent copies will be done at the EndFrame() or some similar location
							/////////////////

							hr = GetD3DDevice()->GetRenderTargetData( pmrtsurf, preadsurf );
						}

					}
					ReadSurfaces[MrtIndex] = preadsurf;
				}
				MrtIndex++;
			}
		}

		for( int isurf=0; isurf<GetPixelContext::kmaxitems; isurf++ )
		{
			IDirect3DSurface9 *preadsurf = ReadSurfaces[isurf];

			const GetPixelContext::EPixelUsage eusage = ctx.mUsage[isurf];

			if( preadsurf )
			{
				hr = preadsurf->LockRect( & lrect, & rect, D3DLOCK_READONLY );
				D3DSURFACE_DESC Desc;
				hr = preadsurf->GetDesc( & Desc );
				D3DFORMAT readfmt = Desc.Format;
				int ipitch = lrect.Pitch;
				void *pData = lrect.pBits;
			
				switch( readfmt )
				{
					case D3DFMT_A16B16G16R16F:
					{
						static const int knumfloats = 9*4;
						float fpix[knumfloats];
						D3DXFloat16To32Array( fpix, (CONST D3DXFLOAT16 *) pData, knumfloats );
						static int kpixbase = 4*4;

						float fx = fpix[kpixbase+0];
						float fy = fpix[kpixbase+1];
						float fz = fpix[kpixbase+2];
						float fw = fpix[kpixbase+3];

						switch( eusage )
						{
							case GetPixelContext::EPU_FLOAT:
							{
								Color.SetX( fx );
								Color.SetY( fy );
								Color.SetZ( fz );
								Color.SetW( fw );
								break;
							}
							case GetPixelContext::EPU_PTR32:
							{
								struct fixfloat
								{
									static float doit( const float fin )
									{
										//float fv0 = fin*255.0f;
										//int iv0 = int(ceilf(fv0));
										return fin ; //float(iv0)/255.0f;
									}
								};
								Color.SetX( fixfloat::doit(fx) );
								Color.SetY( fixfloat::doit(fy) );
								Color.SetZ( fixfloat::doit(fz) );
								Color.SetW( fixfloat::doit(fw) );
								break;
							}
							default:
								OrkAssert(false);
								break;
						}
						break;
					}
					case D3DFMT_A32B32G32R32F:
					{
						Color = CColor4(0.0f,0.0f,0.0f,0.0f);
						break;
					}
					case D3DFMT_A8R8G8B8:
					{
						U32* pU32 = (U32*) pData;
						int idx = 1+(ipitch>>2); // 5th pixel of 9
						U32 uval = pU32[idx];
						U32 u_00_07 = (uval&0x000000ff);
						U32 u_08_15 = (uval&0x0000ff00)>>8;
						U32 u_16_23 = (uval&0x00ff0000)>>16;
						U32 u_24_31 = (uval&0xff000000)>>24;
						F32 f_00_07 = (F32) u_00_07 / 255.0f;
						F32 f_08_15 = (F32) u_08_15 / 255.0f;
						F32 f_16_23 = (F32) u_16_23 / 255.0f;
						F32 f_24_31 = (F32) u_24_31 / 255.0f;
						Color.SetX( f_16_23 );
						Color.SetY( f_08_15 );
						Color.SetZ( f_00_07 );
						Color.SetW( f_24_31 );
						orkprintf( "%08x\n", uval );
						break;
					}
					default:
						OrkAssert( false );
						break;
				}
				hr = preadsurf->UnlockRect();

				ctx.mPickColors[isurf] = Color;
			}
		}

	}
	else if( bInBounds )
	{
		ctx.mPickColors[0] = Color;
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////

DxFboObject::DxFboObject()
{
	for( int i=0; i<kmaxrt; i++ )
	{
		mFBO[i] = 0;
		mFBOREAD[i] = 0;
		mTEX[i] = 0;
	}
	mDSBO = 0;
}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////

#endif
