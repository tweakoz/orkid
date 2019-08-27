////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Win32GL Specific
///////////////////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/glheaders.h>
///////////////////////////////////////////////////////////////////////////////
#include "glfx/glslfxi.h"
/////////////////////////////

#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/kernel/concurrent_queue.h>

///////////////////////////////////////////////////////////////////////////////


#if 0 //defined( _DEBUG )
#define GL_ERRORCHECK() { GLenum iErr = GetGlError(); OrkAssert( iErr==GL_NO_ERROR ); }
#else
#define GL_ERRORCHECK() {}
#endif
#define GL_NF_ERRORCHECK() { GLenum iErr = GetGlError(); if( iErr!=GL_NO_ERROR ) printf("GLERROR FILE<%s> LINE<%d>\n", __FILE__, __LINE__ ); }


///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

namespace dxt { struct DDS_HEADER; }

struct GlFboObject
{
	static const int kmaxrt = RtGroup::kmaxmrts;
	GLuint mFBOMaster;
	GLuint mDSBO;
	GLuint mTEX[kmaxrt];
	GlFboObject();
};

int GetGlError( void );

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
	void BindRasterState( const SRasterState &rState, bool bForce ) override;

	void SetZWriteMask( bool bv ) override;
	void SetRGBAWriteMask( bool rgb, bool a ) override;
	void SetBlending( EBlending eVal ) override;
	void SetDepthTest( EDepthTest eVal ) override;
	void SetCullTest( ECullTest eVal ) override;
	void SetScissorTest( EScissorTest eVal ) override;

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

public:

	GlGeometryBufferInterface( GfxTargetGL& target );

private:
	///////////////////////////////////////////////////////////////////////
	// VtxBuf Interface

	void* LockVB( VertexBufferBase& VBuf, int ivbase, int icount ) final;
	void UnLockVB( VertexBufferBase& VBuf ) final;

	const void* LockVB( const VertexBufferBase& VBuf, int ivbase=0, int icount=0 ) final;
	void UnLockVB( const VertexBufferBase& VBuf ) final;

	void ReleaseVB( VertexBufferBase& VBuf ) final;

	//

	void*LockIB ( IndexBufferBase& VBuf, int ivbase, int icount ) final;
	void UnLockIB ( IndexBufferBase& VBuf ) final;

	const void* LockIB ( const IndexBufferBase& VBuf, int ibase=0, int icount=0 ) final;
	void UnLockIB ( const IndexBufferBase& VBuf ) final;

	void ReleaseIB( IndexBufferBase& VBuf ) final;

	//

	bool BindStreamSources( const VertexBufferBase& VBuf, const IndexBufferBase& IBuf );
	bool BindVertexStreamSource( const VertexBufferBase& VBuf );
	void BindVertexDeclaration( EVtxStreamFormat efmt );

	void DrawPrimitive( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) final;
	void DrawIndexedPrimitive( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) final;
	void DrawPrimitiveEML( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) final;
	void DrawIndexedPrimitiveEML( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) final;

	GfxTargetGL& mTargetGL;

	uint32_t mLastComponentMask;

	void DoBeginFrame() final { mLastComponentMask=0; }
	//virtual void DoEndFrame() {}


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

	virtual void	Clear( const CColor4 &rCol, float fdepth );

	virtual void	SetViewport( int iX, int iY, int iW, int iH );
	virtual void	SetScissor( int iX, int iY, int iW, int iH );

	virtual void	PushScissor( const SRect &rScissorRect );
	virtual SRect&	PopScissor( void );

	//////////////////////////////////////////////
	// Capture Interface

	virtual void	Capture( const RtGroup& inpbuf, int irt, const file::Path& pth );
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

struct GLTextureObject
{
	GLuint			mObject;
	GLuint			mFbo;
	GLuint			mDbo;
	GLenum			mTarget;

	GLTextureObject() : mObject(0), mFbo(0), mDbo(0), mTarget(GL_NONE) {} //, mfQtzTime(0.0f) {}

};

class PboSet
{
public:

	PboSet( int isize );
	~PboSet();
	GLuint Get();

private:

	static const int knumpbos = 1;
	GLuint mPBOS[knumpbos];
	int miCurIndex;
};

struct GlTexLoadReq
{
	Texture *ptex;
	dxt::DDS_HEADER* ddsh;
	GLTextureObject* pTEXOBJ;
	CFile* pTEXFILE;
};

class GlTextureInterface : public TextureInterface
{
public:

	void TexManInit( void ) override;
	bool DestroyTexture( Texture *ptex ) override;
	bool LoadTexture( const AssetPath& fname, Texture *ptex ) override;
	void SaveTexture( const ork::AssetPath& fname, Texture *ptex )  override;
	void ApplySamplingMode( Texture *ptex ) override;
	void UpdateAnimatedTexture( Texture *ptex, TextureAnimationInst* tai )  override;


	void LoadDDSTextureMainThreadPart(const GlTexLoadReq& req);
	bool LoadDDSTexture( const AssetPath& fname, Texture *ptex );
	bool LoadVDSTexture( const AssetPath& fname, Texture *ptex );
	bool LoadQTZTexture( const AssetPath& fname, Texture *ptex );

	GLuint GetPBO( int isize );

	GlTextureInterface( GfxTargetGL& tgt );

private:

	std::map<int,PboSet*> mPBOSets;
	GfxTargetGL& mTargetGL;
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

	void SetSize( int ix, int iy, int iw, int ih ) final;
	void DoBeginFrame( void ) final {}
	void DoEndFrame( void ) final {}

	public:

	//////////////////////////////////////////////
	// Interfaces

	FxInterface*				FXI() final { return & mFxI; }
	ImmInterface*				IMI() final { return & mImI; }
	RasterStateInterface*		RSI() final { return & mRsI; }
	MatrixStackInterface*		MTXI() final { return & mMtxI; }
	GeometryBufferInterface*	GBI() final { return & mGbI; }
	FrameBufferInterface*		FBI() final { return & mFbI; }
	TextureInterface*			TXI() final { return & mTxI; }

	///////////////////////////////////////////////////////////////////////

	~GfxTargetGL();

	//////////////////////////////////////////////

	void MakeCurrentContext( void );

	//////////////////////////////////////////////

	static void GLinit();
	static bool HaveGLExtension( const std::string & extname );

	//////////////////////////////////////////////

	void AttachGLContext( CTXBASE *pCTFL );
	void SwapGLContext( CTXBASE *pCTFL );

	GlFrameBufferInterface&	GLFBI() { return mFbI; }

  void InitializeContext( GfxBuffer *pBuf ) final ;   // make a pbuffer

protected:

    void TakeThreadOwnership() final ;
    bool SetDisplayMode(DisplayMode *mode) final;
    void InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase ) final ;    // make a window
    void* DoBeginLoad() final;
    void DoEndLoad(void*ploadtok) final; // virtual

		void*			mhHWND;
		void*			mGLXContext;
		GfxTargetGL*	mpParentTarget;

		std::stack< void* >		mDCStack;
		std::stack< void* >		mGLRCStack;

	//////////////////////////////////////////////

	static ork::MpMcBoundedQueue<void*> mLoadTokens;

	///////////////////////////////////////////////////////////////////////////
	// Rendering State Info

	EDepthTest			meCurDepthTest;

	////////////////////////////////////////////////////////////////////
	// Rendering Path Variables

	static orkvector< std::string >		gGLExtensions;
	static orkset< std::string >		gGLExtensionSet;

	///////////////////////////////////////////////////////////////////////////

	GlImiInterface				    mImI;
	GlslFxInterface			      mFxI;
	GlRasterStateInterface		mRsI;
	GlMatrixStackInterface		mMtxI;
	GlGeometryBufferInterface	mGbI;
	GlFrameBufferInterface		mFbI;
	GlTextureInterface			  mTxI;
	bool 					 	          mTargetDrawableSizeDirty;

};

} }

///////////////////////////////////////////////////////////////////////////////

#if ! defined(GL_RGBA16F)
#define GL_RGBA16F GL_RGBA16F_ARB
#endif

///////////////////////////////////////////////////////////////////////////////
