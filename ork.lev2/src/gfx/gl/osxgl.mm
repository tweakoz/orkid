////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#import <ork/pch.h>
#if defined( ORK_OSX )
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

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ContextGL, "ContextGL")

extern "C"
{
	bool gbVSYNC = false;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

bool _macosUseHIDPI = false;

ork::MpMcBoundedQueue<void*> ContextGL::_loadTokens;

struct GlOsxPlatformObject
{
	static NSOpenGLContext* gShareMaster;

	NSOpenGLContext*	mNSOpenGLContext;
	NSView*				mOsxView;
	bool				mbInit;
	bool				mbNSOpenGlView;
	opq::OperationsQueue		mOpQ;
	void_lambda_t       mBindOp;
	ContextGL*		mTarget;

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

void ContextGL::GLinit()
{
	//orkprintf( "INITOPENGL\n" );

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

	//printf( "numPixelFormats<%d>\n", int(numPixelFormats) );

	if( gpixfmt == nullptr ) {
		attribs[last_attribute - 2] = (CGLPixelFormatAttribute)NULL;
		CGLChoosePixelFormat (attribs, &gpixfmt, &numPixelFormats);
	}

	//printf( "gpixfmt<%p>\n", (void*) gpixfmt );

	CGLError err = CGLCreateContext ( gpixfmt, NULL, & gOGLdefaultctx );

	OrkAssert( err==kCGLNoError );

	gNSGLdefaultctx = [[NSOpenGLContext alloc] initWithCGLContextObj:gOGLdefaultctx];

	OrkAssert( gNSGLdefaultctx!=nil );


	err = CGLSetCurrentContext( gOGLdefaultctx );
	OrkAssert( err==kCGLNoError );

	gNSPixelFormat = [[NSOpenGLPixelFormat alloc] initWithCGLPixelFormatObj:gpixfmt];

	GlOsxPlatformObject::gShareMaster = [[NSOpenGLContext alloc] initWithFormat:gNSPixelFormat shareContext:nil];
	//printf( "gShareMaster<%p>\n", (void*) gNSGLdefaultctx );
	//printf( "gNSPixelFormat<%p>\n", (void*) gNSPixelFormat );
}

//	NSOpenGLPixelFormat* nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithCGLPixelFormatObj:gpixfmt];
//	plato->mNSOpenGLContext = [[NSOpenGLContext alloc] initWithFormat:nsPixelFormat shareContext:GlOsxPlatformObject::gShareMaster];

///////////////////////////////////////////////////////////////////////////////

std::string GetGlErrorString( void );

void OpenGlContextInit() {
	///////////////////////////////////////////////////////////
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	///////////////////////////////////////////////////////////

	auto clazz = dynamic_cast<object::ObjectClass*>(ContextGL::GetClassStatic());
	GfxEnv::setContextClass(clazz);
  ContextGL::GLinit();
  auto target = new ContextGL;
  target->initializeLoaderContext();
  GfxEnv::GetRef().SetLoaderTarget(target);
}

/////////////////////////////////////////////////////////////////////////

ContextGL::ContextGL()
	: Context()
	, mFxI( *this )
	, mImI( *this )
	, mRsI( *this )
	, mGbI( *this )
	, mFbI( *this )
	, mTxI( *this )
	, mMtxI( *this )
	, mDWI(*this)
	, mTargetDrawableSizeDirty(true)
{

	FxInit();

	NSOpenGLPixelFormat* nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithCGLPixelFormatObj:gpixfmt];
	for( int i=0; i<4; i++ ){
		GlOsxLoadContext* loadctx = new GlOsxLoadContext;
		NSOpenGLContext* gctx = [[NSOpenGLContext alloc] initWithFormat:gNSPixelFormat shareContext:GlOsxPlatformObject::gShareMaster];
		OrkAssert( gctx!=nil );
		loadctx->mGlContext = gctx;
		_loadTokens.push( (void*) loadctx );
	}

}

/////////////////////////////////////////////////////////////////////////

ContextGL::~ContextGL(){}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeWindowContext( Window *pWin, CTXBASE* pctxbase  ) {
  meTargetType = ETGTTYPE_WINDOW;
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
	//orkprintf( "osxview<%p> class<%s> parclass<%s> is_nsopenglview<%d>\n", osxview, osxviewClassName, osxviewParentClassName, int(is_nsopenglview) );
	plato->mOsxView = osxview;
	plato->mTarget = this;

  [osxview  setWantsBestResolutionOpenGLSurface:_macosUseHIDPI?YES:NO];

	///////////////////////

	//////////////////////////////////////////////
	// create and init nsContext
	//////////////////////////////////////////////

	NSOpenGLPixelFormat* nsPixelFormat = [[NSOpenGLPixelFormat alloc] initWithCGLPixelFormatObj:gpixfmt];
	plato->mNSOpenGLContext = [[NSOpenGLContext alloc] initWithFormat:nsPixelFormat shareContext:GlOsxPlatformObject::gShareMaster];

	//printf( "mNSOpenGLContext<%p>\n", (void*) plato->mNSOpenGLContext );

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
				Window *pWin = (Window *) tgt->mFbI.GetThisBuffer();
				if( osxview != plato->mOsxView )
				{
					//printf( "MCC PATH D\n" );
					OrkAssert(false);
					tgt->initializeWindowContext( pWin,mCtxBase );
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

void ContextGL::initializeOffscreenContext( OffscreenBuffer *pBuf )
{
  meTargetType = ETGTTYPE_OFFSCREEN;
	///////////////////////

	miW = pBuf->GetBufferW();
	miH = pBuf->GetBufferH();

	mCtxBase = 0;

	GlOsxPlatformObject* plato = new GlOsxPlatformObject;
	plato->mTarget = this;
	mPlatformHandle = (void*) plato;
	mFbI.SetThisBuffer( pBuf );

	plato->mNSOpenGLContext = GlOsxPlatformObject::gShareMaster;
	plato->mOsxView = nullptr;
	plato->mbNSOpenGlView = false;
	plato->mbInit = false;
  _defaultRTG = new RtGroup(this,miW,miH,1);
  auto rtb = new RtBuffer(ERTGSLOT0,EBufferFormat::RGBA8,miW,miH);
  _defaultRTG->SetMrt(0,rtb);

	//////////////////////////////////////////
	// Bind Texture

  auto texture = _defaultRTG->GetMrt(0)->texture();

	FBI()->SetBufferTexture( texture );

	///////////////////////////////////////////
	// create material


//	[plato->mNSOpenGLContext makeCurrentContext];

	//////////////////////////////////////////////

	//mFbI.InitializeContext( pBuf );
	pBuf->SetContext(this);

	//////////////////////////////////////////////

	plato->mBindOp = [=]()
	{
		[plato->mNSOpenGLContext makeCurrentContext];
	};

	//////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::initializeLoaderContext() {

  meTargetType = ETGTTYPE_LOADING;

miW = 8;
miH = 8;

mCtxBase = 0;

GlOsxPlatformObject* plato = new GlOsxPlatformObject;
plato->mTarget = this;
mPlatformHandle = (void*) plato;

plato->mNSOpenGLContext = GlOsxPlatformObject::gShareMaster;
plato->mOsxView = nullptr;
plato->mbNSOpenGlView = false;
plato->mbInit = false;

_defaultRTG = new RtGroup(this,miW,miH,1);
auto rtb = new RtBuffer(ERTGSLOT0,EBufferFormat::RGBA8,miW,miH);
_defaultRTG->SetMrt(0,rtb);
  auto texture = _defaultRTG->GetMrt(0)->texture();
  FBI()->SetBufferTexture(texture);

plato->mBindOp = [=](){
	[plato->mNSOpenGLContext makeCurrentContext];
    if (this->mTargetDrawableSizeDirty) {
      int w = mainSurfaceWidth();
      int h = mainSurfaceHeight();
      //printf("resizing defaultRTG<%p>\n", _defaultRTG);
      _defaultRTG->Resize(w, h);
      mTargetDrawableSizeDirty = false;
    }
};
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::makeCurrentContext( void )
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

void ContextGL::SwapGLContext( CTXBASE *pCTFL )
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

void* ContextGL::_doBeginLoad()
{
	void* pvoiddat = nullptr;

	while(false==_loadTokens.try_pop(pvoiddat))
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

void ContextGL::_doEndLoad(void*ploadtok)
{
	GlOsxLoadContext* loadctx = (GlOsxLoadContext*) ploadtok;
	printf( "ENDLOAD loadctx<%p> glx<%p>\n", loadctx,loadctx->mGlContext);
	_loadTokens.push(ploadtok);
}

void recomputeHIDPI(Context* ctx){
}
bool _HIDPI() {
  return _macosUseHIDPI;
}
bool _MIXEDDPI() {
  return false;
}
float _currentDPI(){
  return 221.0f; // hardcoded to macbook pro for now..
}

}}
///////////////////////////////////////////////////////////////////////////////

#endif // #if defined( ORK_CONFIG_OPENGL ) && defined(ORK_CONFIG_IX)
