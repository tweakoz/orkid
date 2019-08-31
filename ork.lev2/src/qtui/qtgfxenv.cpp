////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/ctxbase.h>
//
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/camera/cameraman.h>
#include <ork/lev2/qtui/qtui.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/widget.h>

#if ! defined (_CYGWIN) 

///////////////////////////////////////////////////////////////////////////////

namespace ork {
namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

CQtGfxWindow::CQtGfxWindow( ui::Widget* prw )
	: GfxWindow( 0, 0, 640, 448, "yo" ) 
	, mbinit( true )
	, mRootWidget( prw )
{
}

CQtGfxWindow::~CQtGfxWindow()
{
	if( mRootWidget )
	{
		delete mRootWidget;
	}
}

///////////////////////////////////////////////////////////////////////////////

void CQtGfxWindow::Draw( void )
{
	int ix = GetContextX();
	int iy = GetContextY();
	int iw = GetContextW();
	int ih = GetContextH();
	mRootWidget->SetRect( ix,iy,iw,ih );

	ui::DrawEvent drwev( GetContext() );
	mRootWidget->Draw( drwev );
}

///////////////////////////////////////////////////////////////////////////////

void CQtGfxWindow::GotFocus( void )
{
	if( mRootWidget )
	{
		mRootWidget->GotKeyboardFocus();

		ui::Event uievent;
		uievent.mEventCode = ork::ui::UIEV_GOT_KEYFOCUS;
		mRootWidget->HandleUiEvent( uievent );
	}
	mbHasFocus = true;
}

///////////////////////////////////////////////////////////////////////////////

void CQtGfxWindow::LostFocus( void )
{
	if( mRootWidget )
	{
		mRootWidget->LostKeyboardFocus();

		ui::Event uievent;
		uievent.mEventCode = ork::ui::UIEV_LOST_KEYFOCUS;
		mRootWidget->HandleUiEvent( uievent );
	}
	mbHasFocus = false;
}

///////////////////////////////////////////////////////////////////////////////

void CQtGfxWindow::OnShow()
{
	ork::lev2::GfxTarget *pTARG = GetContext();

	//if( mbinit )
	{
		//mbinit = false;
		/////////////////////////////////////////////////////////////////////
		int ix = GetContextX();
		int iy = GetContextY();
		int iw = GetContextW();
		int ih = GetContextH();
		mRootWidget->SetRect( ix,iy,iw,ih );
		SetRootWidget( mRootWidget );
	}
}

} // namespace tool
} // namespace ork

#endif

