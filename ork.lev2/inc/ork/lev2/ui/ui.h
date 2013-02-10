////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _UI_UI_H
#define _UI_UI_H

#include <ork/lev2/ui/ui_enum.h>
//#include <ork/lev2/gfx/lev2renderer.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {
class HotKey;	
namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

class GfxTarget;

///////////////////////////////////////////////////////////////////////////////

class CUIViewport;
class GfxWindow;
class FrameTechniqueBase;
class PickBufferBase;

///////////////////////////////////////////////////////////////////////////////

enum EUIEventCode
{
	UIEV_UNKNOWN  = 0,
	UIEV_SHOW ,
	UIEV_HIDE ,
	UIEV_PUSH ,
	UIEV_DOUBLECLICK ,
	UIEV_RELEASE ,
	UIEV_DRAG ,
	UIEV_MOVE ,
	UIEV_KEY ,
	UIEV_KEYUP ,
	UIEV_DRAW ,
	UIEV_MOUSEWHEEL ,
	UIEV_MULTITOUCH ,
	UIEV_TABLET_BRUSH ,
	UIEV_GOT_KEYFOCUS ,
	UIEV_LOST_KEYFOCUS ,
};

///////////////////////////////////////////////////////////////////////////////

class CUICoord
{
	public: //
	
	CUICoord()
		: mfRawX( 0.0f )
		, mfRawY( 0.0f )
		, mfUnitX( 0.0f )
		, mfUnitY( 0.0f )
	{
	}

	f32 GetRawX( void ) const { return mfRawX; }
	f32 GetRawY( void ) const { return mfRawY; }
	f32 GetUnitX( void ) const { return mfUnitX; }
	f32 GetUnitY( void ) const { return mfUnitY; }

	void SetRawX( f32 u ) { mfRawX=u; }
	void SetRawY( f32 u ) { mfRawY=u; }
	void SetUnitX( f32 u ) { mfUnitX=u; }
	void SetUnitY( f32 u ) { mfUnitY=u; }

	private:

	f32 mfRawX;
	f32 mfRawY;
	f32 mfUnitX;
	f32 mfUnitY;
	
};

///////////////////////////////////////////////////////////////////////////////

struct MultiTouchPoint
{
    enum PointState
    {
        PS_UP=0,
        PS_PUSHED,
        PS_DOWN,
        PS_RELEASED,
    };
    
    float mfOrigX;
    float mfOrigY;
    float mfPrevX;
    float mfPrevY;
    float mfCurrX;
    float mfCurrY;
    float mfPressure;
    int   mID;
    PointState mState;
    
    MultiTouchPoint()
        : mfOrigX(0.0f)
        , mfOrigY(0.0f)
        , mfPrevX(0.0f)
        , mfPrevY(0.0f)
        , mfCurrX(0.0f)
        , mfCurrY(0.0f)
        , mfPressure(0.0f)
        , mID(-1)
        , mState(PS_UP)
    {
    }
};

class CUIEvent
{
public:
	
	int mEventCode;

	GfxWindow*	mpGfxWin;
	CUICoord	mUICoord;

	int miEventCode;
	int miX;
	int miY;
	int miLastX;
	int miLastY;
	int miMWY;
	int miState;
	int miKeyCode;
	int	miNumHits;

	f32 mfX;
	f32 mfY;
	f32 mfUnitX;
	f32 mfUnitY;
	f32 mfLastUnitX;
	f32 mfLastUnitY;
	f32 mfPressure;

    bool mbCTRL; 
    bool mbALT;	
    bool mbSHIFT;	
    bool mbMETA;	
	
	bool mbLeftButton;
	bool mbMiddleButton;
	bool mbRightButton;
        
	CVector4 mvRayN;
	CVector4 mvRayF;

	void* mpBlindEventData;

    static const int kmaxmtpoints = 4;
    
    MultiTouchPoint mMultiTouchPoints[kmaxmtpoints];
    int             miNumMultiTouchPoints;

	CVector2 GetUnitCoordBP()
	{
		CVector2 rval;
		rval.SetX( 2.0f*mfUnitX-1.0f );
		rval.SetY( -(2.0f*mfUnitY-1.0f) );
		return rval;
	}

	CUIEvent()
		: mUICoord()
		, miEventCode(0)
		, miX(0)
		, miY(0)
		, mfUnitX(0.0f)
		, mfUnitY(0.0f)
		, mfLastUnitX(0.0f)
		, mfLastUnitY(0.0f)
		, miLastX(0)
		, miLastY(0)
		, miState(0)
		, miKeyCode(0)
		, miNumHits(0)
		, mbCTRL(false)
		, mbALT(false)
		, mbSHIFT(false)
		, mbMETA(false)
		, mbLeftButton(false)
		, mbMiddleButton(false)
		, mbRightButton(false)
		, mvRayN(CReal(0.0f),CReal(0.0f),CReal(0.0f),CReal(0.0f))
		, mvRayF(CReal(0.0f),CReal(0.0f),CReal(0.0f),CReal(0.0f))
		, mpBlindEventData( 0 )
		, mpGfxWin( 0 )
        , miNumMultiTouchPoints(0)
	{
	}
	
	void GetFromOS( void );
		
};

///////////////////////////////////////////////////////////////////////////////

class CWidgetFlags
{
	public: //

	unsigned	mbEnabled			:1;		// 1
	unsigned	muState				:3;		// 4
	unsigned	muPushUIShader		:1;		// 5
	unsigned	muGroupDelete		:1;		// 6
	unsigned	muSizeDirty			:1;		// 7
	unsigned	muPad				:25;	// 32

	CWidgetFlags() :
		mbEnabled(1),
		muState(0),
		muPushUIShader(0),
		muGroupDelete(1),
		muSizeDirty(1)
	{
	}

	void Enable( void ) { mbEnabled = true; }
	void Disable( void ) { mbEnabled = false; }
	void SetState( EUIWidgetState eState ) { muState = eState; }
	void SetPushUIShader( bool bval ) { muPushUIShader=bval; }
	void SetSizeDirty( bool bval ) { muSizeDirty=bval; }
	bool GetEnable( void ) const { return mbEnabled; }
	EUIWidgetState GetState( void )	{ return static_cast<EUIWidgetState>(muState); }
	bool GetPushUIShader( void ) const { return muPushUIShader; }
	bool IsSizeDirty( void ) const { return muSizeDirty; }
};

///////////////////////////////////////////////////////////////////////////////

class DrawEvent
{
public:

	DrawEvent( GfxTarget* ptarg ) : mpTarget(ptarg) {}
	GfxTarget* GetTarget() const { return mpTarget; }

private:

	GfxTarget* mpTarget;
};

///////////////////////////////////////////////////////////////////////////////

class CUIViewport : public ork::Object
{
	RttiDeclareAbstract( CUIViewport, ork::Object );

public:

	///////////////////////////////////////

	enum ERefreshState
	{
		EREFRESH_PLAY = 0,		// Refreshes at full framerate
		EREFRESH_UIEVENT,		// Refreshes on any UI input event
		EREFRESH_REQUEST,		// Refreshes on any draw request
	};

	struct SViewportFlags
	{
		ERefreshState	meRefreshState			:2;
		unsigned		mPad					:30;
	};

	///////////////////////////////////////

	void ExtDraw( GfxTarget* pTARG );
	void Draw( DrawEvent& drq );
	
	void BeginFrame( GfxTarget* pTARG );
	void EndFrame( GfxTarget* pTARG );
	virtual void DoDraw() = 0;


	///////////////////////////////////////

	CWidgetFlags &GetFlagsRef( void ) { return mWidgetFlags; }
	const std::string & GetName( void ) const { return msName; }
	void SetName( const std::string & name ) { msName = name; }
	virtual void Enter( void ) { mWidgetFlags.SetState( EUI_WIDGET_HOVER ); }
	virtual void Exit( void ) { mWidgetFlags.SetState( EUI_WIDGET_OFF ); }

	virtual void resize( void );

	CUIViewport( const std::string & name, int x, int y, int w, int h, CColor3 color, F32 depth );
	virtual ~CUIViewport() {}

	int GetX( void ) const { return static_cast<int>(miX); }
	int GetY( void ) const  { return static_cast<int>(miY); }
	int GetX2( void ) const  { return static_cast<int>(miX2); }
	int GetY2( void ) const  { return static_cast<int>(miY2); }
	int GetW( void ) const  { return static_cast<int>(miW); }
	int GetH( void ) const  { return static_cast<int>(miH); }
	void SetX( int X ) { SetSize( X, miY, miW, miH ); }
	void SetY( int Y ) { SetSize( miX, Y, miW, miH ); }
	void SetX2( int X2 ) { SetSize( miX, miY, (X2-miX), miH ); }
	void SetY2( int Y2 ) { SetSize( miX, miY, miW, (Y2-miY) ); }
	void SetW( int W ) { SetSize( miX, miY, W, miH ); }
	void SetH( int H ) { SetSize( miX, miY, miW, H ); }

	void SetSize( int iX, int iY, int iW, int iH )
	{
		miLX = miX;
		miLY = miY;
		miLW = miW;
		miLH = miH;

		miX = s16(iX);
		miY = s16(iY);
		miW = s16(iW);
		miH = s16(iH);
		mWidgetFlags.SetSizeDirty( true );
	}

	void GotKeyboardFocus( void ) { mbKeyboardFocus=true; }
	void LostKeyboardFocus( void ) { mbKeyboardFocus=false; }
	bool HasKeyboardFocus( void ) const { return mbKeyboardFocus; }

	bool IsKeyDepressed( int ch );
	bool IsHotKeyDepressed( const char* pact );
	bool IsHotKeyDepressed( const HotKey& hk );

	virtual EUIHandled UIEventHandler( CUIEvent *pEv )
	{
		return EUI_NOT_HANDLED;
	}

	CColor3 &GetClearColorRef( void ) { return mcClearColor; }
	F32 GetClearDepth( void ) { return mfClearDepth; }

	void Attach();
	void Clear();

	GfxTarget* GetTarget( void ) const { return mpTarget; }

	virtual void Show( void ) {}
	virtual void Hide( void ) {}
	void PushFrameTechnique( FrameTechniqueBase* ftek );
	void PopFrameTechnique();
	FrameTechniqueBase* GetFrameTechnique() const;

	PickBufferBase* GetPickBuffer() { return mpPickBuffer; }

protected:

	virtual void Init( GfxTarget* pTARG ) {}


	PickBufferBase*		mpPickBuffer;
	bool				mbClear;
	CColor3				mcClearColor;
	F32					mfClearDepth;
	GfxTarget*			mpTarget;
	static int 						siDefaultWidgetHeight;
	orkstack<FrameTechniqueBase*>	mpActiveFrameTek;
	DrawEvent*						mpDrawEvent;

	int					miX, miY, miW, miH, miX2, miY2;		
	int					miLX, miLY, miLW, miLH, miLX2, miLY2;
	std::string			msName;								
	CWidgetFlags		mWidgetFlags;						
	int					miLevel;
	bool				mbKeyboardFocus;
	bool				mbDrawOK;
	bool				mbInit;

};

///////////////////////////////////////////////////////////////////////////////

} }

///////////////////////////////////////////////////////////////////////////////

#endif // _UI_UI_H
