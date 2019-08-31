////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

///////////////////////////////////////////////////////////////////////////////
#if defined( _WIN32 )
///////////////////////////////////////////////////////////////////////////////
#include <GLEW/glew.c>
#include <GLEW/wglew.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/ui/ui.h>
//
#include <ork/lev2/qtui/qtui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/dbgfontman.h>
///////////////////////////////////////////////////////////////////////////////
#if defined(_USE_CGFX)
#pragma comment( lib, "cg.lib" )
#pragma comment( lib, "cggl.lib" )
#endif
//#pragma comment(lib, "glew32.lib")  
#pragma comment(lib,"opengl32.lib")
///////////////////////////////////////////////////////////////////////////////
extern bool sbExit;

extern int giCurMouseX;
extern int giCurMouseY;

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxTargetGL, "GfxTargetGL")
///////////////////////////////////////////////////////////////////////////////

//		HGLRC				mhGLRC;		// Handle to a GL context.
//		HWND				mhHWND;		// Handle to a Windpw
//		WNDCLASSEX			m_MainWindowClass;

//		std::stack< HDC >		mDCStack;
//		std::stack< HGLRC >		mGLRCStack;

struct GlWinPlatformObject
{
	bool				mbInit;
	HWND				mhHWND;		// Handle to a Windpw
	HDC					mhDC;		// Handle to a device context.
	HGLRC				mhGLRC;

	static GlWinPlatformObject* gShareMaster;

	GlWinPlatformObject()
		: mbInit(true)
		, mhHWND(nullptr)
		, mhDC(nullptr)
		, mhGLRC(nullptr)
	{
	}
};

GlWinPlatformObject* GlWinPlatformObject::gShareMaster = nullptr;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {
std::string GetGlErrorString( int iGLERR )
{
	std::string RVAL = (std::string) "GL_UNKNOWN_ERROR";

	switch( iGLERR )
	{
		case GL_NO_ERROR:
			RVAL =  "GL_NO_ERROR";
			break;
		case GL_INVALID_ENUM:
			RVAL =  (std::string) "GL_INVALID_ENUM";
			break;
		case GL_INVALID_VALUE:
			RVAL =  (std::string) "GL_INVALID_VALUE";
			break;
		case GL_INVALID_OPERATION:
			RVAL =  (std::string) "GL_INVALID_OPERATION";
			break;
		case GL_OUT_OF_MEMORY:
			RVAL =  (std::string) "GL_OUT_OF_MEMORY";
			break;
		default:
			break;
	}
	return RVAL;
}

int GetGlError( void )
{
	int err = glGetError();

	std::string errstr = GetGlErrorString( err );

	if( err != GL_NO_ERROR )
		orkprintf( "GLERROR [%s]\n", errstr.c_str() );

	return err;
}

///////////////////////////////////////////////////////////////////////////////

void OpenGlGfxTargetInit()
{
	GfxEnv::SetTargetClass(GfxTargetGL::GetClassStatic());
}

///////////////////////////////////////////////////////////////////////////////

std::string GetGlErrorString( void );

/////////////////////////////////////////////////////////////////////////

GfxTargetGL::GfxTargetGL()
	: GfxTarget()
	, mFxI( *this )
	, mImI( *this )
	, mGbI( *this )
	, mFbI( *this )
	, mMtxI( *this )
{
	FxInit();
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::SwapGLContext( CTXBASE *pCTFL )
{
	GlWinPlatformObject* plato = (GlWinPlatformObject*) mPlatformHandle;

	wglSwapLayerBuffers( plato->mhDC, WGL_SWAP_MAIN_PLANE );
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::MakeCurrentContext( void )
{
	GlWinPlatformObject* plato = (GlWinPlatformObject*) mPlatformHandle;

	if( wglMakeContextCurrentARB( plato->mhDC, plato->mhDC, plato->mhGLRC ) == false )
	{
		orkprintf( "CRenderTarget::MakeCurrent() failed.\n" );
		OrkAssert( false );
	}
}

/////////////////////////////////////////////////////////////////////////

LRESULT WINAPI OGLMsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

/////////////////////////////////////////////////////////////////////////

static GfxTargetGL *gpmaincontext = 0;

void GfxTargetGL::InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase )
{
	CTQT *pqtwin = (CTQT*) pWin->mpCTXBASE;
	GlWinPlatformObject* plato = new GlWinPlatformObject;
	mCtxBase = pctxbase;
	mPlatformHandle = (void*) plato;
	mFbI.SetThisBuffer( pWin );
	plato->mhHWND =  HWND(pqtwin->winId());
	pWin->SetContext(this);
	pctxbase->SetTarget(this);
	///////////////////////////////////////
	//extern u32 W32KeyMap_Current[8]; // 256 bits
	//for( int idx=0; idx<8; idx++ )
	//{
	//	W32KeyMap_Current[idx] = 0;
	//}
	///////////////////////////////////////////////////////
	// Get GL Context
	HDC RenderingDC = GetDC( plato->mhHWND );
	// - Ensure that we're running on a non-palettized display
	if ( ( GetDeviceCaps( RenderingDC, RASTERCAPS ) & RC_PALETTE ) != RC_PALETTE )
	{
		// - Find an appropriate pixel format
		int dWGLPixelFormatIndex;

		PIXELFORMATDESCRIPTOR StandardWGLPixelFormat;

		StandardWGLPixelFormat.nVersion = 1;
		StandardWGLPixelFormat.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_GENERIC_ACCELERATED;
		StandardWGLPixelFormat.iPixelType = PFD_TYPE_RGBA;

		StandardWGLPixelFormat.cColorBits = 32;
		StandardWGLPixelFormat.cDepthBits = 24;
		StandardWGLPixelFormat.cStencilBits = 8;

		dWGLPixelFormatIndex = ChoosePixelFormat( RenderingDC, &StandardWGLPixelFormat );
		SetPixelFormat( RenderingDC, dWGLPixelFormatIndex, &StandardWGLPixelFormat );
	}

	///////////////////////////////////////////////////////////
	
	static bool gbinitglew = true;

	if( gbinitglew )
	{
		HGLRC tempContext=wglCreateContext(RenderingDC);
		wglMakeCurrent(RenderingDC,tempContext);
		GLenum giOK = glewInit();
		OrkAssert( giOK == GLEW_OK );
		wglDeleteContext(tempContext);		
		gbinitglew = false;	
	}

	//int attribs[] = { };
	int attribs[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
		WGL_CONTEXT_MINOR_VERSION_ARB, 1,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		0
	};
	HGLRC OpenGLContext = wglCreateContextAttribsARB( RenderingDC, 0, attribs );
	//			     const int *attribList)	HGLRC OpenGLContext = wglCreateContext( RenderingDC );

	OrkAssert( OpenGLContext );

	static HGLRC FIRST_GLRC = OpenGLContext;

	if( FIRST_GLRC != OpenGLContext )
	{
		bool bshare = wglShareLists( FIRST_GLRC, OpenGLContext );

		if( false == bshare )
		{
			orkprintf( "pbuffer: wglShareLists() failed\n" );
			exit( -1 );
		}
	}

	wglMakeCurrent( RenderingDC, OpenGLContext );
	plato->mhDC = wglGetCurrentDC();
	plato->mhGLRC = wglGetCurrentContext();

	///////////////////////////////////////////////////////
	// Post Init

	static bool b1timeonly = true;

	if( b1timeonly )
	{
		//InitGLExt();
		b1timeonly = false;
		gpmaincontext = this;
	}

	if( 0 == GlWinPlatformObject::gShareMaster )
	{
		GlWinPlatformObject::gShareMaster = plato;
	}

	GL_ERRORCHECK();
	///////////////////////////////////////////////////////

}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::InitializeContext( GfxBuffer *pBuf )
{
	GlWinPlatformObject* shmaster = GlWinPlatformObject::gShareMaster;

	GlWinPlatformObject* plato = new GlWinPlatformObject;
	mPlatformHandle = (void*) plato;
	mFbI.SetThisBuffer( pBuf );

	plato->mhDC = shmaster->mhDC;
	plato->mhGLRC = shmaster->mhGLRC;
	plato->mhHWND = shmaster->mhHWND;
	//GfxEnv::GetRef().PushContext( this );

	FBI()->SetOffscreenTarget( true );

	GfxTargetGL* pTARG = this;

//	HDC hWindowDC = pTARG->GetHDC();
	//HGLRC hWindowGLRC = pTARG->GetHGLRC();

	//////////////////////////////////////////
	// Bind Texture

	Texture* pTexture = new Texture();
	pTexture->SetWidth( miW );
	pTexture->SetHeight( miH );
	pTexture->SetBytesPerPixel( 4 );
	pTexture->SetTexClass( ork::lev2::Texture::ETEXCLASS_RENDERTARGET );

	FBI()->SetBufferTexture( pTexture );

	///////////////////////////////////////////
	// create material

	GfxMaterialUITextured* pmtl = new GfxMaterialUITextured(this);
	pBuf->SetMaterial(pmtl);
	pmtl->SetTexture( ETEXDEST_DIFFUSE, pTexture );
	pBuf->SetTexture(pTexture);

	////////////////////////////////////////////////////////////////////////////

//	InitTargFBO();

	////////////////////////////////////////////////////////////////////////////

	pBuf->SetContext(this);

}

///////////////////////////////////////////////////////////////////////////////


bool GfxTargetGL::SetDisplayMode(DisplayMode *mode)
{
	return false;
}

///////////////////////////////////////////////////////////////////////////////

GfxTargetGL::~GfxTargetGL()
{
//	CFontMan::FlushFonts();
}

///////////////////////////////////////////////////////////////////////////////

} }

#endif // _WIN32
