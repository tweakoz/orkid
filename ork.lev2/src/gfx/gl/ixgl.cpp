////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "gl.h"

#if defined( ORK_CONFIG_OPENGL ) && defined( LINUX )

#include <ork/lev2/qtui/qtui.h>
#include <QtCore/QMetaObject>
#include <QtX11Extras/QX11Info>
#include <GL/glx.h>

#include <QtGui/qpa/qplatformnativeinterface.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxTargetGL, "GfxTargetGL")

extern "C"
{
	extern bool gbVSYNC;

}

///////////////////////////////////////////////////////////////////////////////
namespace ork {
namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

ork::MpMcBoundedQueue<void*> GfxTargetGL::mLoadTokens;

struct GlIxPlatformObject
{
	static GLXContext gShareMaster;
	static GLXFBConfig* gFbConfigs;
	static XVisualInfo* gVisInfo;
	static Display* gDisplay;

	GLXContext			mGlxContext;
	Display*			mDisplay;
	int 				mXWindowId;
	bool				mbInit;

	GlIxPlatformObject()
		: mbInit(true)
		, mGlxContext(nullptr)
		, mDisplay(nullptr)
		, mXWindowId(0)
	{
	}
};

/////////////////////////////////////////////////////////////////////////

struct GlxLoadContext
{
	GLXContext	mGlxContext;
	GLXContext	mPushedContext;
	Window 		mWindow;
	Display* 	mDisplay;
};

/////////////////////////////////////////////////////////////////////////

Display* GlIxPlatformObject::gDisplay = nullptr;
GLXContext GlIxPlatformObject::gShareMaster = nullptr;
GLXFBConfig* GlIxPlatformObject::gFbConfigs = nullptr;
XVisualInfo* GlIxPlatformObject::gVisInfo = nullptr;
static ork::atomic<int> atomic_init;
int g_rootwin = 0;
///////////////////////////////////////////////////////////////////////////////


static int  g_glx_win_attrlist[] =
{
    //GLX_RGBA,
    //GLX_DOUBLEBUFFER, False,
    //GLX_RED_SIZE, 1,
    //GLX_GREEN_SIZE, 1,
    //GLX_BLUE_SIZE, 1,
    //None
    GLX_X_RENDERABLE, True,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
//    GLX_RED_SIZE, 8,
//    GLX_GREEN_SIZE, 8,
//    GLX_BLUE_SIZE, 8,
//    GLX_ALPHA_SIZE, 8,
//    GLX_DEPTH_SIZE, 24,
//    GLX_STENCIL_SIZE, 8,
    GLX_SAMPLE_BUFFERS, 1,            // <-- MSAA
    GLX_SAMPLES, 16,            // <-- MSAA
    GLX_DOUBLEBUFFER, True,
    None
};


static int  g_glx_off_attrlist[] =
{
    //GLX_RGBA,
    //GLX_DOUBLEBUFFER, False,
    //GLX_RED_SIZE, 1,
    //GLX_GREEN_SIZE, 1,
    //GLX_BLUE_SIZE, 1,
    //None
    GLX_X_RENDERABLE, True,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
//    GLX_RED_SIZE, 8,
//    GLX_GREEN_SIZE, 8,
//    GLX_BLUE_SIZE, 8,
//    GLX_ALPHA_SIZE, 8,
//    GLX_DEPTH_SIZE, 24,
//    GLX_STENCIL_SIZE, 8,
    GLX_SAMPLE_BUFFERS, 1,            // <-- MSAA
    GLX_SAMPLES, 16,            // <-- MSAA
    GLX_DOUBLEBUFFER, True,
    None
};

/*int glx_attribs[] =
{
	GLX_RGBA,
    GLX_DOUBLEBUFFER, True,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    None
};*/

typedef GLXContext (*glXcca_proc_t)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
static GLXFBConfig gl_this_fb_config;
static glXcca_proc_t GLXCCA = nullptr;
PFNGLPATCHPARAMETERIPROC GLPPI = nullptr;


static int gl46_context_attribs[] =
{
    GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
    GLX_CONTEXT_MINOR_VERSION_ARB, 6,
    GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
#if 1
    GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
    None
};

void check_debug_log()
{
    GLuint count = 1024; // max. num. of messages that will be read from the log
    GLsizei bufsize = 32768;
    GLenum sources[count];
    GLenum types[count];
    GLuint ids[count];
    GLenum severities[count];
    GLsizei lengths[count];
    GLchar messageLog[bufsize];

    auto retVal = glGetDebugMessageLog( count, bufsize, sources, types,
                                           ids, severities, lengths, messageLog);
    if(retVal > 0)
    {
        int pos = 0;
        for(int i=0; i<retVal; i++)
        {
            //DebugOutputToFile(sources[i], types[i], ids[i], severities[i],&messageLog[pos]);
            printf( "GLDEBUG msg<%d:%s>\n", i, & messageLog[pos] );

            pos += lengths[i];
        }
    }
}

void GfxTargetGL::GLinit()
{
	int iinit = atomic_init++;

	if( 0 != iinit )
	{
		return;
	}

	orkprintf( "INITOPENGL\n" );

	GLXFBConfig fb_config;
	//XInitThreads();
	Display* x_dpy = XOpenDisplay(0);
	assert(x_dpy!=0);
	int x_screen = 0; // screen?
	int inumconfigs = 0;
	GlIxPlatformObject::gFbConfigs = glXChooseFBConfig(x_dpy,x_screen,g_glx_win_attrlist,&inumconfigs);
	printf( "gFbConfigs<%p>\n", (void*) GlIxPlatformObject::gFbConfigs );
	printf( "NUMCONFIGS<%d>\n", inumconfigs );
	assert(inumconfigs>0);

	gl_this_fb_config = GlIxPlatformObject::gFbConfigs[0];

	GlIxPlatformObject::gVisInfo = glXGetVisualFromFBConfig( x_dpy, gl_this_fb_config );
	XVisualInfo* vi = GlIxPlatformObject::gVisInfo;
	printf( "vi<%p>\n", (void*) vi );

	printf( "numfbconfig<%d>\n", inumconfigs );
	assert(GlIxPlatformObject::gFbConfigs!=0);

	GlIxPlatformObject::gDisplay = x_dpy;

	g_rootwin = RootWindow(x_dpy,x_screen);
	XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(x_dpy,g_rootwin,vi->visual,AllocNone);
    swa.border_pixel = 0;
    //swa.background_pixel = 0;
    swa.event_mask = StructureNotifyMask;
    swa.override_redirect = true;

    uint32_t win_flags = CWBorderPixel;
//    win_flags |= CWBackPixel;
    win_flags |= CWColormap;
    win_flags |= CWEventMask;
    win_flags |= CWOverrideRedirect;
    Window dummy_win = XCreateWindow(x_dpy,g_rootwin,
        0,0,1,1,
        0,GlIxPlatformObject::gVisInfo->depth,InputOutput,
        GlIxPlatformObject::gVisInfo->visual,
        win_flags,
        &swa );

    XMapWindow(x_dpy,dummy_win);

    ///////////////////////////////////////////////////////////////
    // create oldschool context just to determine if the newschool method
    //  is available
		///////////////////////////////////////////////////////////////

    GLXContext old_school = glXCreateContext(x_dpy, GlIxPlatformObject::gVisInfo, nullptr, GL_TRUE);

    GLXCCA =  (glXcca_proc_t) glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");

    GLPPI = (PFNGLPATCHPARAMETERIPROC) glXGetProcAddress((const GLubyte*)"glPatchParameteri");

    assert( GLPPI!=nullptr );

    glXMakeCurrent(x_dpy, 0, 0);
    glXDestroyContext(x_dpy, old_school);

    if (GLXCCA == nullptr)
    {
        printf( "glXCreateContextAttribsARB entry point not found. Aborting.\n");
        assert(false);
    }



    ///////////////////////////////////////////////////////////////


    GlIxPlatformObject::gShareMaster = GLXCCA(x_dpy, gl_this_fb_config, NULL, true, gl46_context_attribs);

    glXMakeCurrent(x_dpy, dummy_win, GlIxPlatformObject::gShareMaster);

    assert(GlIxPlatformObject::gShareMaster!=nullptr);

	printf( "display<%p> screen<%d> rootwin<%d> numcfgs<%d> gsharemaster<%p>\n", x_dpy, x_screen, g_rootwin, inumconfigs, GlIxPlatformObject::gShareMaster );

	///////////////////////////////////////////////////////////////

	if(!gladLoadGL()) { exit(-1); }

 	printf("OpenGL Version %d.%d loaded\n", GLVersion.major, GLVersion.minor);

	//printf( "glad_glDrawMeshTasksNV<%p>\n", glad_glDrawMeshTasksNV );
	//printf( "glad_glDrawMeshTasksIndirectNV<%p>\n", glad_glDrawMeshTasksIndirectNV );
	//printf( "glad_glMultiDrawMeshTasksIndirectNV<%p>\n", glad_glMultiDrawMeshTasksIndirectNV );
	//printf( "glad_glMultiDrawMeshTasksIndirectCountNV<%p>\n", glad_glMultiDrawMeshTasksIndirectCountNV );/

	//printf( "glObjectLabel<%p>\n", glObjectLabel );
	//printf( "glPushDebugGroup<%p>\n", glPushDebugGroup );
	//printf( "glPopDebugGroup<%p>\n", glPopDebugGroup );

	//printf( "glad_glInsertEventMarkerEXT<%p>\n", glad_glInsertEventMarkerEXT );

	glInsertEventMarkerEXT = glad_glInsertEventMarkerEXT;
	//glPushGroupMarkerEXT = glad_glPushDebugGroup;
	//glPopGroupMarkerEXT = glad_glPopDebugGroup;

	//printf( "GLAD_GL_EXT_debug_label<%d>\n", int(GLAD_GL_EXT_debug_label) );
	//printf( "GLAD_GL_EXT_debug_marker<%d>\n", int(GLAD_GL_EXT_debug_marker) );
	//printf( "GLAD_GL_NV_mesh_shader<%d>\n", int(GLAD_GL_NV_mesh_shader) );
	//printf( "GLAD_GL_KHR_debug<%d>\n", int(GLAD_GL_KHR_debug));

	for( int i=0; i<1; i++ )
	{
		GlxLoadContext* loadctx = new GlxLoadContext;

        GLXContext gctx = GLXCCA(x_dpy,gl_this_fb_config,GlIxPlatformObject::gShareMaster,GL_TRUE,gl46_context_attribs);

		loadctx->mGlxContext = gctx;
		loadctx->mWindow = g_rootwin;
		loadctx->mDisplay = GlIxPlatformObject::gDisplay;

		mLoadTokens.push( (void*) loadctx );
	}

}

///////////////////////////////////////////////////////////////////////////////

std::string GetGlErrorString( void );

void OpenGlGfxTargetInit()
{
	GfxEnv::SetTargetClass(GfxTargetGL::GetClassStatic());
	GfxTargetGL::GLinit();
	auto target = new GfxTargetGL;
	auto poutbuf = new GfxBuffer(0,0,0,1280,720);
	GfxEnv::GetRef().SetLoaderTarget(target);
	target->InitializeContext(poutbuf);
}

/////////////////////////////////////////////////////////////////////////

GfxTargetGL::GfxTargetGL()
	: GfxTarget()
	, mFxI( *this )
	, mImI( *this )
	, mRsI( *this )
	, mGbI( *this )
	, mFbI( *this )
	, mTxI( *this )
	, mMtxI( *this )
	, mCI( *this )
{
	GfxTargetGL::GLinit();

	FxInit();

}

/////////////////////////////////////////////////////////////////////////

GfxTargetGL::~GfxTargetGL()
{
}

/////////////////////////////////////////////////////////////////////////


static QWindow* windowForWidget(const QWidget* widget)
{
    QWindow* window = widget->windowHandle();
    if (window)
        return window;
    auto nativeParent = widget->nativeParentWidget();
    if (nativeParent)
        return nativeParent->windowHandle();
    return 0;
}

Window getHandleForWidget(const QWidget* widget)
{
    auto window = windowForWidget(widget);
    if (window)
    {
        auto PNI = QGuiApplication::platformNativeInterface();
        return (Window)(PNI->nativeResourceForWindow(QByteArrayLiteral("handle"), window));
    }
    return 0;
}
void GfxTargetGL::InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase  )
{
	///////////////////////
	GlIxPlatformObject* plato = new GlIxPlatformObject;
	mCtxBase = pctxbase;
	mPlatformHandle = (void*) plato;
	///////////////////////
	CTQT* pctqt = (CTQT*) pctxbase;
	auto pctxW = pctqt->GetQWidget();
	Display* x_dpy = QX11Info::display();
	int x_screen = QX11Info::appScreen();
	int x_window = getHandleForWidget(pctxW);

	//auto pvis = (XVisualInfo*) x11info.visual();
	XVisualInfo* vinfo = GlIxPlatformObject::gVisInfo;
	///////////////////////
    int DWMM = DisplayWidthMM(x_dpy,x_screen);
    int DHMM = DisplayHeightMM(x_dpy,x_screen);
	int RESW = DisplayWidth(x_dpy,x_screen);
    int RESH = DisplayHeight(x_dpy,x_screen);
    float CDPIX = float(RESW)/float(DWMM)*25.4f;
    float CDPIY = float(RESH)/float(DHMM)*25.4f;
    int DPIX = QX11Info::appDpiX(x_screen);
    int DPIY = QX11Info::appDpiY(x_screen);
	printf( "GfxTargetGL<%p> dpy<%p> screen<%d> vis<%p>\n", this, x_dpy, x_screen, vinfo );
    printf( "dpi <%d %d>\n", DPIX, DPIY );
    printf( "res <%d %d>\n", RESW, RESH );
    printf( "siz <%d %d>\n", DWMM, DHMM );
    printf( "cpi <%g %g>\n", CDPIX, CDPIY );
    float avgdpi = (CDPIX+CDPIY)*0.5f;
    _hiDPI = avgdpi>180.0;
    if( _hiDPI ) {
      printf("HIDPI enabled\n");
      ork::lev2::_HIDPI = _hiDPI; // todo remove when the correct plumbing is in place
    }

	plato->mGlxContext = GLXCCA(x_dpy,gl_this_fb_config,plato->gShareMaster,GL_TRUE,gl46_context_attribs);

	plato->mDisplay = x_dpy;
	plato->mXWindowId = pctxW->winId();
	printf( "ctx<%p>\n", plato->mGlxContext );

	makeCurrentContext();

	PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT;
  PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA;
  PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI;

	glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddress( (const GLubyte*)"glXSwapIntervalEXT");
  if (glXSwapIntervalEXT != NULL){
    glXSwapIntervalEXT(x_dpy, x_window, 0);
		printf( "DISABLEVSYNC VIA EXT\n");
	}
	//else
	  //glXSwapIntervalMESA = (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddress( (const GLubyte*)"glXSwapIntervalMESA");
	  //if ( glXSwapIntervalMESA != NULL )
	    // glXSwapIntervalMESA(0);
	  else{
	     glXSwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)glXGetProcAddress( (const GLubyte*)"glXSwapIntervalSGI");
	     if ( glXSwapIntervalSGI != NULL )
	        glXSwapIntervalSGI(0);
					printf( "DISABLEVSYNC VIA SGI\n");
		}

	mFbI.SetThisBuffer( pWin );
	mFbI.SetOffscreenTarget( false );
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

	GlIxPlatformObject* plato = new GlIxPlatformObject;
	mPlatformHandle = (void*) plato;
	mFbI.SetThisBuffer( pBuf );

	plato->mGlxContext = GlIxPlatformObject::gShareMaster;
	plato->mbInit = false;
	plato->mDisplay = GlIxPlatformObject::gDisplay;
	plato->mXWindowId = g_rootwin;
	mFbI.SetOffscreenTarget( true );

	//////////////////////////////////////////
	// Bind Texture

	Texture* pTexture = new Texture();
	pTexture->_width = miW;
	pTexture->_height = miH;

	FBI()->SetBufferTexture( pTexture );

	///////////////////////////////////////////
	// create material

	GfxMaterialUITextured* pmtl = new GfxMaterialUITextured(this);
	pBuf->SetMaterial(pmtl);
	pmtl->SetTexture( ETEXDEST_DIFFUSE, pTexture );
	pBuf->SetTexture(pTexture);

//	makeCurrentContext();

	//////////////////////////////////////////////

	//mFbI.InitializeContext( pBuf );
	//////////////////////////////////////////////

	pBuf->SetContext(this);
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::makeCurrentContext( void )
{
	GlIxPlatformObject* plato = (GlIxPlatformObject*) mPlatformHandle;
	OrkAssert(plato);
	if( plato )
	{
		bool bOK = glXMakeCurrent(plato->mDisplay,plato->mXWindowId,plato->mGlxContext);
	//	OrkAssert(bOK);
	}
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::SwapGLContext( CTXBASE *pCTFL )
{
	GlIxPlatformObject* plato = (GlIxPlatformObject*) mPlatformHandle;
	OrkAssert(plato);
	if( plato && (plato->mXWindowId>0) )
	{
		glXMakeCurrent(plato->mDisplay,plato->mXWindowId,plato->mGlxContext);
		glXSwapBuffers(plato->mDisplay,plato->mXWindowId);
	}

}

/////////////////////////////////////////////////////////////////////////

void* GfxTargetGL::DoBeginLoad()
{
	void* pvoiddat = nullptr;

	while(false==mLoadTokens.try_pop(pvoiddat))
	{
		usleep(1<<10);
	}

	GlxLoadContext* loadctx = (GlxLoadContext*) pvoiddat;

	loadctx->mPushedContext = glXGetCurrentContext();

	bool bOK = glXMakeCurrent(loadctx->mDisplay,loadctx->mWindow,loadctx->mGlxContext);

	printf( "BEGINLOAD loadctx<%p> glx<%p> OK<%d>\n", loadctx,loadctx->mGlxContext,int(bOK));

	OrkAssert(bOK);

	return pvoiddat;
}

/////////////////////////////////////////////////////////////////////////

void GfxTargetGL::DoEndLoad(void*ploadtok)
{
	GlxLoadContext* loadctx = (GlxLoadContext*) ploadtok;
	printf( "ENDLOAD loadctx<%p> glx<%p>\n", loadctx,loadctx->mGlxContext);
	mLoadTokens.push(ploadtok);
}

/////////////////////////////////////////////////////////////////////////

}}
///////////////////////////////////////////////////////////////////////////////

#endif // #if defined( ORK_CONFIG_OPENGL ) && defined(IX)
