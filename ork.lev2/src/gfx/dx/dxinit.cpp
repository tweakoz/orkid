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
#include <ork/file/fileenv.h>

#if defined( ORK_CONFIG_DIRECT3D )
#include "dx.h"

#define D3D_DEBUG_INFO

void OrkCoInitialize();

namespace prodigy {
//std::string ConfigFileName();
};

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
WNDCLASSEX				GfxTargetDX::s_windowclass;
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
//	GBI()->DestroySharedVB();
}

///////////////////////////////////////////////////////////////////////////////

void GfxTargetDX::FxInit()
{
	static bool binit = true;

	if( true == binit )
	{
		binit = false;
		FxShader::RegisterLoaders( "shaders\\dx9\\fx\\", "fx" );
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
	const GfxTargetCreationParams& CreationParams = GfxEnv::GetRef().GetCreationParams();
	
	//m_MainWindow = CreationParams.mHWND;

	////////////////////////////////////////////////////////////////////////
    // Create the Direct3D object
	if(0 == s_pD3D)
	{
		OrkCoInitialize();

		if(NULL == (s_pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
		{
			OrkAssert(false);
		}

		////////////////////////////////////////////////////////////////////////
		// determine s_caps

		s_devicetype = D3DDEVTYPE_HAL; // D3DDEVTYPE_HAL D3DDEVTYPE_REF D3DDEVTYPE_SW

		HRESULT hr = s_pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, s_devicetype, &s_caps);

		U32 deviceflags = D3DCREATE_MULTITHREADED; //D3DCREATE_DISABLE_DRIVER_MANAGEMENT;
		if(s_caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
		{
			// TODO MAKE ME AUTOMATIC
			deviceflags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
			//deviceflags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
			s_SoftwareVertexProcessing = false;

			if(s_caps.VertexShaderVersion < D3DVS_VERSION(1, 1))
			{
				deviceflags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
				s_SoftwareVertexProcessing = true;
    		}

			deviceflags |= D3DCREATE_PUREDEVICE;
		}
	    else
		{
			deviceflags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
			s_SoftwareVertexProcessing = true;
		}

		////////////////////////////////////////////////////////////////////////
		// get formats

		D3DDISPLAYMODE CurDisplayMode;
		hr = s_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &CurDisplayMode);

		D3DFORMAT adapterfmt = CurDisplayMode.Format;
		D3DFORMAT rgbafmt = (D3DFORMAT)-1;
		D3DFORMAT zsfmt = (D3DFORMAT)-1;

		int iordinal = 0;

		unsigned int inumModes = s_pD3D->GetAdapterModeCount(iordinal, adapterfmt);
		for(unsigned int uiMode = 0; uiMode < inumModes; uiMode++)
		{
			D3DDISPLAYMODE DisplayMode;

			s_pD3D->EnumAdapterModes(iordinal, adapterfmt, uiMode, &DisplayMode);

			unsigned int uiW = DisplayMode.Width;
			unsigned int uiH = DisplayMode.Height;
			unsigned int uiRefresh = DisplayMode.RefreshRate;
			D3DFORMAT iFormat = DisplayMode.Format;

			const D3DFORMAT depthFormats[3] =
			{
				D3DFMT_D16,
				D3DFMT_D24X8,
				D3DFMT_D24S8,
			};

			std::string formatname;
			switch(iFormat)
			{
				case D3DFMT_A8R8G8B8:
					formatname = "argb32";
					break;
				case D3DFMT_X8R8G8B8:
					formatname = "xrgb32";
					break;
				case D3DFMT_R5G6B5:
					formatname = "rGb16";
					break;
			}

			// Find compatible depth/stencil formats
			for( int j=0; j<3; j++ )
			{
				if( SUCCEEDED( s_pD3D->CheckDeviceFormat( iordinal, D3DDEVTYPE_HAL, adapterfmt, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, depthFormats[j] ) ) )
				{

					if( (uiW>=640) && (uiH>=480) )
					{
						rgbafmt = iFormat;
						zsfmt = depthFormats[j];

						std::string depthfmtname;

						switch( zsfmt )
						{
							case D3DFMT_D16:
								depthfmtname = "D16";
								break;
							case D3DFMT_D24X8:
								depthfmtname = "D24X8";
								break;
							case D3DFMT_D24S8:
								depthfmtname = "D24S8";
								break;
						}

						bool rgba32tex = SUCCEEDED( s_pD3D->CheckDeviceFormat( iordinal, D3DDEVTYPE_HAL, adapterfmt, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8 ) );
						bool rgba16tex = SUCCEEDED( s_pD3D->CheckDeviceFormat( iordinal, D3DDEVTYPE_HAL, adapterfmt, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_A1R5G5B5 ) );
						bool rgba64tex = SUCCEEDED( s_pD3D->CheckDeviceFormat( iordinal, D3DDEVTYPE_HAL, adapterfmt, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) );
						bool rgba128tex = SUCCEEDED( s_pD3D->CheckDeviceFormat( iordinal, D3DDEVTYPE_HAL, adapterfmt, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F ) );
						bool f16tex = SUCCEEDED( s_pD3D->CheckDeviceFormat( iordinal, D3DDEVTYPE_HAL, adapterfmt, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_R16F ) );

						bool texZ16 = SUCCEEDED( s_pD3D->CheckDeviceFormat( iordinal, D3DDEVTYPE_HAL, iFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_D16 ) );
						bool texZ24X8 = SUCCEEDED( s_pD3D->CheckDeviceFormat( iordinal, D3DDEVTYPE_HAL, iFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_D24X8 ) );
						bool texZ24S8 = SUCCEEDED( s_pD3D->CheckDeviceFormat( iordinal, D3DDEVTYPE_HAL, iFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_D24S8 ) );

						//orkprintf( "DXMode [w %04d] [h %04d] [ref %03d] [rgbafmt %s] [zsfmt %s] [rtex rgba 16:%d 32:%d 64:%d 128:%d d16:%d] [texZ 16:%d 24:%d 32:%d]\n", iW, iH, iRefresh, formatname.c_str(), depthfmtname.c_str(), rgba16tex, rgba32tex, rgba64tex, rgba128tex, f16tex, texZ16, texZ24X8, texZ24S8 );
					}
				}
			}

			// only supporting 4x3, 16x9 and 60 Hz (for now)
			//if((uiW == (uiH * 4 / 3) || uiW == (uiH * 16 / 9)) && uiRefresh == 60)
			if( uiRefresh == 60 )
				mDisplayModes.push_back(new DisplayModeDX(uiW, uiH, uiRefresh, iFormat, uiMode));
		}

		////////////////////////////////////////////////////////////////////////
		// check for multihead

		s_iNumAdapters = s_caps.NumberOfAdaptersInGroup;
		int imassteradaptersord = s_caps.MasterAdapterOrdinal;
		int iadapterordingroup = s_caps.AdapterOrdinalInGroup;

		bool bMultiHead = false; //(miNumAdapters>1);

		if( bMultiHead )
		{
			deviceflags |= D3DCREATE_ADAPTERGROUP_DEVICE;
		}

		///////////////////////////////////////////////////////////
		// Register the window class

	    DWORD wcstyle = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;

		WNDCLASSEX wc = {	sizeof(WNDCLASSEX), wcstyle, 0, 0L, 0L,
							GetModuleHandle(NULL), NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, "OrkidD3D", NULL };

		memcpy( (void *) & s_windowclass, (void *) & wc, sizeof( WNDCLASSEX ) );

		DWORD lerr = GetLastError();

		ATOM _atom = RegisterClassEx( &s_windowclass );

		// Create the application's window
		s_windowstyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX ;

		//////////////////////////
		// Set up the presentation parameters for a double-buffered, 640x480,
		// 32-bit display using depth-stencil. Override these parameters in
		// your derived class as your app requires.

		s_pd3dpp = new D3DPRESENT_PARAMETERS[ s_iNumAdapters ];
		s_Presentations = new D3DPRESENT_PARAMETERS[ s_iNumAdapters ];

		ZeroMemory( s_pd3dpp, sizeof(D3DPRESENT_PARAMETERS)*s_iNumAdapters );

		bool bFULLSCREEN = CreationParams.mbFullScreen;

		for( int ipp=0; ipp<s_iNumAdapters; ipp++ )
		{
			D3DPRESENT_PARAMETERS& rPP = s_pd3dpp[ipp];

			rPP.BackBufferFormat				= D3DFMT_A8R8G8B8;	// D3DFMT_A2R10G10B10 D3DFMT_A8R8G8B8 D3DFMT_A32B32G32R32F
			rPP.BackBufferCount					= 1;
			rPP.EnableAutoDepthStencil			= TRUE;
			rPP.AutoDepthStencilFormat			= D3DFMT_D24S8;
			rPP.SwapEffect						= D3DSWAPEFFECT_DISCARD;	// D3DSWAPEFFECT_COPY // D3DSWAPEFFECT_DISCARD
			rPP.MultiSampleType					= D3DMULTISAMPLE_NONE; // D3DMULTISAMPLE_4_SAMPLES
			rPP.FullScreen_RefreshRateInHz		= 0;
			//rPP.PresentationInterval			= D3DPRESENT_INTERVAL_ONE; //D3DPRESENT_INTERVAL_ONE_OR_IMMEDIATE
			rPP.PresentationInterval			= D3DPRESENT_INTERVAL_IMMEDIATE; //D3DPRESENT_INTERVAL_ONE_OR_IMMEDIATE
			//rPP.PresentationInterval			= D3DPRESENT_INTERVAL_ONE; //D3DPRESENT_INTERVAL_ONE_OR_IMMEDIATE
			//rPP.PresentationInterval			= D3DPRESENT_INTERVAL_ONE; //D3DPRESENT_INTERVAL_ONE_OR_IMMEDIATE
			rPP.Windowed						= bFULLSCREEN ? FALSE : TRUE;

			rPP.Flags							= D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;

			rPP.BackBufferWidth					= bFULLSCREEN ? CreationParams.miDefaultWidth : 0; //miW;
			rPP.BackBufferHeight				= bFULLSCREEN ? CreationParams.miDefaultHeight : 0; //miH;
			rPP.hDeviceWindow					= CreationParams.mHWND;

		}

		memcpy( s_Presentations, s_pd3dpp, sizeof( D3DPRESENT_PARAMETERS )*s_iNumAdapters );


		////////////////////////////////////////////////////////////////////////
		// create device

		s_pd3dDevice = 0;

		hr = s_pD3D->CreateDevice(  D3DADAPTER_DEFAULT,
									s_devicetype,
									CreationParams.mHWND, //m_MainWindow,
									deviceflags,
									s_Presentations,
									&s_pd3dDevice );


		orkprintf( "d3ddevice<%p>\n", s_pd3dDevice );


#if ! defined( _XBOX )

		if( false == SUCCEEDED(hr) )
		{
			std::string cname = "";//prodigy::ConfigFileName();
			if( ork::CFileEnv::DoesFileExist( cname.c_str() ) )
			{
				Sleep(10);
				MessageBox(	0,
							"Cannot Create D3D Device, deleting config file. Press Ok and Try running again",
							"MiniOrk D3D Error",
							MB_OK|MB_SYSTEMMODAL );

				Sleep(10);
				DeleteFile(cname.c_str());
				ExitProcess(0);
			}	
		}

#endif
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
	pWin->SetContext(this);
	//////////////////////////////////////////////////////////////////////////////

	mCtxBase = pWin->mpCTXBASE;
	m_MainWindow = mCtxBase->GetThisXID();
	mCtxBase->SetTarget(this);

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

#if defined( _USE_WACOM )
	void WacomInit( HWND hwnd );
	WacomInit( m_MainWindow );
#endif

//	BOOL bv = SetWindowPos( m_MainWindow, HWND_TOP, 0, 0, 800, 600, SWP_SHOWWINDOW );

	//mImmIndexBuffer.Init( 1024 );
	//IdxBuf_Init( mImmIndexBuffer );
	//GfxEnv::GetRef().PopContext();

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

	miW = pBuf->GetBufferW();
	miH = pBuf->GetBufferH();
	miX = 0;
	miY = 0;

	pBuf->SetContext(this);
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

	switch( pBuf->GetBufferFormat() )
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
		hr = GetD3DDevice()->CreateDepthStencilSurface( mTarget.GetW(), mTarget.GetH(), D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &mpDepthBuffer, 0 );
		OrkAssert( D3D_OK == hr );
		hr = mpRenderTexture->GetSurfaceLevel( 0, & m_pBackBuffer );
		OrkAssert( D3D_OK == hr );

		mpDefaultRenderTarget = m_pBackBuffer;

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

	pBuf->SetTexture(ptexture);

	///////////////////////////////////////////
}

} }

#endif





