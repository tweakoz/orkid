////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#if defined( ORK_CONFIG_OPENGL ) && defined( _IOS )

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"
#include <ork/file/fileenv.h>
#include <ork/file/filedev.h>

#include <OpenGLES/EAGL.h>
#include <ork/kernel/objc.h>

#import <UIKit/UIKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <QuartzCore/QuartzCore.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxTargetGL, "GfxTargetGL")

extern bool gbVSYNC;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

static CAEAGLLayer* gEAGLLayer = nullptr;
static EAGLContext* gNSGLdefaultctx = nil;

/////////////////////////////////////////////////////////////////////////

//CGLContextObj gOGLdefaultctx;
//CGLPixelFormatObj gpixfmt;
id gNSGLfirstctx = nil;
id gNSGLcurrentctx = nil;
id GlIosPlatformObject::gShareMaster = nil;
id gNSPixelFormat = nil;
std::vector<ork::Objc::Object*> gWindowContexts;

///////////////////////////////////////////////////////////////////////////////

void GfxTargetGL::GLinit()
{
	static bool gbInit = true;
	
	if( gbInit )
	{
		gbInit = false;
	
        gNSGLdefaultctx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];	
		orkprintf( "INITOPENGL context<%p>\n", gNSGLdefaultctx );
        		
		GlIosPlatformObject::gShareMaster = gNSGLdefaultctx;

        [EAGLContext setCurrentContext: gNSGLdefaultctx];

		//gNSPixelFormat = new Objc::Object( 
		//	Objc::Class("NSOpenGLPixelFormat").Alloc().InitV("initWithCGLPixelFormatObj:",  gpixfmt )
		//);
		        
		InitGLExt();
	}
}

///////////////////////////////////////////////////////////////////////////////

std::string GetGlErrorString( void );

void OpenGlGfxTargetInit()
{
	///////////////////////////////////////////////////////////
	Objc::Object nsAutoReleasePool = Objc::Class("NSAutoreleasePool").Alloc().Init();
	///////////////////////////////////////////////////////////
	GfxEnv::SetTargetClass(GfxTargetGL::GetClassStatic());
	//const ork::SFileDevContext& datactx = ork::CFileEnv::UrlBaseToContext( "data" );//, DataDirContext );
}

/////////////////////////////////////////////////////////////////////////

GfxTargetGL::GfxTargetGL() 
	: GfxTarget()
	, mFxI( *this )
	, mImI( *this )
	, mRsI()
	, mGbI( *this )
	, mFbI( *this )
	, mTxI()
	, mMtxI( *this )
{
	GfxTargetGL::GLinit();
	FxInit();
}

/////////////////////////////////////////////////////////////////////////

GfxTargetGL::~GfxTargetGL()
{
}

/////////////////////////////////////////////////////////////////////////////

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
		//case GL_STACK_OVERFLOW:
		//	RVAL =  (std::string) "GL_STACK_OVERFLOW";
		//	break;
		//case GL_STACK_UNDERFLOW:
		//	RVAL =  (std::string) "GL_STACK_UNDERFLOW";
		//	break;
		case GL_OUT_OF_MEMORY:
			RVAL =  (std::string) "GL_OUT_OF_MEMORY";
			break;
		default:
			break;
	}
	return RVAL;
}

/////////////////////////////////////////////////////////////////////////

int GetGlError( void )
{
	int err = glGetError();

	std::string errstr = GetGlErrorString( err );

	if( err != GL_NO_ERROR )
		orkprintf( "GLERROR [%s]\n", errstr.c_str() );

	return err;
}

/////////////////////////////////////////////////////////////////////////

bool GfxTargetGL::SetDisplayMode(DisplayMode *mode)
{
	return false;
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase  )
{
#if 1
	GlIosPlatformObject* plato = new GlIosPlatformObject;
	IosMainFbo& main_fbo = plato->mMainFbo;
    
	mCtxBase = pctxbase;
	
	//GetCtxBase
	
	mPlatformHandle = (void*) plato;
	
	//id iosview = (id) mCtxBase->GetThisXID();
	//Class iosviewclass = object_getClass(iosview);
	//Class iosviewparentclass = class_getSuperclass(iosviewclass);

	//const char* iosviewClassName = object_getClassName(iosview);
	//const char* iosviewParentClassName = class_getName(iosviewparentclass);
	
	//orkprintf( "iosview<%p> class<%s> parclass<%s>\n", iosview, iosviewClassName, iosviewParentClassName );

	//plato->mIosView = iosview;
	
    plato->mEaglCtx = gNSGLdefaultctx;
    [EAGLContext setCurrentContext:plato->mEaglCtx];

    // Create Main FBO
    glGenFramebuffers(1, &main_fbo.mFBO);
    glGenRenderbuffers(1, &main_fbo.mColorBuf);
    glGenRenderbuffers(1, &main_fbo.mDepthBuf);

    glBindRenderbuffer(GL_RENDERBUFFER, main_fbo.mColorBuf);
    [plato->mEaglCtx renderbufferStorage:GL_RENDERBUFFER fromDrawable:gEAGLLayer];
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &main_fbo.miWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &main_fbo.miHeight);

    NSLog( @"MiniOrkView Width<%d> Height<%d>\n", main_fbo.miWidth, main_fbo.miHeight );

    glBindRenderbuffer(GL_RENDERBUFFER, main_fbo.mDepthBuf);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, main_fbo.miWidth, main_fbo.miHeight);
    glBindRenderbuffer(GL_RENDERBUFFER, main_fbo.mColorBuf);

    glBindFramebuffer(GL_FRAMEBUFFER, main_fbo.mFBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, main_fbo.mColorBuf); 
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, main_fbo.mDepthBuf);

	//////////////////////////////////////////////
	// create and init nsContext
	//////////////////////////////////////////////

	/*Objc::Object nsPixelFormat = Objc::Class("NSOpenGLPixelFormat").Alloc().InitV("initWithCGLPixelFormatObj:",  gpixfmt );
	
	plato->mNSOpenGLContext = Objc::Class("NSOpenGLContext").Alloc().InitII( "initWithFormat:shareContext:", nsPixelFormat.mInstance, GlOsxPlatformObject::gShareMaster );

	if( gNSGLfirstctx==nil )
		gNSGLfirstctx = & plato->mNSOpenGLContext;

	gWindowContexts.push_back( & plato->mNSOpenGLContext );

	plato->mNSOpenGLContext.InvokeV("makeCurrentContext");
  		
	//////////////////////////////////////////////
	// set its view
	plato->mNSOpenGLContext.InvokeVI( "setView:", osxview );
	//////////////////////////////////////////////
	// turn on vsync
	
	GLint vsyncparams[] = 
	{
		gbVSYNC ? 1 : 0,
	};
	plato->mNSOpenGLContext.InvokeVVN( "setValues:forParameter:", (void*)&vsyncparams[0], kCGLCPSwapInterval );
	
	mFbI.SetThisBuffer( pWin );
*/
#endif
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::InitializeContext( GfxBuffer *pBuf )
{
/*	miW = pBuf->miWidth;
	miH = pBuf->miHeight;
	miX = 0;
	miY = 0;
	pBuf->mpContext = this;
	FBI()->SetOffscreenTarget( true );

	mCtxBase = 0;

	GlOsxPlatformObject* plato = new GlOsxPlatformObject;
			
	mPlatformHandle = (void*) plato;

	//////////////////////////////////////////////
	// create and init nsContext
	//////////////////////////////////////////////

	//Objc::Object nsPixelFormat = Objc::Class("NSOpenGLView").InvokeClassMethodI("defaultPixelFormat" );
	Objc::Object nsPixelFormat = Objc::Class("NSOpenGLPixelFormat").Alloc().InitV("initWithCGLPixelFormatObj:",  gpixfmt );
	//nsPixelFormat.Dump();
		
	plato->mNSOpenGLContext = Objc::Class("NSOpenGLContext").Alloc().InitII( "initWithFormat:shareContext:", nsPixelFormat.mInstance, GlOsxPlatformObject::gShareMaster );
	//plato->mNSOpenGLContext.Dump();

	if( 0 == GlOsxPlatformObject::gShareMaster )
	{
		GlOsxPlatformObject::gShareMaster = plato->mNSOpenGLContext.mInstance;
	}
	plato->mNSOpenGLContext.InvokeV("makeCurrentContext");
  		
	//////////////////////////////////////////////
	
	mFbI.SetThisBuffer( pBuf );
	mFbI.InitializeContext( pBuf );
    */
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::MakeCurrentContext( void )
{
#if 1
	ork::lev2::GlIosPlatformObject* plato = (ork::lev2::GlIosPlatformObject*) GetPlatformHandle();
	ork::lev2::IosMainFbo& main_fbo = plato->mMainFbo;

    [EAGLContext setCurrentContext:plato->mEaglCtx];
    glBindFramebuffer(GL_FRAMEBUFFER, main_fbo.mFBO);
    glViewport(0,0,main_fbo.miWidth,main_fbo.miHeight);
    glScissor(0,0,main_fbo.miWidth,main_fbo.miHeight);
    /////////////
    // clear
    /////////////
    int ir=rand()%256;
    int ig=rand()%256;
    int ib=rand()%256;
    float fr = float(ir)/256.0f;
    float fg = float(ig)/256.0f;
    float fb = float(ib)/256.0f;
    glClearColor(fr,fg,fb,1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    /////////////

	//if( 0 == mCtxBase ) return;
	/*
	// GetCtxBase
	GlOsxPlatformObject* plato = (GlOsxPlatformObject*) mPlatformHandle;
	if( plato )
	{
		if( mCtxBase )
		{
			id osxview = (id) mCtxBase->GetThisXID();
			GfxWindow *pWin = (GfxWindow *) mFbI.GetThisBuffer();
			if( osxview != plato->mOsxView.mInstance )
			{
				OrkAssert(false);
				InitializeContext( pWin,mCtxBase );
			}
		}
		//printf( "UPDATE<%p>\n", this );
		if(plato->mbInit)
		{
			if( plato->mOsxView.mInstance )
			{
				plato->mNSOpenGLContext.InvokeVI( "setView:", plato->mOsxView.mInstance );
			}
			plato->mbInit=false;
		}
		plato->mNSOpenGLContext.InvokeV( "makeCurrentContext" );
		plato->mNSOpenGLContext.InvokeV( "update" );
		
		gNSGLcurrentctx = & plato->mNSOpenGLContext;
	}
*/
#endif
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::SwapGLContext( CTXBASE *pCTFL )
{
#if 1
	ork::lev2::GlIosPlatformObject* plato = (ork::lev2::GlIosPlatformObject*) GetPlatformHandle();
	IosMainFbo& main_fbo = plato->mMainFbo;
    /////////////
    // discard
    /////////////
    const GLenum discards[]  = {GL_DEPTH_ATTACHMENT};
    glDiscardFramebufferEXT(GL_FRAMEBUFFER,1,discards);
    /////////////
    // present
    /////////////
    glBindRenderbuffer(GL_RENDERBUFFER, main_fbo.mColorBuf);
    [plato->mEaglCtx presentRenderbuffer:GL_RENDERBUFFER];
/*
	//if( 0 == pCTFL ) return;
	
	if( 0 )
	{
		glViewport( miX, miY, miW, miH );
		glScissor( miX, miY, miW, miH );
		
		MTXI()->PushUIMatrix();
		CMatrix4 uimtx_M = MTXI()->RefMMatrix();
		CMatrix4 uimtx_V = MTXI()->RefVMatrix();
		CMatrix4 uimtx_P = MTXI()->RefPMatrix();
		//CMatrix4 
		MTXI()->PopUIMatrix();
		glMatrixMode( GL_PROJECTION );
		glLoadMatrixf( uimtx_P.GetArray() );
		glMatrixMode( GL_MODELVIEW );
		glLoadMatrixf( uimtx_V.GetArray() );
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glColor4f( 0.0f, 0.0f, 0.0f, 1.0f );
		
		glBegin( GL_LINES );
			glVertex2f( miX, miY );
			glVertex2f( miX+miW, miY+miH );
			glVertex2f( miX+miW, miY );
			glVertex2f( miX, miY+miH );
		glEnd();
		glFinish();
	}
	// GetCtxBase
	GlOsxPlatformObject* plato = (GlOsxPlatformObject*) mPlatformHandle;
	if( plato )
	{
		//printf( "SWAP<%p>\n", this );
		plato->mNSOpenGLContext.InvokeV( "flushBuffer" );
	}
	//Objc::Class("NSOpenGLContext").InvokeClassMethodV("clearCurrentContext" );
    */
#endif
}

}}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
@interface MiniOrkView : UIView
@end
@implementation MiniOrkView
{
    //ork::lev2::GfxTargetGL* mpMainGfxTgt;
    //ork::lev2::GfxWindow*   mpMainGfxWin;
}
//////////////////
- (UIView*) initWithFrame:(CGRect)frame
{
    srand(100);
    self = [super initWithFrame:frame];
    self.layer.opaque = YES; // compositing performance optimization
    ork::lev2::gEAGLLayer = (CAEAGLLayer*) self.layer;
    ApplicationStack::Top()->Init();

    //int iw = frame.size.width;
    //int ih = frame.size.height;
    //NSLog( @"MiniOrkView::init iw<%d> ih<%d>\n", iw, ih );
    
    //mpMainGfxWin = new ork::lev2::GfxWindow(0,0,iw,ih);
    //mpMainGfxWin->mPlatformHandle = (void*) self.layer;
    //mpMainGfxTgt = (ork::lev2::GfxTargetGL*) mpMainGfxWin->GetContext();

    return self;
}
//////////////////
+ (Class)layerClass
{
    return [CAEAGLLayer class];
}
//////////////////
- (void)drawFrame
{
    ApplicationStack::Top()->Update();
    ApplicationStack::Top()->Draw();
	/*ork::lev2::GlIosPlatformObject* plato = (ork::lev2::GlIosPlatformObject*) mpMainGfxTgt->GetPlatformHandle();
	ork::lev2::IosMainFbo& main_fbo = plato->mMainFbo;

    [EAGLContext setCurrentContext:plato->mEaglCtx];
    /////////////
    // Bind Main FBO, and set max region
    /////////////
    glBindFramebuffer(GL_FRAMEBUFFER, main_fbo.mFBO);
    glViewport(0,0,main_fbo.miWidth,main_fbo.miHeight);
    glScissor(0,0,main_fbo.miWidth,main_fbo.miHeight);
    /////////////
    // clear
    /////////////
    int ir=rand()%256;
    int ig=rand()%256;
    int ib=rand()%256;
    float fr = float(ir)/256.0f;
    float fg = float(ig)/256.0f;
    float fb = float(ib)/256.0f;
    glClearColor(fr,fg,fb,1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    /////////////
    // discard
    /////////////
    const GLenum discards[]  = {GL_DEPTH_ATTACHMENT};
    glDiscardFramebufferEXT(GL_FRAMEBUFFER,1,discards);
    /////////////
    // present
    /////////////
    glBindRenderbuffer(GL_RENDERBUFFER, main_fbo.mColorBuf);
    [plato->mEaglCtx presentRenderbuffer:GL_RENDERBUFFER];*/
}
//////////////////
- (void) onTimer:(NSTimer*)timer
{
    [self setNeedsDisplay];
}
@end
///////////////////////////////////////////////////////////////////////////////
@interface MiniOrkAppDelegate: NSObject
@end
///////////////////////////////////////////////////////////////////////////////
@implementation MiniOrkAppDelegate
{
     UIWindow*      mainWindow;
     MiniOrkView*   mainView;
}

- (void) applicationDidFinishLaunching: (UIApplication *) application
{
    // create window and view at max size
    auto main_screen = [UIScreen mainScreen];
    auto screen_bounds = [main_screen bounds];
    auto maxview_size = [main_screen applicationFrame];
    mainWindow = [[UIWindow alloc] initWithFrame:screen_bounds];
    mainView = [[MiniOrkView alloc] initWithFrame:maxview_size];

    // add view to window, and focus input on window
    [mainWindow addSubview: mainView];
    [mainWindow makeKeyAndVisible];
    
    // setup animation
    auto displayLink = [mainWindow.screen displayLinkWithTarget:mainView selector:@selector(drawFrame)];
    [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void) applicationWillTerminate
{
     [mainWindow release];
}

- (UIWindow *) getMainWindow
{
     return mainWindow;
}

@end

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
void OpenGlGfxTargetMain()
{
    NSLog( @"OpenGlGfxTargetMain()\n" );
    int argc = 0;
    char** argv = 0;
    
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    NSLog( @"entering UIApplicationMain()\n" );
    int r = UIApplicationMain(argc, argv, @"UIApplication", @"MiniOrkAppDelegate");
    [pool release];
}
}}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#endif // #if defined( ORK_CONFIG_OPENGL ) && defined(IX)
