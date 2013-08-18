#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/panel.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>

namespace ork { namespace ui {

/////////////////////////////////////////////////////////////////////////

Panel::Panel( const std::string & name, int x, int y, int w, int h )
	: Widget(name,x,y,w,h)
	, mChild(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////

void Panel::SetChild( Widget* pch)
{
	mChild = pch;
	mChild->SetParent(this);
}

/////////////////////////////////////////////////////////////////////////

void Panel::DoDraw(ui::DrawEvent& drwev)
{
	auto tgt = drwev.GetTarget();
	auto fbi = tgt->FBI();
	auto mtxi = tgt->MTXI();
	auto& primi = lev2::CGfxPrimitives::GetRef();
	auto defmtl = lev2::GfxEnv::GetDefaultUIMaterial();

	//lev2::GfxMaterialUI UiMat(tgt);
	lev2::SRasterState defstate;
	tgt->RSI()->BindRasterState( defstate );
	tgt->FXI()->InvalidateStateBlock();
	tgt->PushMaterial( defmtl );

	bool has_foc = HasMouseFocus();
	tgt->PushModColor( has_foc?CColor4::White():CColor4::Red() );
	mtxi->PushUIMatrix();
	{
		int ix_root = 0;
		int iy_root = 0;
		LocalToRoot( 0, 0, ix_root, iy_root );

		primi.RenderQuadAtZ(
			tgt,
			ix_root, ix_root+miW, 	// x0, x1
			iy_root, iy_root+miH, 	// y0, y1
			0.0f,			// z
			0.0f, 1.0f,		// u0, u1
			0.0f, 1.0f		// v0, v1
			);
	}
	mtxi->PopUIMatrix();
	tgt->PopModColor();
	tgt->PopMaterial();

	if( mChild )
		mChild->Draw(drwev);


	//fbi->PopViewport();
	//fbi->PopScissor();
}

static const int kpanelw = 12;
/////////////////////////////////////////////////////////////////////////

void Panel::DoLayout()
{
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

int idownx = 0;
int idowny = 0;
int iprevpx = 0;
int iprevpy = 0;
int iprevpw = 0;
int iprevph = 0;

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
				mPanelUiState = 1;
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
				int x2 = GetX2();
				int pw = mParent->GetW();
				int xd = abs(x2-pw);
				int y2 = GetY2();
				int ph = mParent->GetH();
				int yd = abs(y2-ph);
				printf( "x2<%d> pw<%d> xd<%d>\n", x2, pw, xd );
				printf( "y2<%d> ph<%d> yd<%d>\n", y2, ph, yd );
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


