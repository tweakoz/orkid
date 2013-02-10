////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Win32DX Specific
///////////////////////////////////////////////////////////////////////////////

#ifndef _EXECENV_GFX_WIN32DX_H
#define _EXECENV_GFX_WIN32DX_H

#if defined(_DEBUG)
#define D3D_DEBUG_INFO
#endif


//#define DBG 1

#if defined( ORK_CONFIG_DIRECT3D )

// #define _USE_WACOM

#if defined(_WIN32)

#	define _DX9

#	if defined(_DX8)
#		include <d3d8.h>
#		include <d3dx8.h>
//#		include <windows.h>
#	elif defined(_DX9)
#		include <d3dx9.h>
#	endif
#if defined(_XBOX)
#include <Fxl.h>
#else 
#include <dxsdkver.h>
#endif

//#if (_DXSDK_BUILD_MAJOR>=892) // check for feb2007 sdk
//#define DXSDK_FEB2007
//#endif

//#include <wtl/mfc2wtl.h>

namespace dxt 
{
//  DDS_header.dwFlags
const unsigned long DDSD_CAPS			= 0x00000001;
const unsigned long DDSD_HEIGHT			= 0x00000002;
const unsigned long DDSD_WIDTH			= 0x00000004;
const unsigned long DDSD_PITCH			= 0x00000008;
const unsigned long DDSD_PIXELFORMAT	= 0x00001000;
const unsigned long DDSD_MIPMAPCOUNT	= 0x00020000;
const unsigned long DDSD_LINEARSIZE		= 0x00080000;
const unsigned long DDSD_DEPTH			= 0x00800000;
const unsigned long DDSD_RGB			= 0x00000040;
const unsigned long DDSD_RGBA			= 0x00000041;

const unsigned long DDS_MAGIC  = 0x20534444;
const unsigned long DDS_FOURCC = 0x00000004;

const unsigned long DDS_COMPLEX = 0x00000008;
const unsigned long DDS_CUBEMAP = 0x00000200;
const unsigned long DDS_VOLUME  = 0x00200000;

const unsigned long FOURCC_DXT1 = 0x31545844; //(MAKEFOURCC('D','X','T','1'))
const unsigned long FOURCC_DXT3 = 0x33545844; //(MAKEFOURCC('D','X','T','3'))
const unsigned long FOURCC_DXT5 = 0x35545844; //(MAKEFOURCC('D','X','T','5'))

struct DDS_PIXELFORMAT
{
    unsigned long dwSize;
    unsigned long dwFlags;
    unsigned long dwFourCC;
    unsigned long dwRGBBitCount;
    unsigned long dwRBitMask;
    unsigned long dwGBitMask;
    unsigned long dwBBitMask;
    unsigned long dwABitMask;

	void FixEndian();
};

struct DXTColBlock
{
    unsigned short col0;
    unsigned short col1;

    unsigned char row[4];
};

struct DXT3AlphaBlock
{
    unsigned short row[4];
};

struct DXT5AlphaBlock
{
    unsigned char alpha0;
    unsigned char alpha1;

    unsigned char row[6];
};

struct DDS_HEADER
{
	unsigned long dwMagic;
	unsigned long dwSize;
    unsigned long dwFlags;
    unsigned long dwHeight;
    unsigned long dwWidth;
    unsigned long dwPitchOrLinearSize;
    unsigned long dwDepth;
    unsigned long dwMipMapCount;
    unsigned long dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    unsigned long dwCaps1;
    unsigned long dwCaps2;
    unsigned long dwReserved2[3];

	void FixEndian();
};
}

#define MULTSAMPTYPE  D3DMULTISAMPLE_NONE //D3DMULTISAMPLE_4_SAMPLES
///////////////////////////////////////////////////////
// DX9

#if defined( _DX9 )
	typedef IDirect3DVertexBuffer9*			D3DVtxBufHandle;
	typedef LPDIRECT3DDEVICE9				D3DGfxDevice;
	typedef LPDIRECT3D9						D3DEnumerator;
	typedef IDirect3DPixelShader9*			D3DPixShaderH;
	typedef IDirect3DVertexShader9*			D3DVtxShaderH;
	typedef LPDIRECT3DSURFACE9				D3DGfxSurface;
	typedef IDirect3DVertexDeclaration9*	D3DVtxDecl;
	typedef IDirect3DBaseTexture9			D3DBaseTex;
#endif

#if defined(_XBOX)
static const DWORD D3dUsageDynamic = 0;
static const U32 D3dLockDiscard = 0;
#else
static const DWORD D3dUsageDynamic = D3DUSAGE_DYNAMIC;
static const U32 D3dLockDiscard = D3DLOCK_DISCARD;
#endif
static const U32 D3dUsageWriteOnly = 0; //D3DUSAGE_WRITEONLY
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

inline DWORD FtoDW( FLOAT f ) { return *((DWORD*)&f); }

struct SDXBufferFormat
{
	D3DFORMAT	mRGBAFormat;
	D3DFORMAT	mZSFormat;

	SDXBufferFormat( D3DFORMAT rgbafmt=D3DFMT_FORCE_DWORD, D3DFORMAT zsfmt=D3DFMT_FORCE_DWORD )
		: mRGBAFormat( rgbafmt )
		, mZSFormat( zsfmt )
	{
	}
};

///////////////////////////////////////////////////////////////////////////////

struct D3DXTexturePackage
{
	enum ETYPE
	{
		ETYPE_2D,
		ETYPE_3D,
		ETYPE_CUBEMAP,
		ETYPE_END
	};

	D3DXIMAGE_INFO				mImageInfo;
	void*						mpData;
	LPD3DXBUFFER				mD3DxBuffer;
	size_t						miDataLen;
	D3DSURFACE_DESC				mSurfaceDesc;
	ETYPE						meType;
	int							miDataFormat;
	IDirect3DBaseTexture9*		mD3dTexureHandle;
	const Texture*				mTexture;

	D3DXTexturePackage();
	~D3DXTexturePackage();
	bool IsResident() const { return (mD3dTexureHandle!=0); }
	void VRAM_Upload( D3DGfxDevice device, const Texture* ptex );
	void VRAM_Release();

	static D3DXTexturePackage* CreateUserPackage( ETYPE etyp, const Texture* ptex );
	static D3DXTexturePackage* CreateRenderTargetPackage( ETYPE etyp, const Texture* ptex, IDirect3DBaseTexture9* pdxtex );
};

///////////////////////////////////////////////////////////////////////////////

struct DxFboObject
{
	static const int kmaxrt = RtGroup::kmaxmrts;
	IDirect3DSurface9 *mFBO[kmaxrt];
	IDirect3DSurface9 *mFBOREAD[kmaxrt];
	IDirect3DTexture9 *mTEX[kmaxrt];
	IDirect3DSurface9 *mDSBO;
#ifdef _XBOX
	static const int kmaxtiles = 4;
	D3DRECT mStripeRects[kmaxtiles];
	int mNumStripes;
#endif
	DxFboObject();
};

///////////////////////////////////////////////////////////////////////////////

typedef FixedString<64> FxSmlString;
typedef FixedString<256> FxBigString;

struct FxLightTextureToSamplerMap
{
	orkvector<FxSmlString>	mSamplers;
};
struct FxShaderPackage
{
	static orkset<FxShaderPackage*>	gPackages;

	////////////////////////////////////////////////

#if defined( _XBOX )
	FXLEffect*										mpeffect;
	orkmap<FxSmlString,FxLightTextureToSamplerMap>	mtex2samplermap;
#else
	ID3DXEffect*	mpeffect;
#endif
	FxShader*		mpOrkShader;
	file::Path		mShaderPath;

	FxShaderPackage();
	static void OnDeviceReset(D3DGfxDevice d3ddev);
	static void Add(FxShaderPackage*);
	void Compile(D3DGfxDevice d3ddev);
};

#if defined( _XBOX )
struct FxShaderParamPackage
{
	static const int		kmaxsamplers=4;
	int						miNumSamplers;
	FXLHANDLE				mShaderParamHandle;
	FXLHANDLE				mSamplerParams[kmaxsamplers];
	FxShaderParamPackage() 
		: miNumSamplers(0)
		, mShaderParamHandle(0)
	{
		for( int i=0; i<kmaxsamplers; i++ ) mSamplerParams[i] = 0;
	}
	void AddSampler(FXLHANDLE h) 
	{
		OrkAssert( miNumSamplers<kmaxsamplers );
		mSamplerParams[miNumSamplers] = h;
		miNumSamplers++;
	}
};
#endif

///////////////////////////////////////////////////////////////////////////////

class GfxTargetDX;

class DxFxInterface : public FxInterface
{
public:

	virtual void DoBeginFrame() {}

	virtual int BeginBlock( FxShader* hfx, const RenderContextInstData& data );
	virtual bool BindPass( FxShader* hfx, int ipass );
	virtual bool BindTechnique( FxShader* hfx, const FxShaderTechnique* htek );
	virtual void EndPass( FxShader* hfx );
	virtual void EndBlock( FxShader* hfx );
	virtual void CommitParams( void );

	virtual const FxShaderTechnique* GetTechnique( FxShader* hfx, const std::string & name );
	virtual const FxShaderParam* GetParameterH( FxShader* hfx, const std::string & name );

	virtual void BindParamBool( FxShader* hfx, const FxShaderParam* hpar, const bool bval );
	virtual void BindParamInt( FxShader* hfx, const FxShaderParam* hpar, const int ival );
	virtual void BindParamVect2( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec );
	virtual void BindParamVect3( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec );
	virtual void BindParamVect4( FxShader* hfx, const FxShaderParam* hpar, const CVector4 & Vec );
	virtual void BindParamVect4Array( FxShader* hfx, const FxShaderParam* hpar, const CVector4 * Vec, const int icount );
	virtual void BindParamFloatArray( FxShader* hfx, const FxShaderParam* hpar, const float * pfA, const int icnt );
	virtual void BindParamFloat( FxShader* hfx, const FxShaderParam* hpar, float fA );
	virtual void BindParamFloat2( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB );
	virtual void BindParamFloat3( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC );
	virtual void BindParamFloat4( FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD );
	virtual void BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 & Mat );
	virtual void BindParamMatrix( FxShader* hfx, const FxShaderParam* hpar, const CMatrix3 & Mat );
	virtual void BindParamMatrixArray( FxShader* hfx, const FxShaderParam* hpar, const CMatrix4 * MatArray, int iCount );
	virtual void BindParamU32( FxShader* hfx, const FxShaderParam* hpar, U32 uval );
	virtual void BindParamCTex( FxShader* hfx, const FxShaderParam* hpar, const Texture *pTex );
	virtual bool LoadFxShader( const AssetPath& pth, FxShader *ptex );

	DxFxInterface( GfxTargetDX& glctx );

	virtual void DoOnReset();

protected:

	GfxTargetDX&						mTargetDX;

	static FxShaderPackage* LoadFxShader( FxShader* pfxshader, const AssetPath& fname );


};

///////////////////////////////////////////////////////////////////////////////

class DxImiInterface : public ImmInterface
{
	virtual void DrawLine( const CVector4 &From, const CVector4 &To );
	virtual void DrawPoint( F32 fx, F32 fy, F32 fz );
	virtual void DrawPrim( const CVector4 *Points, int inumpoints, EPrimitiveType eType );
	virtual void DoBeginFrame() {}
	virtual void DoEndFrame() {}

	D3DGfxDevice GetD3DDevice( void );

public:

	GfxTargetDX& mTargetDX;

	DxImiInterface( GfxTargetDX& target );
};

///////////////////////////////////////////////////////////////////////////////

class DxRasterStateInterface : public RasterStateInterface
{
	GfxTargetDX& mTargetDX;
	D3DGfxDevice GetD3DDevice( void );
	virtual void BindRasterState( const SRasterState &rState, bool bForce = false );
public:
	DxRasterStateInterface( GfxTargetDX& target );

	D3DZBUFFERTYPE	mLast_ZENABLE;
	D3DCMPFUNC		mLast_ZFUNC;

	bool			mLast_AlphaTestEnable;
	D3DCMPFUNC		mLast_AFUNC;
	unsigned int	mLast_ARef;

	bool			mLast_BlendEnable;
	D3DBLEND		mLast_SrcBlend;
	D3DBLEND		mLast_DstBlend;

};

///////////////////////////////////////////////////////////////////////////////

class DxMatrixStackInterface : public MatrixStackInterface
{
	virtual CMatrix4 Ortho( float left, float right, float top, float bottom, float fnear, float ffar );
	virtual CMatrix4 Frustum( float left, float right, float top, float bottom, float zn, float zf );
public:
	DxMatrixStackInterface( GfxTargetDX& target );
};

///////////////////////////////////////////////////////////////////////////////

class DxGeometryBufferInterface: public GeometryBufferInterface
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
	void BindVertexStreamSource( const VertexBufferBase& VBuf );

	virtual void DrawPrimitive( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );
	virtual void DrawIndexedPrimitive( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );
	virtual void DrawPrimitiveEML( const VertexBufferBase& VBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );
	virtual void DrawIndexedPrimitiveEML( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType=EPRIM_NONE, int ivbase = 0, int ivcount = 0 );

	//////////////////////////////////////////////
	// DisplayList Interface

	virtual void DisplayListBegin( DisplayList& dlist );
	virtual void DisplayListAddPrimitiveEML( DisplayList& dlist, const VertexBufferBase& VBuf );
	virtual void DisplayListAddIndexedPrimitiveEML( DisplayList& dlist, const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType );
	virtual void DisplayListEnd( DisplayList& dlist );
	virtual void DisplayListDraw( const DisplayList& dlist );
	virtual void DisplayListDrawEML( const DisplayList& dlist );

#if defined(_XBOX)
	void* BeginVertices( EPrimitiveType etype, int ivtxsize, int icount );
	void EndVertices();
#endif

	D3DGfxDevice GetD3DDevice( void );

	GfxTargetDX& mTargetDX;
	DynamicIndexBuffer<U16>			mImmIndexBuffer;
	CPerformanceItem				mDrawIdxPrimEMLItem;
	int								miNumDrawPrimCalls;
	int								miNumDrawTriangles;
	LPDIRECT3DVERTEXDECLARATION9	mpDXVtxDeclArray[EVTXSTREAMFMT_END];// = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	LPDIRECT3DVERTEXDECLARATION9	mLastVBDECL;

	void DoBeginFrame(); // virtual
	void DoEndFrame(); // virtual

public:

	void BindVertexDeclaration( EVtxStreamFormat efmt );

	DxGeometryBufferInterface( GfxTargetDX& target );

	int GetNumDrawPrimCalls() const { return miNumDrawPrimCalls; }
};

///////////////////////////////////////////////////////////////////////////////

class DxFrameBufferInterface : public FrameBufferInterface
{
public:

	DxFrameBufferInterface( GfxTargetDX& mTarget );
	~DxFrameBufferInterface();

	void InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase );
	void InitializeContext( GfxBuffer *pBuf );
	void DeviceReset( int inx, int iny, int inw, int inh );

private:

	virtual void	SetRtGroup( RtGroup* Base );
#if defined(_XBOX)
	void			BeginMrt( RtGroup* Base );
	void			EndMrt();
#endif

	///////////////////////////////////////////////////////

	virtual void	SetViewport( int iX, int iY, int iW, int iH );
	virtual void	SetScissor( int iX, int iY, int iW, int iH );
	virtual void	AttachViewport( CUIViewport *pVP = 0 );
	virtual void	ClearViewport( CUIViewport *pVP );

	//////////////////////////////////////////////
	// Capture Interface

	virtual void	Capture( const file::Path& pth );
	virtual void	Capture( CaptureBuffer& buffer );
	virtual bool	CaptureToTexture( const CaptureBuffer& capbuf, Texture& tex );
	virtual void	GetPixel( const CVector4 &rAt, GetPixelContext& ctx );

	//////////////////////////////////////////////

	virtual void	DoBeginFrame( void );
	virtual void	DoEndFrame( void );

	//////////////////////////////////////////////

	D3DGfxDevice GetD3DDevice( void );
	void SetAsRenderTarget( void );

	GfxTargetDX&			mTargetDX;

#if ! defined(_XBOX)
	IDirect3DSwapChain9*	mpSwapChain;
#endif

	D3DGfxSurface			mpDefaultRenderTarget;				

	IDirect3DTexture9*		mpRenderTexture;
	IDirect3DSurface9*		mpDepthBuffer;
	D3DGfxSurface			m_pBackBuffer;
	D3DFORMAT				mFORMAT;
	int						miNewW, miNewH;
	int						miOldW, miOldH;

};

class DxTextureInterface : public TextureInterface
{
public:

	virtual void VRamUpload( Texture *pTex );		// Load Texture Data onto card
	virtual void VRamDeport( Texture *pTex );		// Load Texture Data onto card

	virtual void TexManInit( void );

	virtual bool DestroyTexture( Texture *ptex );
	virtual bool LoadTexture( const AssetPath& fname, Texture *ptex );
	virtual void SaveTexture( const ork::AssetPath& fname, Texture *ptex );

	D3DGfxDevice GetD3DDevice( void );

	DxTextureInterface( GfxTargetDX& target );

private:

	GfxTargetDX& mTargetDX;

};

///////////////////////////////////////////////////////////////////////////////

struct DisplayModeDX : DisplayMode
{
	DisplayModeDX(unsigned int w = 0, unsigned int h = 0, unsigned int r = 0,
		D3DFORMAT f = D3DFMT_UNKNOWN, unsigned int o = 0) : DisplayMode(w, h, r), format(f), ordinal(o) {}

	D3DFORMAT format;
	unsigned int ordinal;
};

class GfxTargetDX : public GfxTarget
{
	RttiDeclareConcrete(GfxTargetDX,GfxTarget);
	friend class GfxEnv;

	///////////////////////////////////////////////////////////////////////

	public:

	GfxTargetDX( /*const CClass* pclass*/ );
	~GfxTargetDX();

	void FxInit();

	///////////////////////////////////////////////////////////////////////

	virtual void SetSize( int ix, int iy, int iw, int ih );

	///////////////////////////////////////////////////////////////////////

	virtual U32 CColor4ToU32( const CColor4& clr ) { return clr.GetARGBU32(); }
	virtual U32 CColor3ToU32( const CColor3& clr ) { return clr.GetARGBU32(); }
	virtual CColor4 U32ToCColor4( const U32 uclr ) { CColor4 clr; clr.SetRGBAU32(uclr); return clr; }
	virtual CColor3 U32ToCColor3( const U32 uclr ) { CColor3 clr; clr.SetRGBAU32(uclr); return clr; }

	//////////////////////////////////////////////
	// Interfaces

	virtual FxInterface*				FXI() { return & mFxI; }
	virtual ImmInterface*				IMI() { return & mImI; }
	virtual RasterStateInterface*		RSI() { return & mRsI; }
	virtual MatrixStackInterface*		MTXI() { return & mMtxI; }
	virtual GeometryBufferInterface*	GBI() { return & mGbI; }
	virtual FrameBufferInterface*		FBI() { return & mFbI; }
	virtual TextureInterface*			TXI() { return & mTxI; }

	//////////////////////////////////////////////
	// GfxTarget Concrete Interface

	virtual void DoBeginFrame( void ) {}
	virtual void DoEndFrame( void ) {}

	virtual void InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase );	// make a window
	virtual void InitializeContext( GfxBuffer *pBuf );	// make a pbuffer

	//////////////////////////////////////////////
	// DX Specific

	virtual void InitVB( void );

	/*virtual*/ bool SetDisplayMode(DisplayMode *mode);

	HWND GetHWND( void )	{ return 0;	}
	HWND GetMainWindow() { return m_MainWindow; }

	static D3DGfxDevice GetD3DDevice( void )
	{	OrkAssert( 0 != s_pd3dDevice );
		return s_pd3dDevice;
	}

	D3DPRESENT_PARAMETERS& RefPresentationParams( int idx=0 )
	{	OrkAssert( idx<s_iNumAdapters );
		return mPresentations[idx];
	}

	bool InitResize( void )
	{	bool brval = mbInitResize;
		mbInitResize=false;
		return brval;
	}

///////////////////////////////////////////////////////////////////////////

	DxFrameBufferInterface& DXFBI() { return mFbI; }
	DxGeometryBufferInterface& DXGBI() { return mGbI; }

	static D3DPRESENT_PARAMETERS* GetPresentations() { return s_Presentations; }

///////////////////////////////////////////////////////////////////////////

	static void HandleKeypress( int ich, bool bOn );

	protected:

	static D3DPRESENT_PARAMETERS	s_d3dppInitial;
	static D3DEnumerator			s_pD3D;
	static D3DGfxDevice				s_pd3dDevice;
	static orkvector<SDXBufferFormat>	s_FrameBufferFormats;
	static orkvector<SDXBufferFormat>	s_PBufferFormats;
	static int						s_iNumAdapters;
	static D3DPRESENT_PARAMETERS*	s_pd3dpp;
	static D3DDEVTYPE				s_devicetype;
	static D3DCAPS9					s_caps;
	static orkvector<GfxTargetDX*>	s_windows;
	static DWORD					s_windowstyle;
#if ! defined(_XBOX)
	static WNDCLASSEX				s_windowclass;
#endif
	static int						s_NumTexCoords;
	static int						s_NumTexUnits;
	static int						s_NumTexSamplers;
	static int						s_PixShaderSupport;
	static D3DPRESENT_PARAMETERS*	s_Presentations;
	static bool						s_SoftwareVertexProcessing;

	static void InitializeDevice();

	Texture*						mLastTexH[4];
	int								miLastTexSampState;
	D3DPRESENT_PARAMETERS*			mPresentations;

	HWND					m_MainWindow;

#if ! defined(_XBOX)
	WNDCLASSEX				m_MainWindowClass;
#endif

	D3DVtxShaderH			mVtxShaderV16C4;
	D3DPixShaderH			mPixShaderV16C4;

	DWORD					muCurVertexData;

	bool					mbResetDevice;

	bool					mbInitResize;

	///////////////////////////////////////////////////////////////////////////
	// Rendering State Info

	EDepthTest		meCurDepthTest;

	///////////////////////////////////////////////////////////////////////////

	DxFxInterface				mFxI;
	DxImiInterface				mImI;
	DxRasterStateInterface		mRsI;
	DxMatrixStackInterface		mMtxI;
	DxGeometryBufferInterface	mGbI;
	DxFrameBufferInterface		mFbI;
	DxTextureInterface			mTxI;

};

} }

#endif // _WIN32

#endif // ORK_CONFIG_DIRECT3D

#endif // _EXECENV_GFX_WIN32GL_H
