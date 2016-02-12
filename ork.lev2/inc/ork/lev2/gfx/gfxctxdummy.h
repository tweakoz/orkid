////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2
{

///////////////////////////////////////////////////////////////////////////////

class DummyFxInterface : public FxInterface
{
public:

	virtual void DoBeginFrame() {}

	virtual int BeginBlock( FxShader* hfx, const RenderContextInstData& data ) final { return 0; }
	virtual bool BindPass( FxShader* hfx, int ipass ) final { return false; }
	virtual bool BindTechnique( FxShader* hfx, const FxShaderTechnique* htek ) final { return false; }
	virtual void EndPass( FxShader* hfx ) final {}
	virtual void EndBlock( FxShader* hfx ) final {}
	virtual void CommitParams( void ) final {}

	virtual const FxShaderTechnique* GetTechnique( FxShader* hfx, const std::string & name ) final { return 0; }
	virtual const FxShaderParam* GetParameterH( FxShader* hfx, const std::string & name ) final { return 0; }

	virtual void BindParamBool( FxShader* hfx, const FxShaderParam* hpar, const bool bval ) final {}
	virtual void BindParamInt( FxShader* hfx, const FxShaderParam* hpar, const int ival ) final {}
	virtual void BindParamVect2( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec ) final {}
	virtual void BindParamVect3( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec ) final {}
	virtual void BindParamVect4( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec ) final {}
	virtual void BindParamVect4Array( FxShader* hfx, const FxShaderParam* hpar, const CVector4 * Vec, const int icount ) final {}
	virtual void BindParamFloatArray( FxShader* hfx, const FxShaderParam* hpar, const float * pfA, const int icnt ) final {}
	virtual void BindParamFloat( FxShader* hfx, const FxShaderParam* hpar, float fA ) final {}
	virtual void BindParamFloat2( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB ) final {}
	virtual void BindParamFloat3( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC ) final {}
	virtual void BindParamFloat4( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD ) final {}
	virtual void BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 & Mat ) final {}
	virtual void BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix3 & Mat ) final {}
	virtual void BindParamMatrixArray( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 * MatArray, int iCount ) final {}
	virtual void BindParamU32( FxShader* hfx, const FxShaderParam* hpar, U32 uval ) final {}
	virtual void BindParamCTex( FxShader* hfx, const FxShaderParam* hpar, const Texture *pTex ) final {}
		
	virtual bool LoadFxShader( const AssetPath& pth, FxShader *ptex ) final;

	DummyFxInterface() {}
};

///////////////////////////////////////////////////////////////////////////////

class DuRasterStateInterface : public RasterStateInterface
{
	void BindRasterState( const SRasterState &rState, bool bForce = false ) final {}
	void SetZWriteMask( bool bv ) final {}
	void SetRGBAWriteMask( bool rgb, bool a ) final {}
	void SetBlending( EBlending eVal ) final {}
	void SetDepthTest( EDepthTest eVal ) final {}
	void SetCullTest( ECullTest eVal ) final {}
	void SetScissorTest( EScissorTest eVal ) final {}

public:
};

///////////////////////////////////////////////////////////////////////////////

class DuMatrixStackInterface : public MatrixStackInterface
{
	CMatrix4 Ortho( float left, float right, float top, float bottom, float fnear, float ffar ) final;
public:
	DuMatrixStackInterface( GfxTarget& target ) : MatrixStackInterface(target) {}
};

///////////////////////////////////////////////////////////////////////////////

class DuGeometryBufferInterface: public GeometryBufferInterface
{	
	///////////////////////////////////////////////////////////////////////
	// VtxBuf Interface

	virtual void* LockVB( VertexBufferBase& VBuf, int ivbase, int icount ) final;
	virtual void UnLockVB( VertexBufferBase& VBuf ) final;

	virtual const void* LockVB( const VertexBufferBase& VBuf, int ivbase=0, int icount=0 ) final;
	virtual void UnLockVB( const VertexBufferBase& VBuf ) final;

	virtual void ReleaseVB( VertexBufferBase& VBuf ) final;

	//

	virtual void*LockIB ( IndexBufferBase& VBuf, int ivbase, int icount ) final;
	virtual void UnLockIB ( IndexBufferBase& VBuf ) final;

	virtual const void* LockIB ( const IndexBufferBase& VBuf, int ibase=0, int icount=0 ) final;
	virtual void UnLockIB ( const IndexBufferBase& VBuf ) final;

	virtual void ReleaseIB( IndexBufferBase& VBuf ) final;

	//

	virtual void DrawPrimitive( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) final;
	virtual void DrawIndexedPrimitive( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) final;
	virtual void DrawPrimitiveEML( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) final;
	virtual void DrawIndexedPrimitiveEML( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 ) final;
	
	//////////////////////////////////////////////

public:

};

///////////////////////////////////////////////////////////////////////////////

class DuFrameBufferInterface : public FrameBufferInterface
{
public:

	DuFrameBufferInterface( GfxTarget& target );
	~DuFrameBufferInterface();

	virtual void	SetRtGroup( RtGroup* Base ) final {}

	///////////////////////////////////////////////////////

	virtual void	SetViewport( int iX, int iY, int iW, int iH ) final {}
	virtual void	SetScissor( int iX, int iY, int iW, int iH ) final {}
	virtual void	Clear( const CColor4 &rCol, float fdepth ) final {}

	virtual void	GetPixel( const CVector4 &rAt, GetPixelContext& ctx ) final {}

	//////////////////////////////////////////////

	virtual void	DoBeginFrame( void ) final {}
	virtual void	DoEndFrame( void ) final {}

protected:

};

///////////////////////////////////////////////////////////////////////////////

class DuTextureInterface : public TextureInterface
{
public:

	virtual void VRamUpload( Texture *pTex ) final {}		// Load Texture Data onto card
	virtual void VRamDeport( Texture *pTex ) final {}		// Load Texture Data onto card

	virtual void TexManInit( void ) final {}

	virtual bool DestroyTexture( Texture *ptex ) final { return false; }
	virtual bool LoadTexture( const AssetPath& fname, Texture *ptex ) final; 
	virtual void SaveTexture( const ork::AssetPath& fname, Texture *ptex ) final {}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class GfxTargetDummy : public GfxTarget
{
	RttiDeclareConcrete(GfxTargetDummy,GfxTarget);

	friend class GfxEnv;

	///////////////////////////////////////////////////////////////////////

	public:

	GfxTargetDummy();
	~GfxTargetDummy();

	///////////////////////////////////////////////////////////////////////
	// VtxBuf Interface

	//const void* VtxBuf_ReadLock( const VertexBufferBase& VBuf ) const;
	//void VtxBuf_ReadUnLock( const VertexBufferBase& VBuf ) const;

	/*virtual*/ bool SetDisplayMode(DisplayMode *mode) final;

	//////////////////////////////////////////////
	// FX Interface

	virtual FxInterface* FXI() final { return & mFxI; }
	virtual RasterStateInterface* RSI() final { return & mRsI; }
	virtual MatrixStackInterface* MTXI() final { return & mMtxI; }
	virtual GeometryBufferInterface* GBI() final { return & mGbI; }
	virtual TextureInterface* TXI() final { return & mTxI; }
	virtual FrameBufferInterface* FBI() final { return & mFbI; }

	//////////////////////////////////////////////

private:

	//////////////////////////////////////////////
	// CGfxHWContext Concrete Interface

	virtual void DoBeginFrame( void ) final {}
	virtual void DoEndFrame( void ) final {}
	virtual void InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase ) final;	// make a window
	virtual void InitializeContext( GfxBuffer *pBuf ) final;	// make a pbuffer
	virtual void resize( int iX, int iY, int iW, int iH ) final {}

	///////////////////////////////////////////////////////////////////////

	virtual void SetSize( int ix, int iy, int iw, int ih ) final;

	private:

	DummyFxInterface			mFxI;
	DuMatrixStackInterface		mMtxI;
	DuRasterStateInterface		mRsI;
	DuGeometryBufferInterface	mGbI;
	DuTextureInterface			mTxI;
	DuFrameBufferInterface		mFbI;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} }
