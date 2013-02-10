////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// GLES2
///////////////////////////////////////////////////////////////////////////////

#ifndef _EXECENV_GFX_GLES2_H
#define _EXECENV_GFX_GLES2_H

///////////////////////////////////////////////////////////////////////////////
#if defined( ORK_CONFIG_OPENGL )
///////////////////////////////////////////////////////////////////////////////
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
/////////////////////////////

#define _USE_GLSLFX
//#define _USE_CGFX

#include <ork/kernel/objc.h>
#include <ork/lev2/gfx/texman.h>

///////////////////////////////////////////////////////////////////////////////


#if 0 //defined( _DEBUG )
#define GL_ERRORCHECK() { GLenum iErr = GetGlError(); OrkAssert( iErr==GL_NO_ERROR ); }
#else
#define GL_ERRORCHECK() {}
#endif

///////////////////////////////////////////////////////////////////////////////

#include "glslfxi.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

namespace dxt { struct DDS_HEADER; }

struct FBOObject
{
	static const int kmaxrt = RtGroup::kmaxmrts;
	GLuint mFBOMaster;
	GLuint mCBO[kmaxrt];
	GLuint mDSBO;
	//IDirect3DSurface9 *mFBOREAD[kmaxrt];
	GLuint mTEX[kmaxrt];
	FBOObject();
};

int GetGlError( void );

enum EOpenGLRenderPath
{
	EGLRPATH_ARBVP = 0,		// Arb Vertex Program, Standard GL1.3 Fragment processing
	EGLRPATH_ARBVP_ARBTE,	// Arb Vertex Program, TexEnvCombine/TexEnvAdd
	EGLRPATH_ARBVP_NVRC,	// Arb Vertex Program, NVidia Register Combiners (GF2/GF3/GF4)
	EGLRPATH_ARBVP_NV20,	// Arb Vertex Program, NVidia Register Combiners (GF3/GF4)
	EGLRPATH_ARBVP_ARBFP,	// Arb Vertex Program, Arb Fragment Programe (GFFX/R300)
	EGLRPATH_CGFX,			// CgFX Profile
};

//////////////////////////////////////////////////////////////////////

class GlTextureInterface;

///////////////////////////////////////////////////////////////////////////////

class GlImiInterface : public ImmInterface
{
	virtual void DrawLine( const CVector4 &From, const CVector4 &To );
	virtual void DrawPoint( F32 fx, F32 fy, F32 fz );
	virtual void DrawPrim( const CVector4 *Points, int inumpoints, EPrimitiveType eType );
	virtual void DoBeginFrame() {}
	virtual void DoEndFrame() {}

public:

	GlImiInterface( GfxTargetGL& target );
};

///////////////////////////////////////////////////////////////////////////////

class GlRasterStateInterface : public RasterStateInterface
{
	virtual void BindRasterState( const SRasterState &rState, bool bForce = false );
};

///////////////////////////////////////////////////////////////////////////////

class GlMatrixStackInterface : public MatrixStackInterface
{
	CMatrix4 Ortho( float left, float right, float top, float bottom, float fnear, float ffar ); // virtual
	CMatrix4 Frustum( float left, float right, float top, float bottom, float zn, float zf ); // virtual

public:

	GlMatrixStackInterface( GfxTarget& target );
};

///////////////////////////////////////////////////////////////////////////////

class GlGeometryBufferInterface: public GeometryBufferInterface
{	
	///////////////////////////////////////////////////////////////////////
	// VtxBuf Interface

	virtual void* LockVB( VertexBufferBase& VBuf, int ivbase, int icount );
	virtual void UnLockVB( VertexBufferBase& VBuf );

	virtual const void* LockVB( const VertexBufferBase& VBuf, int ivbase=0, int icount=0 );
	virtual void UnLockVB( const VertexBufferBase& VBuf );

	virtual void ReleaseVB( VertexBufferBase& VBuf );

	//
	
	virtual void*LockIB ( IndexBufferBase& VBuf, int ivbase, int icount );
	virtual void UnLockIB ( IndexBufferBase& VBuf );

	virtual const void* LockIB ( const IndexBufferBase& VBuf, int ibase=0, int icount=0 );
	virtual void UnLockIB ( const IndexBufferBase& VBuf );

	virtual void ReleaseIB( IndexBufferBase& VBuf );

	//

	void BindIndexStreamSource( const IndexBufferBase& IBuf );
	bool BindVertexStreamSource( const VertexBufferBase& VBuf );
	void BindVertexDeclaration( EVtxStreamFormat efmt );

	virtual void DrawPrimitive( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );
	virtual void DrawIndexedPrimitive( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );
	virtual void DrawPrimitiveEML( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );
	virtual void DrawIndexedPrimitiveEML( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );
	
	GfxTargetGL& mTargetGL;

public:

	GlGeometryBufferInterface( GfxTargetGL& target );
};

///////////////////////////////////////////////////////////////////////////////

class GlFrameBufferInterface : public FrameBufferInterface
{
public:

	GlFrameBufferInterface( GfxTargetGL& mTarget );
	~GlFrameBufferInterface();

	virtual void	SetRtGroup( RtGroup* Base );
	void SetAsRenderTarget();

	void InitializeContext( GfxBuffer* pBuf );

	///////////////////////////////////////////////////////

	virtual void	SetViewport( int iX, int iY, int iW, int iH );
	virtual void	SetScissor( int iX, int iY, int iW, int iH );
	virtual void	AttachViewport( CUIViewport *pVP = 0 );
	virtual void	ClearViewport( CUIViewport *pVP );

	virtual void	PushScissor( const SRect &rScissorRect );
	virtual SRect&	PopScissor( void );

	//////////////////////////////////////////////
	// Capture Interface

	virtual void	Capture( const file::Path& pth ) {}
	virtual void	Capture( CaptureBuffer& buffer ) {}
	virtual bool	CaptureToTexture( const CaptureBuffer& capbuf, Texture& tex ) { return false; }
	virtual void	GetPixel( const CVector4 &rAt, GetPixelContext& ctx );

	//////////////////////////////////////////////

	virtual void	ForceFlush( void );
	virtual void	DoBeginFrame( void );
	virtual void	DoEndFrame( void );

	//////////////////////////////////////////////

	void SetColorFBObject( GLuint fbo ) { muColorFBObject=fbo; }
	GLuint GetColorFBObject() const { return muColorFBObject; }

	void SetDepthFBObject( GLuint fbo ) { muDepthFBObject=fbo; }
	GLuint GetDepthFBObject() const { return muDepthFBObject; }

protected:

	GfxTargetGL& mTargetGL;
	GLuint muColorFBObject;
	GLuint muDepthFBObject;
	int miCurScissorX;
	int miCurScissorY;
	int miCurScissorW;
	int miCurScissorH;

};

struct GLTextureObject;

class VdsTextureAnimation : public TextureAnimationBase
{
public:
    VdsTextureAnimation(const AssetPath& pth);
    ~VdsTextureAnimation();  // virtual 
    void UpdateTexture( TextureInterface* txi, Texture* ptex, TextureAnimationInst* ptexanim ); // virtual 
    float GetLengthOfTime( void ) const; // virtual 

    void* ReadFromFrameCache( int iframe, int isize );
    
    int miW, miH;
    int miNumFrames;
    CFile* mpFile;
    std::string mPath;
    dxt::DDS_HEADER* mpDDSHEADER;
    int miFrameBaseSize;
    int miFrameBaseOffset;
    int miFileLength;
    
    std::map<int,int> mFrameCache;
    static const int kframecachesize = 60;
    void* mFrameBuffers[kframecachesize];
private:
    void UpdateFBO(GLTextureObject&glto,float ftime);
};

class QuartzComposerTextureAnimation : public TextureAnimationBase
{
public:
	QuartzComposerTextureAnimation(Objc::Object qcr);
	void UpdateTexture( TextureInterface* txi, Texture* ptex, TextureAnimationInst* ptexanim ); // virtual 
	float GetLengthOfTime( void ) const; // virtual 
	~QuartzComposerTextureAnimation();  // virtual 
private:
//	float			mfQtzTime;
	Objc::Object	mQCRenderer;
	void UpdateQtzFBO(GLTextureObject&glto,float ftime);
};

struct GLTextureObject
{
	GLuint			mObject;
	GLuint			mFbo;
	GLuint			mDbo;

	GLTextureObject() : mObject(0), mFbo(0), mDbo(0) {} //, mfQtzTime(0.0f) {}
	
//	void UpdateQtzFBO();
	
};

class PboSet
{
public:
	
	PboSet( int isize );
	~PboSet();
	GLuint Get();

private:

	static const int knumpbos = 8;
	GLuint mPBOS[knumpbos];
	int miCurIndex;
};

class GlTextureInterface : public TextureInterface
{
public:

	virtual void VRamUpload( Texture *pTex );		// Load Texture Data onto card
	virtual void VRamDeport( Texture *pTex );		// Load Texture Data onto card

	virtual void TexManInit( void );

	virtual bool DestroyTexture( Texture *ptex );
	virtual bool LoadTexture( const AssetPath& fname, Texture *ptex );
	virtual void SaveTexture( const ork::AssetPath& fname, Texture *ptex );
	
	void UpdateAnimatedTexture( Texture *ptex, TextureAnimationInst* tai ); // virtual


	bool LoadDDSTexture( const AssetPath& fname, Texture *ptex );
	bool LoadVDSTexture( const AssetPath& fname, Texture *ptex );
	bool LoadQTZTexture( const AssetPath& fname, Texture *ptex );
	
	GLuint GetPBO( int isize );
	
	
private:

	std::map<int,PboSet*> mPBOSets;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

class GfxTargetGL : public GfxTarget
{
	RttiDeclareConcrete(GfxTargetGL,GfxTarget);
	friend class GfxEnv;

	static const CClass *gpClass;

	///////////////////////////////////////////////////////////////////////

	public:

	GfxTargetGL();

	void FxInit();

	///////////////////////////////////////////////////////////////////////
	
	virtual void SetSize( int ix, int iy, int iw, int ih );

	virtual void DoBeginFrame( void ) {}
	virtual void DoEndFrame( void ) {}

	///////////////////////////////////////////////////////////////////////
	// Shader Interface

	static void SetRenderPath( EOpenGLRenderPath ePath ) { geRenderPath = ePath; }
	static EOpenGLRenderPath GetRenderPath( void ) { return geRenderPath; }

	public:

	//////////////////////////////////////////////
	// Interfaces

	virtual FxInterface*				FXI() { return & mFxI; }
	virtual ImmInterface*				IMI() { return & mImI; }
	virtual RasterStateInterface*		RSI() { return & mRsI; }
	virtual MatrixStackInterface*		MTXI() { return & mMtxI; }
	virtual GeometryBufferInterface*	GBI() { return & mGbI; }
	virtual FrameBufferInterface*		FBI() { return & mFbI; }
	virtual TextureInterface*			TXI() { return & mTxI; }

	///////////////////////////////////////////////////////////////////////

	~GfxTargetGL();

	//////////////////////////////////////////////
	// GfxTarget Concrete Interface

	virtual void InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase );	// make a window
	virtual void InitializeContext( GfxBuffer *pBuf );	// make a pbuffer

	//////////////////////////////////////////////

	void MakeCurrentContext( void );

	//////////////////////////////////////////////

	static void GLinit();
	static void InitGLExt( void );
	static bool HaveGLExtension( const std::string & extname );

	//////////////////////////////////////////////

	void AttachGLContext( CTXBASE *pCTFL );
	void SwapGLContext( CTXBASE *pCTFL );

	int GetNumTexUnits( void ) const { return gNumTexUnits; }

	GlFrameBufferInterface&	GLFBI() { return mFbI; }

	bool SetDisplayMode(DisplayMode *mode); // virtual

protected:

	//////////////////////////////////////////////
	#if defined(_IOS)
	//////////////////////////////////////////////

		void*			mhHWND;
		void*			mGLXContext;
		GfxTargetGL*	mpParentTarget;

		std::stack< void* >		mDCStack;
		std::stack< void* >		mGLRCStack;

	//////////////////////////////////////////////
	#endif
	//////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////
	// Rendering State Info

	EDepthTest			meCurDepthTest;

	////////////////////////////////////////////////////////////////////
	// Rendering Path Variables

	static orkvector< std::string >		gGLExtensions;
	static orkset< std::string >		gGLExtensionSet;
	static U32							gNumTexUnits;
	static EOpenGLRenderPath			geRenderPath;
	std::string							mVtxProgString_NoXFormC;
	static bool							gbUseVBO;
	static bool							gbUseIBO;
	
	static GLenum						geFBOSupport;
	static const GLenum					kNoFBOSupport = 0;
	static const GLuint					kNullFBO = 0xffffffff;


	static const int					kmaxtexobjects = 1024;
	static GLuint						mTextureObjects[kmaxtexobjects];

	///////////////////////////////////////////////////////////////////////////

	GlImiInterface				mImI;
	GlslFxInterface             mFxI;
	GlRasterStateInterface		mRsI;
	GlMatrixStackInterface		mMtxI;
	GlGeometryBufferInterface	mGbI;
	GlFrameBufferInterface		mFbI;
	GlTextureInterface			mTxI;

};

struct IosMainFbo
{
    GLuint mFBO;
    GLuint mColorBuf;
    GLuint mDepthBuf;
    int miWidth;
    int miHeight;
    
    IosMainFbo()
        : mFBO(0)
        , mColorBuf(0)
        , mDepthBuf(0)
        , miWidth(0)
        , miHeight(0)
    {
    }
};

struct GlIosPlatformObject
{
	static id gShareMaster;
	
    //id
	//Objc::Object	mNSOpenGLContext;
	//Objc::Object	mIosView;
	bool			mbInit;
    IosMainFbo      mMainFbo;
    id              mEaglCtx;

	GlIosPlatformObject()
		//: mNSOpenGLContext(nil,nil)
		//, mIosView(nil,nil)
		: mbInit(true)
        , mEaglCtx(0)
		{
		}
};

} }

///////////////////////////////////////////////////////////////////////////////
#endif //  USE_GL_GRAPHICS
///////////////////////////////////////////////////////////////////////////////

#endif // _EXECENV_GFX_GLES2_H
///////////////////////////////////////////////////////////////////////////////
