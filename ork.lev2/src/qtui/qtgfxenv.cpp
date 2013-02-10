////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/input/input.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/camera/cameraman.h>
#include <ork/lev2/qtui/qtui.h>

#if ! defined (_CYGWIN) 

///////////////////////////////////////////////////////////////////////////////

namespace ork {
namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

CQtGfxWindow::CQtGfxWindow( ork::lev2::CUIViewport *pVP )
	: GfxWindow( 0, 0, 640, 448, "yo" ) 
	, mbinit( true )
	, mpviewport( pVP )
{
}

CQtGfxWindow::~CQtGfxWindow()
{
	if( mpviewport )
	{
		delete mpviewport;
	}
}

///////////////////////////////////////////////////////////////////////////////

void CQtGfxWindow::Draw( void )
{
	mpviewport->SetX( GetContextX() );		// TODO add resize call
	mpviewport->SetY( GetContextY() );
	mpviewport->SetW( GetContextW() );
	mpviewport->SetH( GetContextH() );

	DrawEvent drwev( GetContext() );
	mpviewport->Draw( drwev );
}

///////////////////////////////////////////////////////////////////////////////

void CQtGfxWindow::GotFocus( void )
{
	if( mpviewport )
	{
		mpviewport->GotKeyboardFocus();

		ork::lev2::CUIEvent uievent;
		uievent.mEventCode = ork::lev2::UIEV_GOT_KEYFOCUS;
		mpviewport->UIEventHandler( & uievent );
	}
	mbHasFocus = true;
}

///////////////////////////////////////////////////////////////////////////////

void CQtGfxWindow::LostFocus( void )
{
	if( mpviewport )
	{
		mpviewport->LostKeyboardFocus();

		ork::lev2::CUIEvent uievent;
		uievent.mEventCode = ork::lev2::UIEV_LOST_KEYFOCUS;
		mpviewport->UIEventHandler( & uievent );
	}
	mbHasFocus = false;
}

} // namespace tool
} // namespace ork

#endif

