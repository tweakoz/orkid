////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/ctxbase.h>
#include <ork/lev2/input/input.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/camera/cameraman.h>
#include <ork/lev2/qtui/qtui.h>
#include <ork/lev2/ui/viewport.h>

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QGesture>
#include <ork/math/basicfilters.h>

#if defined(_DARWIN)
//#define USE_MTOUCH
#include <dispatch/dispatch.h>
#include "touch.h"
#elif defined(IX)
//#include <dispatch/dispatch.h>
#endif

#if ! defined (_CYGWIN)

extern "C" void StartTouchReciever(void*tr);

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////
#if defined(USE_MTOUCH)
struct QCtxWidgetTR : public ::ITouchReciever
{

	QCtxWidgetTR( QCtxWidget* pw ) : ITouchReciever((void*)pw) {}

	void OnTouchBegin(const MtFinger* finger) // virtual
	{	QCtxWidget* pW = (QCtxWidget*) mpCTX;
		pW->OnTouchBegin(finger);
	}
	void OnTouchEnd(const MtFinger* finger) // virtual
	{	QCtxWidget* pW = (QCtxWidget*) mpCTX;
		pW->OnTouchEnd(finger);
	}
	void OnTouchUpdate(const MtFinger* finger) // virtual
	{	QCtxWidget* pW = (QCtxWidget*) mpCTX;
		pW->OnTouchUpdate(finger);
	}
};
#endif

///////////////////////////////////////////////////////////////////////////////

QCtxWidget::QCtxWidget( CTQT* pctxbase, QWidget* parent )
	: QWidget( parent )
	, mpCtxBase( pctxbase )
	, mbSignalConnected( false )
	, mQtTimer( this )
	, mbEnabled( false )
	, miWidth(1)
	, miHeight(1)
	, mTouchReciver(nullptr)
{
	setMouseTracking(true);
	setAutoFillBackground(false);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
	setFocusPolicy (  Qt::StrongFocus );
	connect( & mQtTimer, SIGNAL(timeout()), this, SLOT(repaint()));
	show();
	activateWindow();
	setAttribute( Qt::WA_DeleteOnClose );
    //setAttribute( Qt::WA_AcceptTouchEvents );

    //grabGesture(Qt::PanGesture);

#if defined(USE_MTOUCH)
	QCtxWidgetTR* ptr = new QCtxWidgetTR(this);
	mTouchReciver = (void*) ptr;
	StartTouchReciever(mTouchReciver);
#endif
}

QCtxWidget::~QCtxWidget()
{
	if( mpCtxBase )
	{
		delete mpCtxBase;
		//delete
	}
}

int ginump = 0;
std::map<int,int> gId2Idx;

void QCtxWidget::SendOrkUiEvent()
{
    if( UIEvent().mpGfxWin )
        UIEvent().mpGfxWin->GetRootWidget()->HandleUiEvent( UIEvent() );
}

void QCtxWidget::OnTouchBegin( const MtFinger* pfinger )
{
#if defined(USE_MTOUCH)
	int ID = pfinger->identifier;
	int idx = ginump++;
	gId2Idx[ID]=idx;


    float fW = float(width());
    float fH = float(height());
    ui::Event& outev = UIEvent();
    outev.miEventCode = UIEV_MULTITOUCH;
	outev.miNumMultiTouchPoints = ginump;
    outev.mfX = pfinger->normalized.pos.x*fW;
    outev.mfY = pfinger->normalized.pos.y*fH;

	outev.mMultiTouchPoints[idx].mfOrigX = outev.mfX;
	outev.mMultiTouchPoints[idx].mfOrigY = outev.mfY;
	outev.mMultiTouchPoints[idx].mfCurrX = outev.mfX;
	outev.mMultiTouchPoints[idx].mfCurrY = outev.mfY;
	outev.mMultiTouchPoints[idx].mState = MultiTouchPoint::PS_PUSHED;
	outev.mMultiTouchPoints[idx].mID = ID;

	printf( "pW<%p> ginump<%d> TB<%d> fx<%f> fy<%f>\n", this, ginump, ID, outev.mfX, outev.mfY );
	//MultiTouchPoint& opnt = mMultiTouchTrackPoints[ipointidx];

	SendOrkUiEvent();

    if( mpCtxBase ) mpCtxBase->SlotRepaint();
#endif
}

int ilastnpoints = 0;

void QCtxWidget::OnTouchUpdate( const MtFinger* pfinger )
{
#if defined(USE_MTOUCH)
	int ID = pfinger->identifier;
	int idx = gId2Idx[ID];

    float fW = float(width());
    float fH = float(height());
    ui::Event& outev = UIEvent();
    outev.miEventCode = UIEV_MULTITOUCH;
	outev.miNumMultiTouchPoints = ginump;
    outev.mfX = pfinger->normalized.pos.x*fW;
    outev.mfY = pfinger->normalized.pos.y*fH;
	outev.mMultiTouchPoints[idx].mfPrevX = outev.mMultiTouchPoints[idx].mfCurrX;
	outev.mMultiTouchPoints[idx].mfPrevY = outev.mMultiTouchPoints[idx].mfCurrY;
	outev.mMultiTouchPoints[idx].mfCurrX = outev.mfX;
	outev.mMultiTouchPoints[idx].mfCurrY = outev.mfY;
	outev.mMultiTouchPoints[idx].mState = MultiTouchPoint::PS_DOWN;

	outev.mMultiTouchPoints[idx].mID = ID;
	//mMultiTouchTrackPoints

	bool bupdate = false;

	if( ilastnpoints!=ginump )
		bupdate=true;
	ilastnpoints=ginump;

	for( int i=0;i<ilastnpoints;i++ )
	{
		if( outev.mMultiTouchPoints[i].mfPrevX!= outev.mMultiTouchPoints[i].mfCurrX )
			bupdate=true;
		if( outev.mMultiTouchPoints[i].mfPrevY!= outev.mMultiTouchPoints[i].mfCurrY )
			bupdate=true;

	}

	//printf( "pW<%p> ginump<%d> TU<%d> fx<%f> fy<%f>\n", this, ginump, ID, poutev->mfX, poutev->mfY );

    if( bupdate )
		SendOrkUiEvent();

   // if( mpCtxBase ) mpCtxBase->SlotRepaint();

#endif
}

void QCtxWidget::OnTouchEnd( const MtFinger* pfinger )
{
#if defined(USE_MTOUCH)
	int ID = pfinger->identifier;
	int idx = gId2Idx[ID];
	gId2Idx.erase(ID);
	ginump--;
    float fW = float(width());
    float fH = float(height());
    ui::Event& outev = UIEvent();
    outev.miEventCode = UIEV_MULTITOUCH;
	outev.miNumMultiTouchPoints = ginump;
    outev.mfX = pfinger->normalized.pos.x*fW;
    outev.mfY = pfinger->normalized.pos.y*fH;
	outev.mMultiTouchPoints[idx].mState = MultiTouchPoint::PS_RELEASED;
	outev.mMultiTouchPoints[idx].mID = ID;

	printf( "pW<%p> ginump<%d> TE<%d> fx<%f> fy<%f>\n", this, ginump, ID, outev.mfX, outev.mfY );

	SendOrkUiEvent();

    if( mpCtxBase ) mpCtxBase->SlotRepaint();
#endif
}

///////////////////////////////////////////////////////////////////////////////

bool QCtxWidget::event(QEvent* event)
{
	return QWidget::event(event);
}
///////////////////////////////////////////////////////////////
static uint64_t uevhash = 0;

/*void QCtxWidget::multiTouchEventCommon(QTouchEvent* ptev)
{
    bool is_end_event = (ptev->type() == QEvent::TouchEnd);

    ui::Event& outev = UIEvent();

	Qt::KeyboardModifiers modifiers = ptev->modifiers();

    poutev->mpBlindEventData = (void*) ptev;

	poutev->mbALT = (modifiers&Qt::AltModifier);
	poutev->mbCTRL = (modifiers&Qt::ControlModifier);
	poutev->mbSHIFT = (modifiers&Qt::ShiftModifier);
	poutev->mbMETA = (modifiers&Qt::MetaModifier);

    poutev->miEventCode = UIEV_MULTITOUCH;
    poutev->mfX = 0.0f;
    poutev->mfY = 0.0f;

    const tpointlist& points = ptev->touchPoints();

    int inumpnts = 0;
    std::map<int,const QTouchEvent::TouchPoint*> downpoints;
    for( tpointlist::const_iterator it=points.begin(); it!=points.end(); it++ )
    {
        const QTouchEvent::TouchPoint& point = *it;
        int id = point.id();
        downpoints[id] = &point;
        switch( point.state() )
        {
            case Qt::TouchPointPressed:
                printf( "point<%d> pressed\n", id );
                break;
            case Qt::TouchPointMoved:
                //printf( "point<%d> moved\n", id );
                break;
            case Qt::TouchPointReleased:
                printf( "point<%d> released\n", id );
                break;
        }
    }
    //QPointF vpPOS = mapToGlobal(QPoint(0,0));
    float fW = float(width());
    float fH = float(height());
    int ipointidx = 0;
    for( std::map<int,const QTouchEvent::TouchPoint*>::const_iterator itf=downpoints.begin(); itf!=downpoints.end(); itf++ )
    {
        if( ipointidx>=ui::Event::kmaxmtpoints ) continue;

        MultiTouchPoint& opnt = mMultiTouchTrackPoints[ipointidx];
        opnt.mfPrevX = opnt.mfCurrX;
        opnt.mfPrevY = opnt.mfCurrY;
        bool iscurrentlydown = (false==is_end_event);
        if( iscurrentlydown )
        {
            const QTouchEvent::TouchPoint& point = *(itf->second);
            int id = point.id();
            bool breleased = (point.state()==Qt::TouchPointReleased);

            QPointF pnt = point.normalizedPos();
            QRectF rec = point.rect();
            opnt.mfCurrX = pnt.x()*fW;
            opnt.mfCurrY = pnt.y()*fH;
            opnt.mfPressure = point.pressure();
            opnt.mID = id;
            switch( opnt.mState )
            {
                case MultiTouchPoint::PS_UP:
                case MultiTouchPoint::PS_RELEASED:
                    opnt.mState = MultiTouchPoint::PS_PUSHED;
                    opnt.mfOrigX = opnt.mfCurrX;
                    opnt.mfOrigY = opnt.mfCurrY;
                    opnt.mfPrevX = opnt.mfCurrX;
                    opnt.mfPrevY = opnt.mfCurrY;
                    break;
                case MultiTouchPoint::PS_PUSHED:
                    opnt.mState = MultiTouchPoint::PS_DOWN;
                    break;
                case MultiTouchPoint::PS_DOWN:
                    if( breleased )
                        opnt.mState = MultiTouchPoint::PS_RELEASED;
                    break;
            }
            inumpnts++;
        }
        else
        {
            switch( opnt.mState )
            {
                case MultiTouchPoint::PS_PUSHED:
                case MultiTouchPoint::PS_DOWN:
                    opnt.mState = MultiTouchPoint::PS_RELEASED;
                    break;
                case MultiTouchPoint::PS_RELEASED:
                    opnt.mState = MultiTouchPoint::PS_UP;
                    break;
                case MultiTouchPoint::PS_UP:
                    break;
            }
        }
        ipointidx++;
    }

    for( int i=0; i<ui::Event::kmaxmtpoints; i++ )
    {
        const MultiTouchPoint& ipnt = mMultiTouchTrackPoints[i];
        MultiTouchPoint& opnt = poutev->mMultiTouchPoints[i];
        opnt = ipnt;
    }

    poutev->miNumMultiTouchPoints = inumpnts;

}*/

///////////////////////////////////////////////////////////////
/*static bool gINMTEV = false;

bool QCtxWidget::multiTouchBeginEvent(QTouchEvent* ptev)
{
    uevhash = 0;
    //printf( "MTBEG\n" );
    gINMTEV=true;
    for( int i=0; i<ui::Event::kmaxmtpoints; i++ )
    {
        new (&mMultiTouchTrackPoints[i]) MultiTouchPoint();
    }

    multiTouchEventCommon( ptev );

    if( UIEVENT().mpGfxWin )
        UIEVENT().mpGfxWin->GetRootWidget()->HandleUiEvent( UIEvent() );

    if( mpCtxBase ) mpCtxBase->SlotRepaint();

    return true; //QWidget::event(ptev);
}
///////////////////////////////////////////////////////////////
bool QCtxWidget::multiTouchUpdateEvent(QTouchEvent* ptev)
{
    multiTouchEventCommon( ptev );

    //boost::Crc64 crc64;
	//crc64_init(crc64);
    //for( int i=0; i<ui::Event::kmaxmtpoints; i++ )
    //{
    //    const MultiTouchPoint& pnt = mMultiTouchTrackPoints[i];
	//	crc64_compute(crc64, &pnt.mfCurrX, sizeof(pnt.mfCurrX));
	//	crc64_compute(crc64, &pnt.mfCurrY, sizeof(pnt.mfCurrY));
	//	crc64_compute(crc64, &pnt.mState, sizeof(pnt.mState));
	//}
	//crc64_fin(crc64);

    static qint64 glastupd = QDateTime::currentMSecsSinceEpoch();
    static int gmtepd = 0;

    qint64 curupd = QDateTime::currentMSecsSinceEpoch();

//    if( uevhash!=crc64.crc0 )
    {
        if( UIEVENT().mpGfxWin )
            UIEVENT().mpGfxWin->GetRootWidget()->HandleUiEvent( UIEvent() );
        //uevhash=crc64.crc0;
    }

    gmtepd++;

    if( (curupd-glastupd)>66 )
    {
        dispatch_async( dispatch_get_main_queue(),
        ^{
            if( mpCtxBase )
                  mpCtxBase->SlotRepaint();
        });
        glastupd=curupd;
    }
    //try some sort of queued and throttled repaint (using GCD?)

    return true; //QWidget::event(ptev);
}
///////////////////////////////////////////////////////////////
bool QCtxWidget::multiTouchEndEvent(QTouchEvent* ptev)
{
    printf( "MTEND\n" );
    gINMTEV=false;

    multiTouchEventCommon( ptev );

    UIEVENT().miNumMultiTouchPoints = 0;

    if( UIEVENT().mpGfxWin )
        UIEVENT().mpGfxWin->GetRootWidget()->HandleUiEvent( UIEvent() );

    if( mpCtxBase ) mpCtxBase->SlotRepaint();

    //bool rv = QWidget::event(ptev);

    for( int i=0; i<ui::Event::kmaxmtpoints; i++ )
    {
        new (&mMultiTouchTrackPoints[i]) MultiTouchPoint();
    }

    return true;
}
 */

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::showEvent ( QShowEvent * event )
{
	UIEvent().mpBlindEventData = (void*) event;
	QWidget::showEvent( event );
	parentWidget()->show();
	//mpCtxBase->Show();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::resizeEvent( QResizeEvent * event )
{
	UIEvent().mpBlindEventData = (void*) event;
	QWidget::resizeEvent( event );
	QSize size = event->size();
	int X = 0;
	int Y = 0;
	int W = size.rwidth();
	int H = size.rheight();
    printf( "W<%d> H<%d>\n", W, H );
    miWidth = W;
	miHeight = H;
	if( mpCtxBase )
		mpCtxBase->Resize( X, Y, W, H );
}

void QCtxWidget::paintEvent( QPaintEvent * event )
{
	static int gistackctr = 0;
	static int gictr = 0;

	gistackctr++;
	if( (1 == gistackctr) && (gictr>0) )
	{	UIEvent().mpBlindEventData = (void*) event;
		if( IsEnabled() )
		{
			if( mpCtxBase )
				mpCtxBase->SlotRepaint();
		}
	}
	gistackctr--;
	gictr++;
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::MouseEventCommon( QMouseEvent * event )
{
	auto& uiev = UIEvent();

	uiev.mpBlindEventData = (void*) event;

	CInputManager::GetRef().Poll();

	Qt::MouseButtons Buttons = event->buttons();
	Qt::KeyboardModifiers modifiers = event->modifiers();

	int ix = event->x();
	int iy = event->y();

	uiev.mbALT = (modifiers&Qt::AltModifier);
	uiev.mbCTRL = (modifiers&Qt::ControlModifier);
	uiev.mbSHIFT = (modifiers&Qt::ShiftModifier);
	uiev.mbMETA = (modifiers&Qt::MetaModifier);
	uiev.mbLeftButton = (Buttons&Qt::LeftButton);
	uiev.mbMiddleButton = (Buttons&Qt::MidButton);
	uiev.mbRightButton = (Buttons&Qt::RightButton);

	uiev.miLastX = uiev.miX;
	uiev.miLastY = uiev.miY;

	uiev.miX = ix;
	uiev.miY = iy;

	float unitX = float(ix)/float(miWidth);
	float unitY = float(iy)/float(miHeight);

	uiev.mfLastUnitX = uiev.mfUnitX;
	uiev.mfLastUnitY = uiev.mfUnitY;
	uiev.mfUnitX = unitX;
	uiev.mfUnitY = unitY;

	//printf( "UNITX<%f> UNITY<%f>\n", unitX, unitY );
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::mouseMoveEvent ( QMouseEvent * event )
{
	auto& uiev = UIEvent();
	auto gfxwin = uiev.mpGfxWin;
	auto vp = gfxwin ? gfxwin->GetRootWidget() : nullptr;

	MouseEventCommon( event );

	Qt::MouseButtons Buttons = event->buttons();

	uiev.miEventCode = (Buttons == Qt::NoButton)  ? ork::ui::UIEV_MOVE : ork::ui::UIEV_DRAG;

	if( vp )
		vp->HandleUiEvent( uiev );

	if( mpCtxBase ) mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::mousePressEvent ( QMouseEvent * event )
{
	MouseEventCommon( event );
	auto& uiev = UIEvent();
	auto gfxwin = uiev.mpGfxWin;
	auto vp = gfxwin ? gfxwin->GetRootWidget() : nullptr;
	uiev.miEventCode = ork::ui::UIEV_PUSH;
	if( vp )
		vp->HandleUiEvent( uiev );

	if( mpCtxBase ) mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::mouseDoubleClickEvent ( QMouseEvent * event )
{
	MouseEventCommon( event );
	auto& uiev = UIEvent();
	auto gfxwin = uiev.mpGfxWin;
	auto vp = gfxwin ? gfxwin->GetRootWidget() : nullptr;
	uiev.miEventCode = ork::ui::UIEV_DOUBLECLICK;
	if( vp )
		vp->HandleUiEvent( uiev );

	if( mpCtxBase ) mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::mouseReleaseEvent ( QMouseEvent * event )
{
	MouseEventCommon( event );
	auto& uiev = UIEvent();
	auto gfxwin = uiev.mpGfxWin;
	auto vp = gfxwin ? gfxwin->GetRootWidget() : nullptr;
	uiev.miEventCode = ork::ui::UIEV_RELEASE;
	if( vp )
		vp->HandleUiEvent( uiev );

	if( mpCtxBase ) mpCtxBase->SlotRepaint();

}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::wheelEvent ( QWheelEvent* qem )
{
	auto& uiev = UIEvent();
	auto gfxwin = uiev.mpGfxWin;
	auto vp = gfxwin ? gfxwin->GetRootWidget() : nullptr;

	uiev.mpBlindEventData = (void*) qem;
	static avg_filter<3> gScrollFilter;

	#if defined(_DARWIN) // trackpad gesture filter
	int irawdelta = qem->delta();
	int idelta = (2*gScrollFilter.compute(irawdelta)/9);
	#else
	int delta = qem->delta();
	#endif

	Qt::KeyboardModifiers modifiers = qem->modifiers();

	uiev.mbALT = (modifiers&Qt::AltModifier);
	uiev.mbCTRL = (modifiers&Qt::ControlModifier);
	uiev.mbSHIFT = (modifiers&Qt::ShiftModifier);
	uiev.mbMETA = (modifiers&Qt::MetaModifier);

	uiev.miEventCode = ork::ui::UIEV_MOUSEWHEEL;

	uiev.miMWY = idelta;
	if( vp && idelta!=0 )
		vp->HandleUiEvent( uiev );

	if( mpCtxBase ) mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::keyPressEvent ( QKeyEvent * event )
{
    if( event->isAutoRepeat() )
        return;

	auto& uiev = UIEvent();
	auto gfxwin = uiev.mpGfxWin;
	auto vp = gfxwin ? gfxwin->GetRootWidget() : nullptr;

	uiev.mpBlindEventData = (void*) event;
	uiev.miEventCode = ork::ui::UIEV_KEY;

	int ikeyUNI = event->key();

	uiev.miKeyCode = ikeyUNI;
	Qt::KeyboardModifiers modifiers = event->modifiers();

	uiev.mbALT = (modifiers&Qt::AltModifier);
	uiev.mbCTRL = (modifiers&Qt::ControlModifier);
	uiev.mbSHIFT = (modifiers&Qt::ShiftModifier);
	uiev.mbMETA = (modifiers&Qt::MetaModifier);

	if( (ikeyUNI>=Qt::Key_A) && (ikeyUNI<=Qt::Key_Z) )
	{
		uiev.miKeyCode = (ikeyUNI-Qt::Key_A)+int('a');
	}
	if( ikeyUNI==0x01000004 ) // enter != (Qt::Key_Enter)
	{
		uiev.miKeyCode = 13;
	}

	if( vp )
		vp->HandleUiEvent( uiev );

	if( mpCtxBase ) mpCtxBase->SlotRepaint();

}

///////////////////////////////////////////////////////////////////////////////

void QCtxWidget::keyReleaseEvent ( QKeyEvent * event )
{
    if( event->isAutoRepeat() )
        return;

	auto& uiev = UIEvent();
	auto gfxwin = uiev.mpGfxWin;
	auto vp = gfxwin ? gfxwin->GetRootWidget() : nullptr;

	uiev.mpBlindEventData = (void*) event;
	uiev.miEventCode = ork::ui::UIEV_KEYUP;

	int ikeyUNI = event->key();

	uiev.miKeyCode = ikeyUNI;

	if( (ikeyUNI>=Qt::Key_A) && (ikeyUNI<=Qt::Key_Z) )
	{

		uiev.miKeyCode = (ikeyUNI-Qt::Key_A)+int('a');
	}
	if( ikeyUNI==0x01000004 ) // enter != (Qt::Key_Enter)
	{
		uiev.miKeyCode = 13;
	}

	if( vp )
		vp->HandleUiEvent( uiev );

	if( mpCtxBase ) mpCtxBase->SlotRepaint();
}

///////////////////////////////////////////////////////////////////////////////
void QCtxWidget::focusInEvent ( QFocusEvent * event )
{
	auto& uiev = UIEvent();
	auto gfxwin = uiev.mpGfxWin;
	auto vp = gfxwin ? gfxwin->GetRootWidget() : nullptr;
	uiev.miEventCode = ork::ui::UIEV_GOT_KEYFOCUS;
	uiev.mpBlindEventData = (void*) event;
	if( Target() )
	{
		if( vp )
			vp->HandleUiEvent( UIEvent() );
	}
	//orkprintf( "CTQT %08x got keyboard focus\n", this );
	QWidget::focusInEvent( event );
	if( GetGfxWindow() )
		GetGfxWindow()->GotFocus();
	//////////////////////////////////////////
	if( mpCtxBase ) mpCtxBase->SlotRepaint();
}

void QCtxWidget::focusOutEvent ( QFocusEvent * event )
{
	auto& uiev = UIEvent();
	auto gfxwin = uiev.mpGfxWin;
	auto vp = gfxwin ? gfxwin->GetRootWidget() : nullptr;
	uiev.miEventCode = ork::ui::UIEV_LOST_KEYFOCUS;
	uiev.mpBlindEventData = (void*) event;
	if( vp )
	{
		vp->HandleUiEvent( UIEvent() );
	}
	//orkprintf( "CTQT %08x lost keyboard focus\n", this );
	QWidget::focusOutEvent( event );
	if( GetGfxWindow() )
		GetGfxWindow()->LostFocus();
	//////////////////////////////////////////
	if( mpCtxBase ) mpCtxBase->SlotRepaint();
}

ui::Event& QCtxWidget::UIEvent()
{
	return mpCtxBase->mUIEvent;
}
const ui::Event& QCtxWidget::UIEvent() const
{
	return mpCtxBase->mUIEvent;
}

GfxTarget*	QCtxWidget::Target() const
{
	return mpCtxBase->mpTarget;
}

GfxWindow* QCtxWidget::GetGfxWindow() const
{
	return mpCtxBase->mpGfxWindow;
}

bool QCtxWidget::AlwaysRun() const
{
	return mpCtxBase->mbAlwaysRun;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

QTimer& CTQT::Timer() const
{
	return mpQtWidget->mQtTimer;
}

///////////////////////////////////////////////////////////////////////////////

void CTQT::Show()
{
	mParent->show();
	if( mbInitialize )
	{
		printf( "CreateCONTEXT\n" );
		mpGfxWindow->CreateContext();
		mpGfxWindow->OnShow();
		mbInitialize = false;
	}
}

void CTQT::Hide()
{
	mParent->hide();
}

///////////////////////////////////////////////////////////////////////////////

void CTQT::SetRefreshRate( int ihz )
{
	if( ihz >= 0 )
	{
		miUserMillis = (ihz<=0) ? 2000 : int( 1000.0f / float(ihz) );
	}

	switch( meRefreshPolicy )
	{
		case CTXBASE::EREFRESH_FASTEST:
			miQtMillis = 0;
			break;
		case CTXBASE::EREFRESH_WHENDIRTY:
			miQtMillis = -1;
			break;
		case CTXBASE::EREFRESH_FIXEDFPS:
			miQtMillis = miUserMillis-1;
			break;
		default:
			break;
	}
	if( miQtMillis != miLastMillis )
	{
		if( miQtMillis==-1 )
		{
			Timer().stop();
		}
		else
		{
			Timer().start();
			Timer().setInterval(miQtMillis);
		}
		miLastMillis = miQtMillis;
	}
}

///////////////////////////////////////////////////////////////////////////////

void CTQT::SetRefreshPolicy( CTXBASE::ERefreshPolicy epolicy )
{
	meRefreshPolicy = epolicy;
	SetRefreshRate( -1 );
}

///////////////////////////////////////////////////////////////////////////////

CTQT::CTQT( GfxWindow* pwin, QWidget* pparent )
	: CTXBASE( pwin )
	, mbAlwaysRun( false )
	, miLastMillis( -1 )
	, miQtMillis( -1 )
	, miUserMillis( -1 )
	, mix( 0 )
	, miy( 0 )
	, miw( 0 )
	, mih( 0 )
	, mParent( 0 )
	, mDrawLock(0)
{
	this->SetThisXID( CTFLXID(0) );
	this->SetTopXID( CTFLXID(0) );

	SetParent(pparent);
}

CTQT::~CTQT()
{
	if( mpGfxWindow ) delete mpGfxWindow;
}

///////////////////////////////////////////////////////////////////////////////

void CTQT::SetParent( QWidget* pparent )
{
	printf( "CTQT::SetParent() pparent<%p>\n", pparent );
	QMainWindow* mainwin = 0;
	if( 0 == pparent )
	{
		mainwin = new QMainWindow;
		pparent = mainwin;
	}

	mParent = pparent;
	mpQtWidget = new QCtxWidget( this, pparent );
	if( mainwin )
	{
		mainwin->setCentralWidget(mpQtWidget);
	}

	this->SetThisXID( (CTFLXID)winId() );
	this->SetTopXID( (CTFLXID)pparent->winId() );

	SetRefreshPolicy( CTXBASE::EREFRESH_WHENDIRTY ); // EREFRESH_FASTEST
	SetRefreshRate( 120 );

	mpGfxWindow->SetDirty( true );
}

///////////////////////////////////////////////////////////////////////////////

CVector2 CTQT::MapCoordToGlobal( const CVector2& v ) const
{
	QPoint p(v.GetX(),v.GetY());
	QPoint p2 = mpQtWidget->mapToGlobal(p);
	return CVector2(p2.x(),p2.y());
}

///////////////////////////////////////////////////////////////////////////////

void CTQT::Resize( int X, int Y, int W, int H )
{
	//////////////////////////////////////////////////////////
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	//////////////////////////////////////////////////////////

	this->SetThisXID( (CTFLXID)winId() );
	//printf( "CTQT::Resize() mpTarget<%p>\n", mpTarget );
	if( mpTarget )
	{
		mpTarget->SetSize( X, Y, W, H );
		mUIEvent.mpGfxWin = (GfxWindow*) mpTarget->FBI()->GetThisBuffer();
		if( mUIEvent.mpGfxWin )
			mUIEvent.mpGfxWin->Resize( X,Y,W,H );
	}
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();

}

///////////////////////////////////////////////////////////////////////////////

void CTQT::SlotRepaint()
{
	auto lamb = [&]()
	{
		if( nullptr == GfxEnv::GetRef().GetDefaultUIMaterial() )
			return;

		ork::PerfMarkerPush( "ork.viewport.draw.begin" );

		this->mDrawLock++;
		if( this->mDrawLock == 1 )
		{
			//printf( "CTQT::SlotRepaint() mpTarget<%p>\n", mpTarget );
			if( this->mpTarget )
			{
				auto& uiev = this->mUIEvent;
				auto gfxwin = uiev.mpGfxWin;
				auto vp = gfxwin ? gfxwin->GetRootWidget() : nullptr;

				//this->UIEvent->mpGfxWin = (GfxWindow*) this->mpTarget->FBI()->GetThisBuffer();
				ui::DrawEvent drwev( this->mpTarget );

				if( vp )
					vp->Draw(drwev);
			}
		}
		this->mDrawLock--;
		ork::PerfMarkerPush( "ork.viewport.draw.end" );
	};

	if( OpqTest::GetContext()->mOPQ == & MainThreadOpQ() )
	{
		// already on main Q
		lamb();
	}
	else
	{
		MainThreadOpQ().push(Op(lamb));
	}
}

} } //namespace ork::lev2

#endif
