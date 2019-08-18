////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#import <ork/pch.h>
#if defined( ORK_CONFIG_OPENGL ) && defined( ORK_OSX )
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>
#import <OpenGL/OpenGL.h>
#import <dispatch/dispatch.h>
#import <ork/lev2/gfx/gfxenv.h>
#import "gl.h"
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#import <ork/lev2/qtui/qtui.h>
#import <ork/file/fileenv.h>
#import <ork/file/filedev.h>
#import <ork/kernel/opq.h>

#import <ork/kernel/objc.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxTargetGL, "GfxTargetGL")

extern "C"
{
	bool gbVSYNC = false;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

ork::MpMcBoundedQueue<void*> GfxTargetGL::mLoadTokens;

struct GlOsxPlatformObject
{
	static NSOpenGLContext* gShareMaster;

	NSOpenGLContext*	mNSOpenGLContext;
	NSView*				mOsxView;
	bool				mbInit;
	bool				mbNSOpenGlView;
	Opq 				mOpQ;
	void_lambda_t       mBindOp;
	GfxTargetGL*		mTarget;

	GlOsxPlatformObject()
		: mNSOpenGLContext(nil)
		, mOsxView(nil)
		, mbInit(true)
		, mbNSOpenGlView(false)
		, mOpQ(0,"GlOsxPlatformObject")
	{
		mBindOp = [=](){};
	}
};

/////////////////////////////////////////////////////////////////////////

struct GlOsxLoadContext
{
	NSOpenGLContext*	mGlContext;
	NSOpenGLContext*	mPushedContext;
};

/////////////////////////////////////////////////////////////////////////

GlOsxPlatformObject* gshareplato = nullptr;

CGLContextObj gOGLdefaultctx;
CGLPixelFormatObj gpixfmt = nullptr;
NSOpenGLContext* gNSGLdefaultctx = nil;
NSOpenGLContext* gNSGLfirstctx = nil;
NSOpenGLContext* gNSGLcurrentctx = nil;
NSOpenGLContext* GlOsxPlatformObject::gShareMaster = nil;
NSOpenGLPixelFormat* gNSPixelFormat = nil;
std::vector<NSOpenGLContext*> gWindowContexts;

void check_debug_log()
{
}

///////////////////////////////////////////////////////////////////////////////

void GfxTargetGL::GLinit()
{
	orkprintf( "INITOPENGL\n" );

	CGLPixelFormatAttribute attribs[] =
	{
		kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
		kCGLPFADepthSize, (CGLPixelFormatAttribute)24,
		kCGLPFANoRecovery,
		kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
 		kCGLPFAAlphaSize, (CGLPixelFormatAttribute)8,
 		kCGLPFAAccelerated,
		kCGLPFADoubleBuffer,
		kCGLPFASampleBuffers, (CGLPixelFormatAttribute)1,
		kCGLPFAMultisample,
		kCGLPFASamples, (CGLPixelFormatAttribute)16,

		//kCGLPFASamples, (CGLPixelFormatAttribute)4,
		//kCGLPFASamples, (CGLPixelFormatAttribute)1,
		(CGLPixelFormatAttribute) 0
	};

	size_t last_attribute = sizeof(attribs) / sizeof(CGLPixelFormatAttribute) - 1ul;

	GLint numPixelFormats;
	long value;


	CGLChoosePixelFormat (attribs, &gpixfmt, &numPixelFormats);

	printf( "numPixelFormats<%d>\n", int(numPixelFormats) );

	if( gpixfmt == nullptr ) {
		attribs[last_attribute - 2] = (CGLPixelFormatAttribute)NULL;
		CGLChoosePixelFormat (attribs, &gpixfmt, &numPixelFormats);
	}

	printf( "gpixfmt<%p>\n", (void*) gpixfmt );

	CGLError err = CGLCreateContext ( gpixfmt, NULL, & gOGLdefaultctx );

	OrkAssert( err==kCGLNoError );

	gNSGLdefaultctx = [[NSOpenGLContext alloc] initWithCGLContextObj:gOGLdefaultctx];

	OrkAssert( gNSGLdefaultctx!=nil );


	err = CGLSetCurrentContext( gOGLdefaultctx );
	OrkAssert( err==kCGLNoError );

	gNSPixelFormat = [[NSOpenGLPixelFormat alloc] initWithCGLPixelFormatObj:gpixfmt];

	GlOsxPlatformObject::gShareMaster = [[NSOpenGLContext alloc] initWithFormat:gNSPixelFormat shareContext:nil];
	printf( "gShareMaster<%p>\n", (void*) gNSGLdefaultctx );

	printf( "gNSPixelFormat<%p>\n", (void*) gNSPixelFormat );
}

//	NSOpenGLPixelFormat* nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithCGLPixelFormatObj:gpixfmt];
//	plato->mNSOpenGLContext = [[NSOpenGLContext alloc] initWithFormat:nsPixelFormat shareContext:GlOsxPlatformObject::gShareMaster];

///////////////////////////////////////////////////////////////////////////////

std::string GetGlErrorString( void );

void OpenGlGfxTargetInit()
{
	///////////////////////////////////////////////////////////
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	///////////////////////////////////////////////////////////
	GfxEnv::SetTargetClass(GfxTargetGL::GetClassStatic());
	//const ork::SFileDevContext& datactx = ork::CFileEnv::UrlBaseToContext( "data" );//, DataDirContext );

	//static dispatch_once_t ginit_once;
	//auto once_blk = ^ void (void)
	{
		GfxTargetGL::GLinit();
		auto target = new GfxTargetGL;
		auto poutbuf = new GfxBuffer(0,0,0,1280,720);
		GfxEnv::GetRef().SetLoaderTarget(target);
		target->InitializeContext(poutbuf);
	}
//	dispatch_once(&ginit_once, once_blk );
}

/////////////////////////////////////////////////////////////////////////

GfxTargetGL::GfxTargetGL()
	: GfxTarget()
	, mFxI( *this )
	, mImI( *this )
	, mRsI()
	, mGbI( *this )
	, mFbI( *this )
	, mTxI( *this )
	, mMtxI( *this )
	, mTargetDrawableSizeDirty(true)
{

	FxInit();

	NSOpenGLPixelFormat* nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithCGLPixelFormatObj:gpixfmt];
	for( int i=0; i<4; i++ )
	{
		GlOsxLoadContext* loadctx = new GlOsxLoadContext;

		NSOpenGLContext* gctx = [[NSOpenGLContext alloc] initWithFormat:gNSPixelFormat shareContext:GlOsxPlatformObject::gShareMaster];

		OrkAssert( gctx!=nil );

		loadctx->mGlContext = gctx;

		mLoadTokens.push( (void*) loadctx );
	}

}

/////////////////////////////////////////////////////////////////////////

GfxTargetGL::~GfxTargetGL()
{
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase  )
{
	///////////////////////
	GlOsxPlatformObject* plato = new GlOsxPlatformObject;
	mCtxBase = pctxbase;
	mPlatformHandle = (void*) plato;
	///////////////////////
	NSView* osxview = (NSView*) mCtxBase->GetThisXID();
	bool is_nsopenglview = [[osxview class]isSubclassOfClass:[NSOpenGLView class]];
	Class osxviewclass = object_getClass(osxview);
	Class osxviewparentclass = class_getSuperclass(osxviewclass);
	const char* osxviewClassName = object_getClassName(osxview);
	const char* osxviewParentClassName = class_getName(osxviewparentclass);
	orkprintf( "osxview<%p> class<%s> parclass<%s> is_nsopenglview<%d>\n", osxview, osxviewClassName, osxviewParentClassName, int(is_nsopenglview) );
	plato->mOsxView = osxview;
	plato->mTarget = this;

    [osxview  setWantsBestResolutionOpenGLSurface:NO];

	///////////////////////

	//////////////////////////////////////////////
	// create and init nsContext
	//////////////////////////////////////////////

	NSOpenGLPixelFormat* nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithCGLPixelFormatObj:gpixfmt];
	plato->mNSOpenGLContext = [[NSOpenGLContext alloc] initWithFormat:nsPixelFormat shareContext:GlOsxPlatformObject::gShareMaster];

	printf( "mNSOpenGLContext<%p>\n", (void*) plato->mNSOpenGLContext );

	if( is_nsopenglview ) // externally created NSOpenGlView
	{
		plato->mbNSOpenGlView = true;
		auto poglv = (NSOpenGLView*) osxview;
		[poglv setOpenGLContext:plato->mNSOpenGLContext];
	}

	if( gNSGLfirstctx==nil )
		gNSGLfirstctx = plato->mNSOpenGLContext;

	gWindowContexts.push_back( plato->mNSOpenGLContext );


	//////////////////////////////////////////////
	// set its view

	auto initvblk = [=]()
	{
		[plato->mNSOpenGLContext setView:osxview];
	};
	//plato->mOpQ.push(initvblk);
	initvblk();

	//printf( "osxgl:1\n");
	[plato->mNSOpenGLContext makeCurrentContext];
	//printf( "osxgl:2\n");

	//////////////////////////////////////////////

	auto cglctx = (CGLContextObj) [plato->mNSOpenGLContext CGLContextObj];
	GLint p0 = 0;
	GLint p1 = 8;
	CGLSetParameter(cglctx,kCGLCPSwapInterval, &p0);
	CGLSetParameter(cglctx,kCGLCPMPSwapsInFlight,&p1);

	//////////////////////////////////////////////
	// turn on vsync



	//printf( "osxgl:3\n");

	GLint vsyncparams[] =
	{
		gbVSYNC ? 1 : 0,
	};

	[plato->mNSOpenGLContext setValues:&vsyncparams[0] forParameter:NSOpenGLCPSwapInterval];
	//printf( "osxgl:4\n");

	mFbI.SetThisBuffer( pWin );

	//////////////////////////////////////////////

	plato->mBindOp = [=]()
	{
		if( plato->mbNSOpenGlView)
		{
			//printf( "MCC PATH A\n" );
			[plato->mNSOpenGLContext makeCurrentContext];
			return;
		}

		//printf( "MCC PATH B\n" );
		if(plato->mbInit)
		{
			auto tgt = 	plato->mTarget;

			if( tgt->mCtxBase )
			{
				//printf( "MCC PATH C\n" );
				NSView* osxview = (NSView*) tgt->mCtxBase->GetThisXID();
				GfxWindow *pWin = (GfxWindow *) tgt->mFbI.GetThisBuffer();
				if( osxview != plato->mOsxView )
				{
					//printf( "MCC PATH D\n" );
					OrkAssert(false);
					tgt->InitializeContext( pWin,mCtxBase );
				}
			}

			if( plato->mOsxView )
			{
				//printf( "MCC PATH E\n" );
				[plato->mNSOpenGLContext setView:plato->mOsxView];
			}
			plato->mbInit=false;
		}
		//printf( "MCC PATH F\n" );
		[plato->mNSOpenGLContext makeCurrentContext];
		if( plato->mTarget->mTargetDrawableSizeDirty )
		{
			//printf( "MCC PATH G\n" );
			[plato->mNSOpenGLContext update];
			plato->mTarget->mTargetDrawableSizeDirty = false;
		}
	};

	//////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::InitializeContext( GfxBuffer *pBuf )
{
	///////////////////////

	miW = pBuf->GetBufferW();
	miH = pBuf->GetBufferH();
	miX = 0;
	miY = 0;

	mCtxBase = 0;

	GlOsxPlatformObject* plato = new GlOsxPlatformObject;
	plato->mTarget = this;
	mPlatformHandle = (void*) plato;
	mFbI.SetThisBuffer( pBuf );

	plato->mNSOpenGLContext = GlOsxPlatformObject::gShareMaster;
	plato->mOsxView = nullptr;
	plato->mbNSOpenGlView = nullptr;
	plato->mbInit = false;

	FBI()->SetOffscreenTarget( true );

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

//	[plato->mNSOpenGLContext makeCurrentContext];

	//////////////////////////////////////////////

	//mFbI.InitializeContext( pBuf );
	pBuf->SetContext(this);

	//////////////////////////////////////////////

	plato->mBindOp = [=]()
	{
		[plato->mNSOpenGLContext makeCurrentContext];
		//[plato->mNSOpenGLContext update];
	};

	//////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::MakeCurrentContext( void )
{
	GlOsxPlatformObject* plato = (GlOsxPlatformObject*) mPlatformHandle;

	if( plato )
	{
		plato->mOpQ.Process();

		plato->mBindOp();

		gNSGLcurrentctx = plato->mNSOpenGLContext;
	}
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::SwapGLContext( CTXBASE *pCTFL )
{
	GlOsxPlatformObject* plato = (GlOsxPlatformObject*) mPlatformHandle;
	if( plato )
	{
		if( plato->mbNSOpenGlView)
		{
			return;
		}
		//printf( "SWAP<%p>\n", this );
		[plato->mNSOpenGLContext flushBuffer];
	}
	//Objc::Class("NSOpenGLContext").InvokeClassMethodV("clearCurrentContext" );
}

void* GfxTargetGL::DoBeginLoad()
{
	void* pvoiddat = nullptr;

	while(false==mLoadTokens.try_pop(pvoiddat))
	{
		usleep(1<<10);
	}

	GlOsxLoadContext* loadctx = (GlOsxLoadContext*) pvoiddat;


	loadctx->mPushedContext = [NSOpenGLContext currentContext];
	[loadctx->mGlContext makeCurrentContext];

	printf( "BEGINLOAD loadctx<%p> glx<%p>\n", loadctx,loadctx->mGlContext);

	//OrkAssert(bOK);

	return pvoiddat;
}

void GfxTargetGL::DoEndLoad(void*ploadtok)
{
	GlOsxLoadContext* loadctx = (GlOsxLoadContext*) ploadtok;
	printf( "ENDLOAD loadctx<%p> glx<%p>\n", loadctx,loadctx->mGlContext);
	mLoadTokens.push(ploadtok);
}

}}
///////////////////////////////////////////////////////////////////////////////

#endif // #if defined( ORK_CONFIG_OPENGL ) && defined(IX)
