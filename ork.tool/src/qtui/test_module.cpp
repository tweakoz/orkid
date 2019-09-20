////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/lev2/qtui/qtui.hpp>
#include <orktool/qtui/qtmainwin.h>
///////////////////////////////////////////////////////////////////////////
#include <pkg/ent/editor/edmainwin.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
void EditorMainWindow::ViewToggleCollisionSpheres()
{
	static bool bviewcs = true;

	OldSchool::SetGlobalIntVariable( "ViewCollisionSpheres", int(bviewcs) );

	bviewcs = bviewcs ? false : true;
}

////////////////////////////////////
//QAction * TglSphAct = new QAction(tr("ToggleCollisionSpheres..."), this);
//TglSphAct->setStatusTip(tr("Toggle the visibility of collision spheres"));
//connect(TglSphAct, SIGNAL(triggered()), this, SLOT(ViewToggleCollisionSpheres()));
////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
