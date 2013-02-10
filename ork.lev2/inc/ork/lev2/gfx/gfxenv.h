////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Grpahics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#ifndef _EXECENV_GFX_GFXENV_H
#define _EXECENV_GFX_GFXENV_H

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/timer.h>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/ui/ui_enum.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/kernel/mutex.h>
#include <ork/lev2/gfx/gfxenv_targetinterfaces.h>
#include <ork/event/Event.h>
#include <ork/object/AutoConnector.h>

///////////////////////////////////////////////////////////////////////////////
#if defined( IX ) || defined( _WIN32 )
#define HAVE_IL
#endif
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

typedef SVtxV12C4T16		TEXT_VTXFMT;

class CTXBASE;

class GfxTarget;
class GfxBuffer;
class GfxWindow;
class RtGroup;

class Texture;
class GfxMaterial;
class GfxMaterialUITextured;

class GfxEnv;

class CUIEvent;
class CUIViewport;

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

struct GfxTargetCreationParams
{
	GfxTargetCreationParams()
		: miNumSharedVerts(0)
		, mbFullScreen(false)
		, mbWideScreen(false)
		, miDefaultWidth(640)
		, miDefaultHeight(480)
		, miDefaultMrtWidth(640)
		, miDefaultMrtHeight(480)
		, miQuality(100)
#if defined(_WIN32) && ! defined(_XBOX)
		, mHWND(0)
#endif
	{
	}

	int		miNumSharedVerts;
	bool	mbFullScreen;
	bool	mbWideScreen;
	int		miDefaultWidth;
	int		miDefaultHeight;
	int		miDefaultMrtWidth;
	int		miDefaultMrtHeight;
	int		miQuality;
#if defined(_WIN32) && ! defined(_XBOX)
	HWND	mHWND;
#endif
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

struct RenderQueueSortingData
{
	RenderQueueSortingData();

	int			miSortingPass;
	int			miSortingOffset;
	bool		mbTransparency;
};

class RenderContextInstData;
class RenderContextFrameData;

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

struct DisplayMode
{
	DisplayMode(unsigned int w = 0, unsigned int h = 0, unsigned int r = 0)
		: width(w)
		, height(h)
		, refresh(r)
	{}

	unsigned int width;
	unsigned int height;
	unsigned int refresh;
};

///////////////////////////////////////////////////////////////////////////////

class GfxTarget : public ork::rtti::ICastable
{
	RttiDeclareAbstract(GfxTarget,ork::rtti::ICastable);

	///////////////////////////////////////////////////////////////////////
	public:
	///////////////////////////////////////////////////////////////////////

	GfxTarget();
	virtual ~GfxTarget();

	//////////////////////////////////////////////
	// Interfaces
	
	virtual FxInterface*				FXI() { return 0; }	// Fx Shader Interface (optional)
	virtual ImmInterface*				IMI() { return 0; }	// Immediate Mode Interface (optional)
	virtual RasterStateInterface*		RSI() = 0;			// Raster State Interface
	virtual MatrixStackInterface*		MTXI() = 0;			// Matrix / Matrix Stack Interface
	virtual GeometryBufferInterface*	GBI() = 0;			// Geometry Buffer Interface
	virtual FrameBufferInterface*		FBI() = 0;			// FrameBuffer/Control Interface
	virtual TextureInterface*			TXI() = 0;			// Texture Interface

	///////////////////////////////////////////////////////////////////////

	virtual void		InitializeContext( GfxWindow *pWin, CTXBASE* pctxbase ) = 0;
	virtual void		InitializeContext( GfxBuffer *pBuf ) = 0;

	///////////////////////////////////////////////////////////////////////

	int					GetX( void ) const { return miX; }
	int					GetY( void ) const { return miY; }
	int					GetW( void ) const { return miW; }
	int					GetH( void ) const { return miH; }

	virtual void		SetSize( int ix, int iy, int iw, int ih ) = 0;
	
	//////////////////////////////////////////////

	void				BeginFrame( void );
	void				EndFrame( void );

	//////////////////////////////////////////////

	GfxMaterial*		GetCurMaterial( void ) { return mpCurMaterial; }
	void				BindMaterial( GfxMaterial* pmtl );
	void				PushMaterial( GfxMaterial* pmtl );
	void				PopMaterial();

	///////////////////////////////////////////////////////////////////////

	virtual U32			CColor4ToU32( const CColor4& clr ) { return clr.GetRGBAU32(); }
	virtual U32			CColor3ToU32( const CColor3& clr ) { return clr.GetRGBAU32(); }
	virtual CColor4		U32ToCColor4( const U32 uclr ) { CColor4 clr; clr.SetRGBAU32(uclr); return clr; }
	virtual CColor3		U32ToCColor3( const U32 uclr ) { CColor3 clr; clr.SetRGBAU32(uclr); return clr; }

	///////////////////////////////////////////////////////////////////////

	CVector4&			RefModColor( void ) { return mvModColor; }
	void				PushModColor( const CVector4 & mclr );
	CVector4&			PopModColor( void );

	///////////////////////////////////////////////////////////////////////

	const ork::rtti::ICastable* GetCurrentObject( void ) const { return mpCurrentObject; }
	void						SetCurrentObject( const ork::rtti::ICastable *pobj ) { mpCurrentObject=pobj; }
	ETargetType					GetTargetType( void ) const { return meTargetType; }
	int							GetTargetFrame( void ) const { return miTargetFrame; }
	CPerformanceItem&			GetFramePerfItem( void ) { return mFramePerfItem; }
	CTXBASE*					GetCtxBase( void ) const { return mCtxBase; }

	///////////////////////////////////////////////////////

	const RenderContextInstData*	GetRenderContextInstData() const { return mRenderContextInstData; }
	void							SetRenderContextInstData(const RenderContextInstData*data) { mRenderContextInstData=data; }
	const RenderContextFrameData*	GetRenderContextFrameData() const { return mRenderContextFrameData; }
	void							SetRenderContextFrameData(const RenderContextFrameData*data) { mRenderContextFrameData=data; }

	//////////////////////////////////////////////

	#if defined(_WIN32) && (!(defined(_XBOX)))
	virtual HWND GetHWND( void ) { return 0; }
	#endif

	//////////////////////////////////////////////

	bool IsDeviceAvailable() const { return mbDeviceAvailable; }
	void SetDeviceAvailable( bool bv ) { mbDeviceAvailable=bv; }

	static const orkvector<DisplayMode *> &GetDisplayModes() { return mDisplayModes; }

	bool SetDisplayMode(unsigned int index);
	virtual bool SetDisplayMode(DisplayMode *mode) = 0;

	void* GetPlatformHandle() const { return mPlatformHandle; }
	void SetPlatformHandle(void*ph) { mPlatformHandle=ph; }
	
	virtual void TakeThreadOwnership() {}
	
	void* BeginLoad();
	void EndLoad(void*ploadtok);

protected:

	static const int					kiModColorStackMax = 8;
	int									miX, miY, miW, miH;
	CTXBASE*							mCtxBase;
	void*								mPlatformHandle;
	ETargetType							meTargetType;
	CVector4							maModColorStack[kiModColorStackMax];
	int									miModColorStackIndex;
	const ork::rtti::ICastable*			mpCurrentObject;
	CVector4							mvModColor;
	bool								mbPostInitializeContext;
	int									miTargetFrame;	
	CPerformanceItem					mFramePerfItem;
	const RenderContextInstData*		mRenderContextInstData;
	const RenderContextFrameData*		mRenderContextFrameData;
	GfxMaterial*						mpCurMaterial;
	std::stack<GfxMaterial*>			mMaterialStack;
	bool								mbDeviceAvailable;
	int									miDrawLock;

	static orkvector<DisplayMode *>		mDisplayModes;
private:

	virtual void DoBeginFrame( void ) = 0;
	virtual void DoEndFrame( void ) = 0;
	virtual void* DoBeginLoad(){return nullptr;}
	virtual void DoEndLoad(void*ploadtok){}
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

struct OrthoQuad
{
	OrthoQuad();

	ork::CColor4 mColor;
	SRect mQrect;
	float mfu0a;
	float mfv0a;
	float mfu1a;
	float mfv1a;
	float mfu0b;
	float mfv0b;
	float mfu1b;
	float mfv1b;
	float mfrot;
};

class GfxBuffer : public ork::Object
{
	RttiDeclareAbstract(GfxBuffer,ork::Object);

	public:

	//////////////////////////////////////////////

	GfxBuffer(	GfxBuffer *Parent,
				int iX, int iY, int iW, int iH,
				EBufferFormat efmt = EBUFFMT_RGBA32,
				ETargetType etype = ETGTTYPE_EXTBUFFER,
				const std::string & name = "NoName" );

	virtual ~GfxBuffer();

	//////////////////////////////////////////////

	struct OrthoQuads
	{
		SRect mViewportRect;
		SRect mOrthoRect;
		GfxMaterial* mpMaterial;
		const OrthoQuad* mpQUADS;
		int miNumQuads;
	};

	//////////////////////////////////////////////

	RtGroup* GetParentMrt( void ) const { return mParentRtGroup; }
	CUIViewport* GetViewport( void ) const { return mpViewport; }
	bool IsDirty( void ) const { return mbDirty; }
	bool IsSizeDirty( void ) const { return mbSizeIsDirty; }
	const std::string & GetName( void ) const { return msName; }
	const CColor4& GetClearColor() const { return mClearColor; }
	GfxBuffer* GetParent( void ) const { return mParent; }
	ETargetType GetTargetType( void ) const { return meTargetType; }
	EBufferFormat GetBufferFormat( void ) const { return meFormat; }
	Texture* GetTexture() const { return mpTexture; }
	GfxMaterial* GetMaterial() const { return mpMaterial; }
	GfxTarget * GetContext( void ) const;

	int GetContextX( void ) const  { return GetContext()->GetX(); }
	int GetContextY( void ) const { return GetContext()->GetY(); }
	int GetContextW( void ) const { return GetContext()->GetW(); }
	int GetContextH( void ) const { return GetContext()->GetH(); }
	
	int GetBufferW( void ) const { return miWidth; }
	int GetBufferH( void ) const { return miHeight; }
	void SetBufferWidth( int iw ) { mbSizeIsDirty=(miWidth!=iw); miWidth=iw; }
	void SetBufferHeight( int ih ) { mbSizeIsDirty=(miHeight!=ih); miHeight=ih; }

	//////////////////////////////////////////////

	void Resize( int ix, int iy, int iw, int ih );
	void SetDirty( bool bval ) { mbDirty=bval; }
	void SetSizeDirty( bool bv ) { mbSizeIsDirty=bv; }
	void SetParentMrt( RtGroup* ParentMrt ) { mParentRtGroup=ParentMrt; }
	CColor4& RefClearColor() { return mClearColor; }
	void SetContext( GfxTarget* pctx ) { mpContext=pctx; }
	void SetTexture( Texture* ptex ) { mpTexture=ptex; }
	void SetMaterial( GfxMaterial* pmtl ) { mpMaterial=pmtl; }

	//////////////////////////////////////////////

	void RenderMatOrthoQuad(	const SRect& ViewportRect,
								const SRect& QuadRect,
								GfxMaterial *pmat, 
								float fu0 = 0.0f,
								float fv0 = 0.0f,
								float fu1 = 1.0f,
								float fv1 = 1.0f,
								float *uv2=NULL,
								const CColor4& clr = CColor4::White() );

	void RenderMatOrthoQuads( const OrthoQuads& oquads );

	//////////////////////////////////////////////

	virtual void BeginFrame( void );
	virtual void EndFrame( void );
	virtual void CreateContext();

protected:

	CUIViewport*		mpViewport;
	GfxTarget*			mpContext;
	GfxMaterial*		mpMaterial;
	Texture*			mpTexture;
	int					miWidth;
	int					miHeight;
	EBufferFormat		meFormat;
	ETargetType			meTargetType;
	bool				mbDirty;
	bool				mbSizeIsDirty;
	std::string			msName;
	CColor4				mClearColor;
	GfxBuffer*			mParent;
	RtGroup*			mParentRtGroup;
    void*              	mPlatformHandle;

};

///////////////////////////////////////////////////////////////////////////

class PickBufferBase : public ork::lev2::GfxBuffer
{
	RttiDeclareAbstract(PickBufferBase,ork::lev2::GfxBuffer);

	public:

	enum EPickBufferType
	{
		EPICK_FACE_VTX = 0,
		EPICK_NORMAL ,
		EPICK_WPOS ,
		EPICK_ST
	};

	PickBufferBase( GfxBuffer *parent,
					 int iX, int iY, int iW, int iH,
					 EPickBufferType etyp );

	void Init();
	
    uint32_t        AssignPickId(ork::Object*);
    ork::Object*    GetObjectFromPickId(uint32_t);

	virtual void Draw( void ) = 0;

	///////////////////////

	EPickBufferType				meType;
	bool						mbInitTex;
	GfxMaterialUITextured*		mpUIMaterial;
    std::map<uint32_t,ork::Object*>	mPickIds;
	ork::lev2::RtGroup*			mpPickRtGroup;

};

///////////////////////////////////////////////////////////////////////////

template <typename VPT> class CPickBuffer : public PickBufferBase
{
	public:

	CPickBuffer(	lev2::GfxBuffer* pbuf,
					VPT* pVP,
					int iX, int iY, int iW, int iH,
					EPickBufferType etyp );
	

	virtual void Draw( void );
	
	VPT* mpViewport;
};

///////////////////////////////////////////////////////////////////////////////

template <typename TLev2Viewport>
CPickBuffer<TLev2Viewport>::CPickBuffer(	lev2::GfxBuffer *Parent,
											TLev2Viewport *pVP,
											int iX, int iY, int iW, int iH,
											EPickBufferType etyp )

	: PickBufferBase( Parent, iX, iY, iW, iH, etyp )
	, mpViewport( pVP )
{
	Init();
}

/// ////////////////////////////////////////////////////////////////////////////
/// Graphics Context Base
/// this abstraction allows us to switch UI toolkits (Qt/Fltk, etc...)
/// ////////////////////////////////////////////////////////////////////////////

#if defined( _WIN32 )
 typedef HWND CTFLXID;
#elif defined( _OSX )
 typedef WindowPtr CTFLXID;
#elif defined (IX)
 typedef void* CTFLXID;
#elif defined (WII)
 typedef unsigned long int CTFLXID;
#endif

 class CTXBASE : public ork::AutoConnector
{
	RttiDeclareAbstract( CTXBASE, ork::AutoConnector );

	DeclarePublicAutoSlot( Repaint );

public:

	enum ERefreshPolicy
	{	
		EREFRESH_FASTEST = 0,	// refresh as fast as the update loop can go
		EREFRESH_WHENDIRTY,		// refresh whenever dirty 
		EREFRESH_FIXEDFPS,		// refresh at a fixed frame rate
	};


	CTFLXID GetThisXID( void ) const { return mxidThis; }
	CTFLXID GetTopXID( void ) const { return mxidTopLevel; }
	void SetThisXID( CTFLXID xid ) { mxidThis=xid; }
	void SetTopXID( CTFLXID xid ) { mxidTopLevel=xid; }
	ERefreshPolicy GetRefreshPolicy() const { return meRefreshPolicy; }

	CTXBASE( GfxWindow* pwin );

	virtual void SlotRepaint( void ) {}
	virtual void SetRefreshRate( int ihz ) {}
	virtual void SetRefreshPolicy( ERefreshPolicy epolicy ) { meRefreshPolicy=epolicy; }

	virtual void Show() {}
	virtual void Hide() {}

	GfxTarget* GetTarget() const { return mpTarget; }
	GfxWindow* GetWindow() const { return mpGfxWindow; }
	void SetTarget(GfxTarget*pt) { mpTarget=pt; }
	void SetWindow(GfxWindow*pw) { mpGfxWindow=pw; }

protected:

	GfxTarget*					mpTarget;
	GfxWindow*					mpGfxWindow;

	CUIEvent*					UIEvent;
	CTFLXID						mxidThis;
	CTFLXID						mxidTopLevel;
	bool						mbInitialize;
	ERefreshPolicy				meRefreshPolicy;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

class GfxWindow : public GfxBuffer
{
	public:

	//////////////////////////////////////////////

	GfxWindow( int iX, int iY, int iW, int iH, const std::string & name = "NoName", void *pdata=0 );
	virtual ~GfxWindow();

	//////////////////////////////////////////////

	virtual void CreateContext();

	virtual void OnShow() {}

	virtual void GotFocus( void ) { mbHasFocus=true; }
	virtual void LostFocus( void ) { mbHasFocus=false; }
	bool HasFocus() const { return mbHasFocus; }

	void SetViewport( CUIViewport*pvp ) { mpViewport=pvp; }

	//////////////////////////////////////////////

	CTXBASE*	mpCTXBASE;
	bool		mbHasFocus;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

class GfxEnv : public NoRttiSingleton< GfxEnv >
{
	friend class GfxBuffer;
	friend class GfxWindow;
	friend class GfxTarget;
	//////////////////////////////////////////////////////////////////////////////

	public:

	GfxTarget* GetLoaderTarget() const { return gLoaderTarget; }
	void SetLoaderTarget( GfxTarget *ptarget ) { gLoaderTarget=ptarget; }

	recursive_mutex& GetGlobalLock() { return mGfxEnvMutex; }

	//////////////////////////////////////////////////////////////////////////////
	// Contex Factory

	GfxEnv();

	void RegisterWinContext( GfxWindow *pWin );

	//////////////////////////////////////////////////////////////////////////////

	GfxBuffer *GetMainWindow( void ) { return mpMainWindow; }
	void SetMainWindow( GfxWindow *pWin ) { mpMainWindow = pWin; }

	//////////////////////////////////////////////////////////////////////////////
	#if defined(_WIN32) && (!(defined(_XBOX)))
		static HWND GetMainHWND( void ) { return GetRef().mpMainWindow->GetContext()->GetHWND(); }
	#endif
	//////////////////////////////////////////////////////////////////////////////

	static GfxMaterial* GetDefaultUIMaterial( void ) { return GetRef().mpUIMaterial; }
	static GfxMaterial* GetDefault3DMaterial( void ) { return GetRef().mp3DMaterial; }

	static void SetTargetClass(const rtti::Class*pclass) { gpTargetClass=pclass; }
	static const rtti::Class* GetTargetClass() { return gpTargetClass; }
	void SetRuntimeEnvironmentVariable( const std::string& key, const std::string& val );
	const std::string& GetRuntimeEnvironmentVariable( const std::string& key ) const;

	void PushCreationParams( const GfxTargetCreationParams& p )
	{
		mCreationParams.push(p);
	}
	void PopCreationParams()
	{
		mCreationParams.pop();
	}
	const GfxTargetCreationParams& GetCreationParams()
	{
		return mCreationParams.top();
	}

	static DynamicVertexBuffer<SVtxV12C4T16>&	GetSharedDynamicVB();

	//////////////////////////////////////////////////////////////////////////////
	protected:
	//////////////////////////////////////////////////////////////////////////////

	GfxMaterial*						mpUIMaterial;
	GfxMaterial*						mp3DMaterial;
	GfxWindow *							mpMainWindow;
	GfxTarget*							gLoaderTarget;

	orkvector<GfxBuffer *>				mvActivePBuffers;
	orkvector<GfxBuffer *>				mvActiveWindows;
	orkvector<GfxBuffer *>				mvInactiveWindows;

	DynamicVertexBuffer<SVtxV12C4T16>	mVtxBufSharedVect;
	orkmap<std::string,std::string>		mRuntimeEnvironment;
	orkstack<GfxTargetCreationParams>	mCreationParams;
	recursive_mutex						mGfxEnvMutex;

	static const rtti::Class*	gpTargetClass;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

class DrawHudEvent : public ork::event::Event
{
    RttiDeclareConcrete( DrawHudEvent, ork::event::Event );
    
public:

	DrawHudEvent(GfxTarget *target = NULL, int camera_number = 1) : mTarget(target), mCameraNumber(camera_number) {}

	GfxTarget *GetTarget() const { return mTarget; }
	void SetTarget(GfxTarget *target) { mTarget = target; }

	int GetCameraNumber() const { return mCameraNumber; }
	void SetCameraNumber(int camera_number) { mCameraNumber = camera_number; }

private:
	GfxTarget *mTarget;
	int mCameraNumber;
};

/// ////////////////////////////////////////////////////////////////////////////
///
/// ////////////////////////////////////////////////////////////////////////////

} }

#define gGfxEnv ork::lev2::GfxEnv::GetRef()

#endif //  _EXECENV_GFX_GFXENV_H
