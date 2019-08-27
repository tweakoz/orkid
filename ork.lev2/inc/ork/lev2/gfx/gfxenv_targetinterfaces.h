////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Graphics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/timer.h>

#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>

#include <ork/lev2/gfx/gfxrasterstate.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/kernel/mutex.h>
#include <ork/math/TransformNode.h>
#include <ork/object/Object.h>
#include <ork/file/path.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

class FxShader;
struct FxShaderTechnique;
struct FxShaderParam;
class RenderContextInstData;
class GfxMaterial;
class VertexBufferBase;
class IndexBufferBase;
class GfxBuffer;
class TextureAnimationInst;
class PickBufferBase;
class RtGroup;
class RtBuffer;

////////////////////////////////////////////////////////////////////////////////

class IManipInterface : public ork::Object
{
	RttiDeclareAbstract(IManipInterface,ork::Object);

	public:

	IManipInterface() {}

	virtual const TransformNode &GetTransform(rtti::ICastable* pobj) = 0;
	virtual void SetTransform(rtti::ICastable* pobj, const TransformNode& node) = 0;
	virtual void Attach(rtti::ICastable* pobj) {}; /// optional - only needed if an object needs to know when it is going to be manipulated
	virtual void Detach(rtti::ICastable* pobj) {}; /// optional - only needed if an object needs to know when it will stop being manipulated
};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Buffer for capturing data from VRAM to ram
/// primarily useful for GP-GPU Tasks (general purpose computation on the GPU)
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class CaptureBuffer
{
public:
	int				GetStride() const;
	int				CalcDataIndex( int ix, int iy ) const;
	void			SetWidth( int iw );
	void			SetHeight( int ih );
	int				GetWidth() const;
	int				GetHeight() const;
	void			SetFormat( EBufferFormat efmt );
	EBufferFormat	GetFormat() const;
	const void*		GetData() const { return mpData; }
	void			CopyData( const void* pfrom, int isize );
	////////////////////////////
	CaptureBuffer();
	~CaptureBuffer();
	////////////////////////////
private:
	////////////////////////////
	EBufferFormat	meFormat;
	int				miW;
	int				miH;
	void*			mpData;
	////////////////////////////
};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Pixel Getter Context
///  this can grab pixels from buffers, including multiple pixels from MRT's
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

struct GetPixelContext
{
	ork::rtti::ICastable * GetObject( PickBufferBase*pb, int ichan ) const;
	void* GetPointer( int ichan ) const;
	GetPixelContext();

	//////////////////////

	enum EPixelUsage
	{
		EPU_FLOAT = 0,
		EPU_PTR32 ,
	};

	static const int kmaxitems = 4;

	GfxBuffer*	mAsBuffer;
	RtGroup*	mRtGroup;
	int			miMrtMask;
	CColor4		mPickColors[kmaxitems];
	EPixelUsage	mUsage[kmaxitems];
	anyp 		mUserData;

};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Abstract Display List (supported on WII and DX9)
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class DisplayList
{
public:

	DisplayList() : mPlatformHandle(0) {}

	void SetPlatformHandle(void*ph) { mPlatformHandle=ph; }
	void* GetPlatformHandle(void) const { return mPlatformHandle; }

private:

	void* mPlatformHandle;

};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// FxInterface (interface for dealing with FX materials)
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class FxInterface
{
public:

	void BeginFrame();

	virtual int BeginBlock( FxShader* hfx, const RenderContextInstData& data ) = 0;
	virtual bool BindPass( FxShader* hfx, int ipass ) = 0;
	virtual bool BindTechnique( FxShader* hfx, const FxShaderTechnique* htek ) = 0;
	virtual void EndPass( FxShader* hfx ) = 0;
	virtual void EndBlock( FxShader* hfx ) = 0;
	virtual void CommitParams( void ) = 0;

	virtual const FxShaderTechnique* GetTechnique( FxShader* hfx, const std::string & name ) = 0;
	virtual const FxShaderParam* GetParameterH( FxShader* hfx, const std::string & name ) = 0;

	virtual void BindParamBool( FxShader* hfx, const FxShaderParam* hpar, const bool bval ) = 0;
	virtual void BindParamInt( FxShader* hfx, const FxShaderParam* hpar, const int ival ) = 0;
	virtual void BindParamVect2( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec ) = 0;
	virtual void BindParamVect3( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec ) = 0;
	virtual void BindParamVect4( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec ) = 0;
	virtual void BindParamVect4Array( FxShader* hfx, const FxShaderParam* hpar, const CVector4 * Vec, const int icount ) = 0;
	virtual void BindParamFloatArray( FxShader* hfx, const FxShaderParam* hpar, const float * pfA, const int icnt ) = 0;
	virtual void BindParamFloat( FxShader* hfx, const FxShaderParam* hpar, float fA ) = 0;
	virtual void BindParamFloat2( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB ) = 0;
	virtual void BindParamFloat3( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC ) = 0;
	virtual void BindParamFloat4( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD ) = 0;
	virtual void BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 & Mat ) = 0;
	virtual void BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix3 & Mat ) = 0;
	virtual void BindParamMatrixArray( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 * MatArray, int iCount ) = 0;
	virtual void BindParamU32( FxShader* hfx, const FxShaderParam* hpar, U32 uval ) = 0;
	virtual void BindParamCTex( FxShader* hfx, const FxShaderParam* hpar, const Texture *pTex ) = 0;

	void BeginMaterialGroup( GfxMaterial* pmtl );
	void EndMaterialGroup();
	GfxMaterial* GetGroupCurMaterial() const { return mpGroupCurMaterial; }
	GfxMaterial* GetGroupMaterial() const { return mpGroupMaterial; }

	virtual bool LoadFxShader( const AssetPath& pth, FxShader *ptex ) = 0;

	void InvalidateStateBlock( void );

	GfxMaterial* GetLastFxMaterial( void ) const { return mpLastFxMaterial; }

	static void Reset();

protected:

	FxInterface();

	FxShader*							mpActiveFxShader;
	GfxMaterial *						mpLastFxMaterial;
	GfxMaterial *						mpGroupMaterial;
	GfxMaterial *						mpGroupCurMaterial;

private:

	virtual void DoBeginFrame() = 0;
	virtual void DoOnReset() {}


};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Immediate Mode Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class ImmInterface
{
public:

	inline CVtxBuffer<SVtxV4C4>&		RefUIQuadBuffer( void ) { return mVtxBufUIQuad; }
	inline CVtxBuffer<SVtxV12C4T16>&	RefUITexQuadBuffer( void ) { return mVtxBufUITexQuad; }
	inline CVtxBuffer<SVtxV12C4T16>&	RefTextVB( void ) { return mVtxBufText; }

protected:

	ImmInterface( GfxTarget& target );

	GfxTarget&							mTarget;
	DynamicVertexBuffer<SVtxV4C4>		mVtxBufUILine;
	DynamicVertexBuffer<SVtxV4C4>		mVtxBufUIQuad;
	DynamicVertexBuffer<SVtxV12C4T16>	mVtxBufUITexQuad;
	DynamicVertexBuffer<SVtxV12C4T16>	mVtxBufText;

private:

	virtual void DoBeginFrame() = 0;
	virtual void DoEndFrame() = 0;

};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Raster State Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class RasterStateInterface
{
public:

	RasterStateInterface();

	SRasterState &GetRasterState( void ) { return mCurrentState; }
	SRasterState & RefUIRasterState( void ) { return mUIRasterState; }
	virtual void BindRasterState( const SRasterState &rState, bool bForce = false ) = 0;

	SRasterState &GetLastState( void ) { return mLastState; }

	virtual void SetZWriteMask( bool bv ) = 0;
	virtual void SetRGBAWriteMask( bool rgb, bool a ) = 0;
	virtual void SetBlending( EBlending eVal ) = 0;
	virtual void SetDepthTest( EDepthTest eVal ) = 0;
	virtual void SetCullTest( ECullTest eVal ) = 0;
	virtual void SetScissorTest( EScissorTest eVal ) = 0;

protected:

	SRasterState						mUIRasterState;
	SRasterState						mCurrentState;
	SRasterState						mLastState;

};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Matrix Stack State Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class MatrixStackInterface
{
public:

	MatrixStackInterface( GfxTarget& target );

	void PushMMatrix( const CMatrix4 &rMat );
	void PushVMatrix( const CMatrix4 &rMat );
	void PushPMatrix( const CMatrix4 &rMat );
	void SetMMatrix( const CMatrix4 &rMat );

	void PopMMatrix( void );
	void PopVMatrix( void );
	void PopPMatrix( void );

	const CMatrix4& RefMMatrix( void ) const;
	const CMatrix4& RefR4Matrix( void ) const;
	const CMatrix3& RefR3Matrix( void ) const;
	const CMatrix4& RefVMatrix( void ) const;
	const CMatrix4& RefVITMatrix( void ) const;
	const CMatrix4& RefVITIYMatrix( void ) const;
	const CMatrix4& RefVITGMatrix( void ) const;
	const CMatrix4& RefPMatrix( void ) const;

	const CMatrix4& RefMVMatrix( void ) const;
	const CMatrix4& RefVPMatrix( void ) const;
	const CMatrix4& RefMVPMatrix( void ) const;

	void OnMMatrixDirty( void );
	void OnVMatrixDirty( void );
	void OnPMatrixDirty( void );

	int GetMStackDepth( void ) { return int( miMatrixStackIndexM ); }
	int GetVStackDepth( void ) { return int( miMatrixStackIndexV ); }
	int GetPStackDepth( void ) { return int( miMatrixStackIndexP ); }

	CMatrix4 &GetUIOrthoProjectionMatrix( void ) { return mUIOrthoProjectionMatrix; }

	virtual CMatrix4 Ortho( float left, float right, float top, float bottom, float fnear, float ffar ) = 0;
	virtual CMatrix4 Persp( float fovy, float aspect, float fnear, float ffar );
	virtual CMatrix4 Frustum( float left, float right, float top, float bottom, float zn, float zf );
	virtual CMatrix4 LookAt( const CVector3& eye, const CVector3& tgt, const CVector3& up ) const;

	const CMatrix4& GetOrthoMatrix( void ) const { return mMatOrtho; }
	void SetOrthoMatrix( const CMatrix4& mtx ) { mMatOrtho=mtx; }

	///////////////////////////////////////////////////////////////////////
	// these will probably get moved somewhere else

	const CMatrix4& GetShadowVMatrix( void ) const { return mShadowVMatrix; }
	const CMatrix4& GetShadowPMatrix( void ) const { return mShadowPMatrix; }

	void SetShadowVMatrix( const CMatrix4& vmat ) { mShadowVMatrix=vmat; }
	void SetShadowPMatrix( const CMatrix4& pmat ) { mShadowPMatrix=pmat; }

	///////////////////////////////////////////////////////////////////////

	const CVector4& GetScreenRightNormal( void ) { return mVectorScreenRightNormal; }
	const CVector4& GetScreenUpNormal( void ) { return mVectorScreenUpNormal; }

	///////////////////////////////////////////////////////////////////////

	void PushUIMatrix();
	void PushUIMatrix(int iw, int ih);
	void PopUIMatrix();

	///////////////////////////////////////////////////////////////////////

protected:

	static const int					kiMatrixStackMax = 16;

	int									miMatrixStackIndexM;
	int									miMatrixStackIndexV;
	int									miMatrixStackIndexP;
	int									miMatrixStackIndexUI;

	CMatrix4							maMatrixStackP[ kiMatrixStackMax ];
	CMatrix4							maMatrixStackM[ kiMatrixStackMax ];
	CMatrix4							maMatrixStackV[ kiMatrixStackMax ];
	CMatrix4							maMatrixStackUI[ kiMatrixStackMax ];

	CMatrix4							mMatrixVIT;
	CMatrix4							mMatrixVITIY;
	CMatrix4							mMatrixVITG;

	CMatrix4							mMatOrtho;

	CMatrix3							mmR3Matrix;
	CMatrix4							mmR4Matrix;
	CMatrix4							mmMVMatrix;
	CMatrix4							mmVPMatrix;
	CMatrix4							mmMVPMatrix;

	CMatrix4							mShadowVMatrix;
	CMatrix4							mShadowPMatrix;

	CMatrix4							mUIOrthoProjectionMatrix;

	CVector4							mVectorScreenRightNormal;
	CVector4							mVectorScreenUpNormal;

	GfxTarget&							mTarget;

};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Geometry Buffer Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class GeometryBufferInterface
{

public:

	void BeginFrame();
	void EndFrame();

	///////////////////////////////////////////////////////////////////////
	// VtxBuf Interface

	void FlushVB( VertexBufferBase& VBuf );

	//////////////////////////////////
	virtual void* LockVB( VertexBufferBase& VBuf, int ivbase=0, int icount=0 ) = 0;
	virtual void UnLockVB( VertexBufferBase& VBuf ) = 0;

	virtual const void* LockVB( const VertexBufferBase& VBuf, int ivbase=0, int icount=0 ) = 0;
	virtual void UnLockVB( const VertexBufferBase& VBuf ) = 0;

	virtual void ReleaseVB( VertexBufferBase& VBuf ) = 0; //e release memory

	virtual void DrawPrimitive( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) = 0;
	virtual void DrawIndexedPrimitive( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) = 0;
	virtual void DrawPrimitiveEML( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) = 0;
	virtual void DrawIndexedPrimitiveEML( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) = 0;

	virtual void* LockIB ( IndexBufferBase& VBuf, int ibase=0, int icount=0 ) = 0;
	virtual void UnLockIB ( IndexBufferBase& VBuf ) = 0;

	virtual const void* LockIB ( const IndexBufferBase& VBuf, int ibase=0, int icount=0 ) = 0;
	virtual void UnLockIB ( const IndexBufferBase& VBuf ) = 0;

	virtual void ReleaseIB( IndexBufferBase& VBuf ) = 0;

	void DrawPrimitive( const VtxWriterBase& VW, EPrimitiveType eType, int icount=0 );

	//////////////////////////////////////////////
	// DisplayList Interface

	virtual void DisplayListBegin( DisplayList& dlist ) {}
	virtual void DisplayListAddPrimitiveEML( DisplayList& dlist, const VertexBufferBase& VBuf ) {}
	virtual void DisplayListAddIndexedPrimitiveEML( DisplayList& dlist, const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE ) {}
	virtual void DisplayListEnd( DisplayList& dlist ) {}
	virtual void DisplayListDraw( const DisplayList& dlist ) {}
	virtual void DisplayListDrawEML( const DisplayList& dlist ) {}

	//////////////////////////////////////////////

	int GetNumTrianglesRendered( void ) { return miTrianglesRendered; }

protected:

	GeometryBufferInterface();
	~GeometryBufferInterface();

	int									miTrianglesRendered;

private:

	virtual void DoBeginFrame() {}
	virtual void DoEndFrame() {}

};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Frame/Buffer / Control Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class FrameBufferInterface
{
public:

	FrameBufferInterface( GfxTarget& mTarget );
	~FrameBufferInterface();

	Texture*		GetBufferTexture( void ) { return mpBufferTex; }
	void			SetBufferTexture( Texture* ptex ) { mpBufferTex=ptex; }
	void			SetClearColor( const CColor4 &scol ) { mcClearColor=scol; }
	const CColor4&	GetClearColor() const { return mcClearColor; }
	void			SetAutoClear( bool bv ) { mbAutoClear=bv; }
	bool			GetAutoClear() const { return mbAutoClear; }
	void			SetVSyncEnable( bool bv ) { mbEnableVSync=bv; }
	GfxBuffer*		GetThisBuffer( void ) { return mpThisBuffer; }
	void			SetThisBuffer( GfxBuffer* pbuf ) { mpThisBuffer=pbuf; }
	bool			IsOffscreenTarget( void ) { return mbIsPbuffer; }
	void			SetOffscreenTarget( bool bv ) { mbIsPbuffer=bv; }
	virtual void	SetRtGroup( RtGroup* Base ) = 0;
	RtGroup*		GetRtGroup() const { return mCurrentRtGroup; }

	void 			PushRtGroup( RtGroup* Base );
	void			PopRtGroup();


	///////////////////////////////////////////////////////

	virtual void	SetViewport( int iX, int iY, int iW, int iH ) = 0;
	virtual void	SetScissor( int iX, int iY, int iW, int iH ) = 0;
	virtual void	Clear( const CColor4 &rCol, float fdepth ) = 0;
	virtual void	PushViewport( const SRect &rViewportRect );
	virtual SRect&	PopViewport( void );
	SRect&			GetViewport( void );
	int				GetVPX( void ) { return miCurVPX; }
	int				GetVPY( void ) { return miCurVPY; }
	int				GetVPW( void ) { return miCurVPW; }
	int				GetVPH( void ) { return miCurVPH; }

	///////////////////////////////////////////////////////

	virtual void	PushScissor( const SRect &rScissorRect );
	virtual SRect&	PopScissor( void );
	inline SRect&	GetScissor( void );

	//////////////////////////////////////////////
	// Capture Interface

	virtual void	Capture( const RtGroup& inpbuf, int irt, const file::Path& pth ) {}
	//virtual void	Capture( GfxBuffer& inpbuf, const file::Path& pth ) {}
	//virtual void	Capture( GfxBuffer& inpbuf, CaptureBuffer& buffer ) {}
	virtual bool	CaptureToTexture( const CaptureBuffer& capbuf, Texture& tex ) { return false; }
	virtual void	GetPixel( const CVector4 &rAt, GetPixelContext& ctx ) = 0;

	//////////////////////////////////////////////

	void			BeginFrame( void );
	void			EndFrame( void );
	virtual void	ForceFlush( void ) {}
	virtual void	DoBeginFrame( void ) = 0;
	virtual void	DoEndFrame( void ) = 0;

	//////////////////////////////////////////////

	void			EnterPickState( PickBufferBase*pb ) { miPickState++; mpPickBuffer=pb; }
	bool			IsPickState( void ) { return (miPickState>0); }

	void			LeavePickState( void )
					{	miPickState--;
						OrkAssert( miPickState>=0 );
                        mpPickBuffer = 0;
					}

    PickBufferBase* GetCurrentPickBuffer() const { return mpPickBuffer; }

	//////////////////////////////////////////////

protected:

	static const int		kiVPStackMax = 16;

	int						miViewportStackIndex;
	int						miScissorStackIndex;
	SRect					maScissorStack[ kiVPStackMax ];
	SRect					maViewportStack[ kiVPStackMax ];

	GfxBuffer*				mpThisBuffer;
	Texture*				mpBufferTex;
	RtGroup*				mCurrentRtGroup;
	CColor4					mcClearColor;
	bool					mbAutoClear;
	bool					mbIsPbuffer;
	bool					mbEnableFullScreen;
	bool					mbEnableVSync;
	int						miCurVPX;
	int						miCurVPY;
	int						miCurVPW;
	int						miCurVPH;
	int						miPickState;
	GfxTarget&				mTarget;
    PickBufferBase*        mpPickBuffer;
	std::stack<lev2::RtGroup*> mRtGroupStack;


};

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Texture Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class TextureInterface
{
public:

	virtual void TexManInit( void ) = 0;

	virtual bool DestroyTexture( Texture *ptex ) = 0;
	virtual bool LoadTexture( const AssetPath& fname, Texture *ptex ) = 0;
	virtual void SaveTexture( const ork::AssetPath& fname, Texture *ptex ) = 0;
	virtual void UpdateAnimatedTexture( Texture *ptex, TextureAnimationInst* tai ) {}
	virtual void ApplySamplingMode( Texture *ptex ) {}

};

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
