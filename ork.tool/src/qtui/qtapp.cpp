////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <orktool/qtui/qtapp.h>

///////////////////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtmainwin.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/input/input.h>
#include <ork/lev2/gfx/dbgfontman.h>

#include <QtWidgets/QStyle>

// This include is relative to src/miniork which is temporarily added an a include search path.
// We'll need to come up with a long-term solution eventually.
//#include <test/platform_lev1/test_application.h>
//#include <application/ds/ds_application.h>

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/qtui/qtui.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/editor/edmainwin.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/thread.h>

#include <dispatch/dispatch.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

OrkQtApp::OrkQtApp( int argc, char** argv )
	: QApplication( argc, argv )
	, mpMainWindow(0)
{
	bool bcon = mIdleTimer.connect( & mIdleTimer, &QTimer::timeout, this, &OrkQtApp::OnTimer );
	assert(bcon);
	mIdleTimer.setInterval(5);
	mIdleTimer.setSingleShot(false);
	mIdleTimer.start();

}

///////////////////////////////////////////////////////////////////////////////

void OrkQtApp::OnTimer()
{
	OpqTest opqtest(&MainThreadOpQ());
	while(MainThreadOpQ().Process());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::tool

#include "qtapp.moc"