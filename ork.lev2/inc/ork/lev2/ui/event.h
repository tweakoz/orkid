#pragma once

#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/touch.h>
#include <ork/lev2/ui/coord.h>

namespace ork { namespace ui {

struct HandlerResult
{
	HandlerResult(Widget* ph=nullptr);

	bool WasHandled() const { return mHandler!=nullptr; }
	void SetHandled( Widget* by ) { mHandler = by; }

	Widget*    mHandler;
	bool       mHoldFocus;
};

struct EventCooked
{
	int miEventCode;
	int miKeyCode;
	int miX;
	int miY;
	int mLastX;
	int mLastY;
	float mUnitX;
	float mUnitY;
	float mLastUnitX;
	float mLastUnitY;
	bool mBut0;
	bool mBut1;
	bool mBut2;
    bool mCTRL; 
    bool mALT;	
    bool mSHIFT;	
    bool mMETA;	
    ork::FixedString<64> mAction;

	void Reset();
};

struct Event final // RawEvent
{	
	int mEventCode;

	lev2::GfxBuffer* mpGfxWin;
	Coordinate mUICoord;

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

	mutable EventCooked mFilteredEvent;

    static const int kmaxmtpoints = 4;
    
    MultiTouchPoint mMultiTouchPoints[kmaxmtpoints];
    int             miNumMultiTouchPoints;

	CVector2 GetUnitCoordBP() const
	{
		CVector2 rval;
		rval.SetX( 2.0f*mfUnitX-1.0f );
		rval.SetY( -(2.0f*mfUnitY-1.0f) );
		return rval;
	}

	Event()
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

struct DrawEvent
{
	DrawEvent( lev2::GfxTarget* ptarg ) : mpTarget(ptarg) {}
	lev2::GfxTarget* GetTarget() const { return mpTarget; }

private:

	lev2::GfxTarget* mpTarget;
};

}}