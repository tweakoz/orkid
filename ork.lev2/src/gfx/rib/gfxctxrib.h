////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _EXECENV_GFX_DUMMY_H
#define _EXECENV_GFX_DUMMY_H

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2
{

///////////////////////////////////////////////////////////////////////////////

class RibFxInterface : public FxInterface
{
public:

	virtual void DoBeginFrame() {}

	virtual int BeginBlock( FxShader* hfx, const RenderContextInstData& data ) { return 0; }
	virtual bool BindPass( FxShader* hfx, int ipass ) { return false; }
	virtual bool BindTechnique( FxShader* hfx, const FxShaderTechnique* htek ) { return false; }
	virtual void EndPass( FxShader* hfx ) {}
	virtual void EndBlock( FxShader* hfx ) {}
	virtual void CommitParams( void ) {}

	virtual const FxShaderTechnique* GetTechnique( FxShader* hfx, const std::string & name ) { return 0; }
	virtual const FxShaderParam* GetParameterH( FxShader* hfx, const std::string & name ) { return 0; }

	virtual void BindParamBool( FxShader* hfx, const FxShaderParam* hpar, const bool bval ) {}
	virtual void BindParamInt( FxShader* hfx, const FxShaderParam* hpar, const int ival ) {}
	virtual void BindParamVect2( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec ) {}
	virtual void BindParamVect3( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec ) {}
	virtual void BindParamVect4( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec ) {}
	virtual void BindParamVect4Array( FxShader* hfx, const FxShaderParam* hpar, const CVector4 * Vec, const int icount ) {}
	virtual void BindParamFloatArray( FxShader* hfx, const FxShaderParam* hpar, const float * pfA, const int icnt ) {}
	virtual void BindParamFloat( FxShader* hfx, const FxShaderParam* hpar, float fA ) {}
	virtual void BindParamFloat2( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB ) {}
	virtual void BindParamFloat3( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC ) {}
	virtual void BindParamFloat4( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD ) {}
	virtual void BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 & Mat ) {}
	virtual void BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix3 & Mat ) {}
	virtual void BindParamMatrixArray( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 * MatArray, int iCount ) {}
	virtual void BindParamU32( FxShader* hfx, const FxShaderParam* hpar, U32 uval ) {}
	virtual void BindParamCTex( FxShader* hfx, const FxShaderParam* hpar, const Texture *pTex ) {}
		
	virtual bool LoadFxShader( const AssetPath& pth, FxShader *ptex );

	RibFxInterface() {}
};

///////////////////////////////////////////////////////////////////////////////

class RibRasterStateInterface : public RasterStateInterface
{
	virtual void BindRasterState( const SRasterState &rState, bool bForce = false ) {}
public:
};

///////////////////////////////////////////////////////////////////////////////

class RibMatrixStackInterface : public MatrixStackInterface
{
	virtual CMatrix4 Ortho( float left, float right, float top, float bottom, float fnear, float ffar );
public:
	RibMatrixStackInterface( GfxTarget& target ) : MatrixStackInterface(target) {}
};

///////////////////////////////////////////////////////////////////////////////

class RibGeometryBufferInterface: public GeometryBufferInterface
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

	virtual void DrawPrimitive( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );
	virtual void DrawIndexedPrimitive( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );
	virtual void DrawPrimitiveEML( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );
	virtual void DrawIndexedPrimitiveEML( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );
	
	//////////////////////////////////////////////

public:

};

///////////////////////////////////////////////////////////////////////////////

class RibFrameBufferInterface : public FrameBufferInterface
{
public:

	RibFrameBufferInterface( GfxTarget& target );
	~RibFrameBufferInterface();

	virtual void	SetRtGroup( RtGroup* Base ) {}

	///////////////////////////////////////////////////////

	virtual void	SetViewport( int iX, int iY, int iW, int iH ) {}
	virtual void	SetScissor( int iX, int iY, int iW, int iH ) {}
	virtual void	AttachViewport( CUIViewport *pVP = 0 ) {}
	virtual void	ClearViewport( CUIViewport *pVP ) {}

	virtual void	GetPixel( const CVector4 &rAt, GetPixelContext& ctx ) {}

	//////////////////////////////////////////////

	virtual void	DoBeginFrame( void ) {}
	virtual void	DoEndFrame( void ) {}

protected:

};

///////////////////////////////////////////////////////////////////////////////

class RibTextureInterface : public TextureInterface
{
public:

	virtual void VRamUpload( Texture *pTex ) {}		// Load Texture Data onto card
	virtual void VRamDeport( Texture *pTex ) {}		// Load Texture Data onto card

	virtual void TexManInit( void ) {}

	virtual bool DestroyTexture( Texture *ptex ) { return false; }
	virtual bool LoadTexture( const AssetPath& fname, Texture *ptex ); 
	virtual void SaveTexture( const ork::AssetPath& fname, Texture *ptex ) {}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class GfxTargetRib : public GfxTarget
{
	RttiDeclareConcrete(GfxTargetRib,GfxTarget);

	friend class GfxEnv;

	///////////////////////////////////////////////////////////////////////

	public:

	GfxTargetRib();
	~GfxTargetRib();

	///////////////////////////////////////////////////////////////////////
	// VtxBuf Interface

	//const void* VtxBuf_ReadLock( const VertexBufferBase& VBuf ) const;
	//void VtxBuf_ReadUnLock( const VertexBufferBase& VBuf ) const;

	/*virtual*/ bool SetDisplayMode(DisplayMode *mode);

	//////////////////////////////////////////////
	// FX Interface

	virtual FxInterface* FXI() { return & mFxI; }
	virtual RasterStateInterface* RSI() { return & mRsI; }
	virtual MatrixStackInterface* MTXI() { return & mMtxI; }
	virtual GeometryBufferInterface* GBI() { return & mGbI; }
	virtual TextureInterface* TXI() { return & mTxI; }
	virtual FrameBufferInterface* FBI() { return & mFbI; }

	//////////////////////////////////////////////

private:

	//////////////////////////////////////////////
	// CGfxHWContext Concrete Interface

	virtual void DoBeginFrame( void ) {}
	virtual void DoEndFrame( void ) {}
	virtual void InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase );	// make a window
	virtual void InitializeContext( GfxBuffer *pBuf );	// make a pbuffer
	virtual void resize( int iX, int iY, int iW, int iH ) {}

	///////////////////////////////////////////////////////////////////////

	virtual void SetSize( int ix, int iy, int iw, int ih );

	private:

	RibFxInterface			mFxI;
	RibMatrixStackInterface		mMtxI;
	RibRasterStateInterface		mRsI;
	RibGeometryBufferInterface	mGbI;
	RibTextureInterface			mTxI;
	RibFrameBufferInterface		mFbI;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} }

#endif // _EXECENV_GFX_WIN32GL_H
