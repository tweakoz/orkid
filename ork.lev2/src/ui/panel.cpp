#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/panel.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>

namespace ork { namespace ui {

static const int kpanelw = 12;

/////////////////////////////////////////////////////////////////////////

Panel::Panel( const std::string & name, int x, int y, int w, int h )
	: Group(name,x,y,w,h)
	, mChild(nullptr)
	, mDockedAtTop(false)
{

}

Panel::~Panel()
{
	if( mChild )
	{	mChild->SetParent(nullptr);
		mChild = nullptr;
	}
	if( mParent )
		mParent->RemoveChild(this);
}

/////////////////////////////////////////////////////////////////////////

void Panel::SetChild( Widget* pch)
{
	mChild = pch;
	AddChild(pch);
}

/////////////////////////////////////////////////////////////////////////

void Panel::DoDraw(ui::DrawEvent& drwev)
{
	auto tgt = drwev.GetTarget();
	auto fbi = tgt->FBI();
	auto mtxi = tgt->MTXI();
	auto& primi = lev2::GfxPrimitives::GetRef();
	auto defmtl = lev2::GfxEnv::GetDefaultUIMaterial();

	lev2::SRasterState defstate;
	tgt->RSI()->BindRasterState( defstate );
	tgt->FXI()->InvalidateStateBlock();
	tgt->PushMaterial( defmtl );

	bool has_foc = HasMouseFocus();

	auto ren_quad = [&](int x,int y, int x2, int y2)
	{
		primi.RenderQuadAtZ(
			tgt,
			x, x2, 	// x0, x1
			y, y2, 	// y0, y1
			0.0f,			// z
			0.0f, 1.0f,		// u0, u1
			0.0f, 1.0f		// v0, v1
			);
	};
	auto ren_line = [&](int x,int y, int x2, int y2)
	{
		auto vb = & lev2::GfxEnv::GetSharedDynamicVB();
		lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
		vw.Lock( tgt, vb, 2 );
		vw.AddVertex( lev2::SVtxV12C4T16( x, y, 0.0f, 0.0f, 0.0f, 0xffffffff ) );
		vw.AddVertex( lev2::SVtxV12C4T16( x2, y2, 0.0f, 0.0f, 0.0f, 0xffffffff ) );
		vw.UnLock( tgt );
		tgt->GBI()->DrawPrimitive( vw, lev2::EPRIM_LINES );
	};

	mtxi->PushUIMatrix();
	{
		int ixr, iyr;
		LocalToRoot( 0, 0, ixr, iyr );

		/////////////
		// panel outline (resize/moving)
		/////////////

		fvec4 clr = fcolor4(1.0f,0.0f,1.0f,0.4f);
		if( has_foc )
			clr = fcolor4::White();

		defmtl->_rasterstate.SetBlending( lev2::EBLENDING_ALPHA );
		tgt->PushModColor( clr );
		ren_quad( ixr, iyr, ixr+miW, iyr+miH );
		tgt->PopModColor();
		defmtl->_rasterstate.SetBlending( lev2::EBLENDING_OFF );

		/////////////
		// close button
		/////////////

		LocalToRoot( mCloseX, mCloseY, ixr, iyr );
		tgt->PushModColor( fcolor4(0.3f,0.0f,0.0f) );
		ren_quad( ixr+1, iyr+1, ixr+kpanelw-1, iyr+kpanelw-1 );
		tgt->PopModColor();
		tgt->PushModColor( fcolor4(1.0f,0.3f,0.3f) );
		ren_quad( ixr+2, iyr+2, ixr+kpanelw-2, iyr+kpanelw-2 );
		tgt->PopModColor();
		tgt->PushModColor( fcolor4(0.3f,0.0f,0.0f) );
		ren_line( ixr+1, iyr+1, ixr+kpanelw-1, iyr+kpanelw-1 );
		ren_line( ixr+kpanelw-1, iyr+1, ixr+1, iyr+kpanelw-1 );
		tgt->PopModColor();


	}
	mtxi->PopUIMatrix();
	tgt->PopMaterial();

	if( mChild )
		mChild->Draw(drwev);

}

/////////////////////////////////////////////////////////////////////////

void Panel::DoLayout()
{
	mDockedAtTop = (miY==-kpanelw);
	printf( "mDockedAtTop<%d>\n", int(mDockedAtTop) );

	mCloseX = kpanelw;
	mCloseY = mDockedAtTop ? miH-kpanelw : 0;

	int cw = miW-(kpanelw*2);
	int ch = miH-(kpanelw*2);

	//printf( "Panel<%s>::DoLayout x<%d> y<%d> w<%d> h<%d>\n", msName.c_str(), miX, miY, miW, miH );
	if( mChild )
	{
		mChild->SetRect(kpanelw,kpanelw,cw,ch);
	}
}

/////////////////////////////////////////////////////////////////////////

HandlerResult Panel::DoRouteUiEvent( const Event& Ev )
{
	//printf( "Panel::DoRouteUiEvent mPanelUiState<%d>\n", mPanelUiState );

	if( mChild && mChild->IsEventInside(Ev) && mPanelUiState==0 )
	{
		HandlerResult res = mChild->RouteUiEvent(Ev);
		if( res.mHandler != nullptr )
			return res;	
	}
	return OnUiEvent(Ev);

}

/////////////////////////////////////////////////////////////////////////

static int idownx = 0;
static int idowny = 0;
static int iprevpx = 0;
static int iprevpy = 0;
static int iprevpw = 0;
static int iprevph = 0;

void Panel::Snap()
{
	if( nullptr == mParent)
		return;

	int x2 = GetX2();
	int pw = mParent->GetW();
	int xd = abs(x2-pw);
	int y2 = GetY2();
	int ph = mParent->GetH();
	int yd = abs(y2-ph);
	//printf( "x2<%d> pw<%d> xd<%d>\n", x2, pw, xd );
	//printf( "y2<%d> ph<%d> yd<%d>\n", y2, ph, yd );
	bool snapl = ( miX<kpanelw );
	bool snapr = ( xd<kpanelw );
	bool snapt = ( miY<kpanelw );
	bool snapb = ( yd<kpanelw );
	if( snapt&&snapb )
	{	SetY(-kpanelw);
		SetH(ph+2*kpanelw);
	}
	else if( snapt )
		SetY(-kpanelw);
	else if( snapb )
		SetY(ph+kpanelw-GetH());
	if( snapl&&snapr )
	{	SetX(-kpanelw);
		SetW(pw+2*kpanelw);
	}
	if( snapl )
		SetX(-kpanelw);
	else if( snapr )
		SetX(pw+kpanelw-GetW());

}

HandlerResult Panel::DoOnUiEvent( const Event& Ev )
{
	HandlerResult ret(this);

	int evx = Ev.miX;
	int evy = Ev.miY;
	//printf( "Panel<%p>::OnUiEvent isshift<%d>\n", this, int(isshift) );
	//////////////////////////////
	int ilocx = 0;
	int ilocy = 0;
	RootToLocal(evx,evy,ilocx,ilocy);
	//////////////////////////////
	const auto& filtev = Ev.mFilteredEvent;
	switch( filtev.miEventCode )
	{
		case ui::UIEV_PUSH: // idle
			idownx = evx;
			idowny = evy;
			iprevpx = miX;
			iprevpy = miY;
			iprevpw = miW;
			iprevph = miH;
			ret.mHoldFocus = true;
			if( filtev.mBut0 )
			{
				printf( "ilocx<%d> mCloseX<%d>\n", ilocx, mCloseX );
				if(    (ilocx>=mCloseX)
					&& ((ilocx-mCloseX)<kpanelw)
					&& (ilocy>=mCloseY)
					&& ((ilocy-mCloseY)<kpanelw)
					)
				{
					auto lamb = [=]() 
					{	delete this;
					};
					opq::Op(lamb).QueueASync(opq::mainSerialQueue());

				}
				else
					mPanelUiState = 1;
			}
			else if( filtev.mBut1||filtev.mBut2 )
			{
				if( abs(ilocy)<kpanelw ) // top
					mPanelUiState = 2;
				else if( abs(ilocy-miH)<kpanelw ) // bot
					mPanelUiState = 3;
				else if( abs(ilocx)<kpanelw ) // lft
					mPanelUiState = 4;
				else if( abs(ilocx-miW)<kpanelw ) // rht
					mPanelUiState = 5;
			}
			break;
		case ui::UIEV_RELEASE: // idle
			ret.mHoldFocus = false;

			if( mPanelUiState ) // moving or sizing w
			{
				Snap();
			}
			mPanelUiState = 0;

			break;
		case ui::UIEV_DRAG:
			ret.mHoldFocus = true;
			break;
		default:
			break;
	}

	int dx = evx-idownx;
	int dy = evy-idowny;

	switch( mPanelUiState )
	{
		case 0:
			break;
		case 1: // move
			SetPos(iprevpx + dx,iprevpy + dy);
			break;
		case 2: // resize h
			SetRect(iprevpx,iprevpy+dy,iprevpw,iprevph-dy);
			break;
		case 3: // resize w
			SetSize(iprevpw,iprevph+dy);
			break;
		case 4:
			SetRect(iprevpx+dx,iprevpy,iprevpw-dx,iprevph);
			break;
		case 5:
			SetSize(iprevpw+dx,iprevph);
			break;
	}

	return ret;
}
//

void Panel::DoOnEnter()
{

}

void Panel::DoOnExit()
{

}

}} // namespace ork { namespace ui {


