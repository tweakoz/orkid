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

#if defined( ORK_CONFIG_DIRECT3D )
#include "dx.h"

#define D3D_DEBUG_INFO

void OrkCoInitialize();

namespace ork { namespace lev2 {

/////////////////////////////////////////////////////////////////////////

D3DPRESENT_PARAMETERS*	GfxTargetDX::s_pd3dpp=0;
D3DPRESENT_PARAMETERS	GfxTargetDX::s_d3dppInitial;
D3DEnumerator			GfxTargetDX::s_pD3D = 0;
D3DGfxDevice			GfxTargetDX::s_pd3dDevice = 0;
int						GfxTargetDX::s_iNumAdapters = 0;
D3DDEVTYPE				GfxTargetDX::s_devicetype=(D3DDEVTYPE)0;
D3DCAPS9				GfxTargetDX::s_caps;
DWORD					GfxTargetDX::s_windowstyle;
orkvector<SDXBufferFormat> GfxTargetDX::s_FrameBufferFormats;
orkvector<SDXBufferFormat> GfxTargetDX::s_PBufferFormats;
orkvector<GfxTargetDX*>GfxTargetDX::s_windows;
int GfxTargetDX::s_NumTexCoords = 0;
int GfxTargetDX::s_NumTexUnits = 0;
int GfxTargetDX::s_NumTexSamplers = 0;

/////////////////////////////////////////////////////////////////////////

static GfxTargetDX* MainTarget = 0;

GfxTargetDX::~GfxTargetDX()
{
}

///////////////////////////////////////////////////////////////////////////////

void GfxTargetDX::FxInit()
{
	static bool binit = true;

	if( true == binit )
	{
		binit = false;
		FxShader::RegisterLoaders( "shaders\\dx\\", "fx" );
	}
}

/////////////////////////////////////////////////////////////////////////

int GfxTargetDX::s_PixShaderSupport = 0;
bool GfxTargetDX::s_SoftwareVertexProcessing = false;
D3DPRESENT_PARAMETERS* GfxTargetDX::s_Presentations = 0;

DxMatrixStackInterface::DxMatrixStackInterface( GfxTargetDX& target )
	: MatrixStackInterface( target )
{
}

GfxTargetDX::GfxTargetDX()
	: GfxTarget()
	, mbResetDevice( false )
	, miLastTexSampState(0)
	, mbInitResize(true)
	, mFxI( *this )
	, mImI( *this )
	, mRsI( *this )
	, mGbI( *this )
	, mFbI( *this )
	, mTxI( *this )
	, mMtxI( *this )
{
	FxInit();

	for( int i=0; i<4; i++ )
		mLastTexH[i] = 0;

}

///////////////////////////////////////////////////////////////////////////////

void GfxTargetDX::InitializeDevice()
{
	////////////////////////////////////////////////////////////////////////
    // Create the Direct3D object

	if( 0 == s_pD3D )
	{
		OrkCoInitialize();

		if( NULL == ( s_pD3D = Direct3DCreate9(D3D_SDK_VERSION) ) )
		{
			OrkAssert(false);
		}

		////////////////////////////////////////////////////////////////////////
		// determine s_caps

		s_devicetype = D3DDEVTYPE_HAL; // D3DDEVTYPE_HAL D3DDEVTYPE_REF D3DDEVTYPE_SW

		HRESULT hr = s_pD3D->GetDeviceCaps( D3DADAPTER_DEFAULT, s_devicetype, &s_caps );

		U32 deviceflags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		deviceflags |= D3DCREATE_PUREDEVICE;
		deviceflags |= D3DCREATE_BUFFER_2_FRAMES;
		s_SoftwareVertexProcessing = false;

		////////////////////////////////////////////////////////////////////////
		// get formats

		D3DFORMAT adapterfmt = D3DFMT_LE_A8R8G8B8;
		D3DFORMAT rgbafmt = (D3DFORMAT) -1;
		D3DFORMAT zsfmt = (D3DFORMAT) -1;

		bool bFullScreen = true;

		//////////////////////////
		// Set up the presentation parameters for a double-buffered, 640x480,
		// 32-bit display using depth-stencil. Override these parameters in
		// your derived class as your app requires.

		s_iNumAdapters = 1;

		s_pd3dpp = new D3DPRESENT_PARAMETERS[ s_iNumAdapters ];
		s_Presentations = new D3DPRESENT_PARAMETERS[ s_iNumAdapters ];

		ZeroMemory( s_pd3dpp, sizeof(D3DPRESENT_PARAMETERS)*s_iNumAdapters );

		XVIDEO_MODE VideoMode;
		XGetVideoMode( &VideoMode );
		static bool g_bWidescreen = VideoMode.fIsWideScreen;
		
		///////////////////////////////////////////////////
		// XBox Gfx Ring Buffer Parameters
		///////////////////////////////////////////////////

		D3DRING_BUFFER_PARAMETERS rbparam;
		ZeroMemory( & rbparam, sizeof(D3DRING_BUFFER_PARAMETERS) );
		static const int kprisize = 1<<20;
		static const int ksecsize = 8<<20;
		rbparam.PrimarySize = kprisize;
		rbparam.SecondarySize = ksecsize;
		//rbparam.pPrimary = XPhysicalAlloc( kprisize, 

		///////////////////////////////////////////////////

		for( int ipp=0; ipp<s_iNumAdapters; ipp++ )
		{
			D3DPRESENT_PARAMETERS& rPP = s_pd3dpp[ipp];

			struct wtfmin
			{
				static int min( int a, int b ) { return (a<b) ? a : b; }
			};

			if (VideoMode.fIsHiDef || VideoMode.fIsWideScreen)
			{
				rPP.BackBufferWidth			= 1280;
				rPP.BackBufferHeight		= 720;
				mDisplayModes.push_back(new DisplayModeDX(1280, 720, 60, D3DFMT_A8R8G8B8, 0));
			}
			else
			{
				rPP.BackBufferWidth			= 640;
				rPP.BackBufferHeight		= 480;
				mDisplayModes.push_back(new DisplayModeDX(640, 480, 60, D3DFMT_A8R8G8B8, 0));
			}
			rPP.BackBufferFormat       = D3DFMT_A8R8G8B8;
			rPP.BackBufferCount        = 1;
			rPP.EnableAutoDepthStencil = TRUE;
			rPP.AutoDepthStencilFormat = D3DFMT_D24S8;
			rPP.SwapEffect             = D3DSWAPEFFECT_DISCARD;
			//rPP.PresentationInterval   = D3DRS_PRESENTIMMEDIATETHRESHOLD; //D3DPRESENT_INTERVAL_IMMEDIATE;
			rPP.PresentationInterval   = D3DPRESENT_INTERVAL_ONE; //D3DPRESENT_INTERVAL_ONE ;
			//rPP.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE; //D3DPRESENT_INTERVAL_ONE ;
			//rPP.PresentationInterval   = D3DPRESENT_INTERVAL_TWO; //D3DPRESENT_INTERVAL_ONE ;
			rPP.MultiSampleType		   = D3DMULTISAMPLE_NONE; //D3DMULTISAMPLE_4_SAMPLES; // D3DMULTISAMPLE_4_SAMPLES

			rPP.RingBufferParameters = rbparam;

			/*rPP.BackBufferFormat				= D3DFMT_A8R8G8B8;	// D3DFMT_A2R10G10B10 D3DFMT_A8R8G8B8 D3DFMT_A32B32G32R32F
			rPP.BackBufferCount					= 1;
			rPP.EnableAutoDepthStencil			= TRUE;
			rPP.AutoDepthStencilFormat			= D3DFMT_D24S8;
			rPP.SwapEffect						= D3DSWAPEFFECT_DISCARD;	// D3DSWAPEFFECT_COPY // D3DSWAPEFFECT_DISCARD
			rPP.FullScreen_RefreshRateInHz		= 0;
			rPP.PresentationInterval			= D3DPRESENT_INTERVAL_IMMEDIATE; //D3DPRESENT_INTERVAL_ONE_OR_IMMEDIATE
			//rPP.PresentationInterval			= D3DPRESENT_INTERVAL_ONE; //D3DPRESENT_INTERVAL_ONE_OR_IMMEDIATE
			rPP.Windowed						= TRUE;

			rPP.Flags							= D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;

			rPP.BackBufferWidth					= 0; //miW;
			rPP.BackBufferHeight				= 0; //miH;
			rPP.hDeviceWindow					= 0; //m_MainWindow;*/

		}

		memcpy( s_Presentations, s_pd3dpp, sizeof( D3DPRESENT_PARAMETERS )*s_iNumAdapters );


		////////////////////////////////////////////////////////////////////////
		// create device

		s_pd3dDevice = 0;

		hr = s_pD3D->CreateDevice(  D3DADAPTER_DEFAULT,
									s_devicetype,
									0,
									deviceflags,
									s_Presentations,
									&s_pd3dDevice );


		orkprintf( "d3ddevice<%p>\n", s_pd3dDevice );
		OrkAssertI( SUCCEEDED(hr), "ERROR: Could not create DirectX device. Is Maya open?");

		s_NumTexCoords = 2;
		s_NumTexUnits = 2;
		s_NumTexSamplers = 2;

		////////////////////////////////////////////////////////////////////////
		// Get Caps

		s_pd3dDevice->GetDeviceCaps(&s_caps);    // initialize GetD3DDevice() before using

		OrkAssert( s_caps.VertexShaderVersion >= D3DVS_VERSION(1,1) );

		if( s_caps.PixelShaderVersion >= D3DPS_VERSION(1,1) )
		{
			s_PixShaderSupport = 11;
		}

	}
}

///////////////////////////////////////////////////////////////////////////////

void GfxTargetDX::InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase )
{
	InitializeDevice();
	mCtxBase = pctxbase;
	m_MainWindow = mCtxBase->GetThisXID();
	///////////////////////////////////////
	s_windows.push_back( this );
	pWin->mpContext = this;
	//////////////////////////////////////////////////////////////////////////////

	mCtxBase = pWin->mpCTXBASE;
	m_MainWindow = mCtxBase->GetThisXID();
	mCtxBase->mpTarget = this;

	mPresentations = new D3DPRESENT_PARAMETERS[ s_iNumAdapters ];

	memcpy( mPresentations, s_pd3dpp, sizeof( D3DPRESENT_PARAMETERS )*s_iNumAdapters );

	for( int ipp=0; ipp<s_iNumAdapters; ipp++ )
	{
		D3DPRESENT_PARAMETERS& rPP = RefPresentationParams(ipp);
		rPP.hDeviceWindow		= m_MainWindow;
		rPP.BackBufferWidth		= miW;
		rPP.BackBufferHeight	= miH;
	}
	///////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	// Direct3D variables

	InitVB();	// Initialize Vertex Buffers

	MainTarget = this;

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	// Init FBI
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	mFbI.SetThisBuffer( pWin );
	mFbI.InitializeContext( pWin, pctxbase );
}

void DxFrameBufferInterface::InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase )
{
    GetD3DDevice()->GetRenderTarget( 0, &mpDefaultRenderTarget );

	GetD3DDevice()->GetDepthStencilSurface( &mpDepthBuffer );

}

void GfxTargetDX::InitializeContext( GfxBuffer* pBuf )
{
	InitializeDevice();

	///////////////////////////////////////////

	miW = pBuf->miWidth;
	miH = pBuf->miHeight;
	miX = 0;
	miY = 0;

	pBuf->mpContext = this;
	FBI()->SetOffscreenTarget( true );

	InitVB();

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	// Init FBI
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////

	mFbI.SetThisBuffer( pBuf );
	mFbI.InitializeContext( pBuf );
}

bool GfxTargetDX::SetDisplayMode(DisplayMode *mode)
{
	if(mode)
	{
		DisplayModeDX *modedx = static_cast<DisplayModeDX *>(mode);

		// TODO
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////

void DxFrameBufferInterface::InitializeContext( GfxBuffer* pBuf )
{
	HRESULT hr;

	///////////////////////////////////////////
	// create texture surface

	D3DFORMAT efmt = D3DFMT_A8R8G8B8;
	int ibytesperpix = 0;

	bool Zonly = false;

	switch( pBuf->meFormat )
	{
		case EBUFFMT_RGBA32:
			efmt = D3DFMT_A8R8G8B8;
			ibytesperpix = 4;
			break;
		case EBUFFMT_RGBA64:
			efmt = D3DFMT_A16B16G16R16F;
			ibytesperpix = 8;
			break;
		case EBUFFMT_RGBA128:
			efmt = D3DFMT_A32B32G32R32F;
			ibytesperpix = 16;
			break;
		case EBUFFMT_Z16:
			efmt = D3DFMT_R16F;
			ibytesperpix = 2;
			Zonly=true;
			break;
		case EBUFFMT_Z32:
			efmt = D3DFMT_R32F;
			ibytesperpix = 2;
			Zonly=true;
			break;
		default:
			OrkAssert(false);
			break;
	}

	mFORMAT = efmt;

	//CreateRenderTarget

	hr = GetD3DDevice()->CreateTexture( mTarget.GetW(),mTarget.GetH(),1,D3DUSAGE_RENDERTARGET,efmt,D3DPOOL_DEFAULT,&mpRenderTexture,NULL);
	OrkAssert( D3D_OK == hr );

	if( mpRenderTexture || Zonly )
	{
		D3DSURFACE_PARAMETERS sparams;
		memset(&sparams,0,sizeof(sparams));
		sparams.Base = 0;
		sparams.HierarchicalZBase = 0xFFFFFFFF;
		sparams.ColorExpBias = 0;
		sparams.HiZFunc = D3DHIZFUNC_DEFAULT;

		hr = GetD3DDevice()->CreateDepthStencilSurface( mTarget.GetW(), mTarget.GetH(),
														D3DFMT_D24S8,
														D3DMULTISAMPLE_NONE,
														0,
														TRUE,
														&mpDepthBuffer,
														&sparams );
		OrkAssert( D3D_OK == hr );

		sparams.Base = GPU_EDRAM_TILES/2;

		hr = GetD3DDevice()->CreateRenderTarget(	mTarget.GetW(), mTarget.GetH(),
													efmt, //( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
													D3DMULTISAMPLE_NONE, 0, 0,
													&m_pBackBuffer,
													&sparams );


		//hr = mpRenderTexture->GetSurfaceLevel( 0, & m_pBackBuffer );
		OrkAssert( D3D_OK == hr );

	}
	else
	{
		hr = GetD3DDevice()->CreateTexture( mTarget.GetW(),mTarget.GetH(),1,D3DUSAGE_RENDERTARGET,D3DFMT_X1R5G5B5,D3DPOOL_DEFAULT,&mpRenderTexture,NULL);
		OrkAssert( D3D_OK == hr );
		hr = GetD3DDevice()->CreateDepthStencilSurface( mTarget.GetW(), mTarget.GetH(), D3DFMT_D16, MULTSAMPTYPE, 0, TRUE, &mpDepthBuffer, 0 );
		OrkAssert( D3D_OK == hr );
	}


	///////////////////////////////////////////
	// create orknum texture and link it

	Texture* ptexture = new Texture();
	ptexture->SetWidth( mTarget.GetW() );
	ptexture->SetHeight( mTarget.GetH() );
	ptexture->SetBytesPerPixel( ibytesperpix );
	ptexture->SetTexClass( ork::lev2::Texture::ETEXCLASS_RENDERTARGET );

	D3DXTexturePackage* package = D3DXTexturePackage::CreateRenderTargetPackage( D3DXTexturePackage::ETYPE_2D, ptexture, mpRenderTexture );
	ptexture->SetTexIH( (void*) package );

	SetBufferTexture(ptexture);

	///////////////////////////////////////////
	// create material

	pBuf->mpTexture = ptexture;

	///////////////////////////////////////////
}

} }

#endif





