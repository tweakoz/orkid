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
#include "vpSceneEditor.h"
extern bool gbheadlight;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
//Moc.AddSlot0( "LightingHeadLightMode()", & EditorMainWindow::LightingHeadLightMode );
//Moc.AddSlot0( "LightingSimulationMode()", & EditorMainWindow::LightingSceneMode );
//Moc.AddSlot0( "LightingSetLightPos()", & EditorMainWindow::LightingSetLightPos );
//QAction * BakeLightsAct = new QAction(tr("BakeLighting..."), this);
//BakeLightsAct->setStatusTip(tr("Bake static lighting"));
//connect(BakeLightsAct, SIGNAL(triggered()), this, SLOT(BakeLighting()));
//QAction * SavCubMapAct = new QAction(tr("SaveCubeMap..."), this);
//SavCubMapAct->setStatusTip(tr("Create a CubeMap at the camera target locator"));
//connect(SavCubMapAct, SIGNAL(triggered()), this, SLOT(SaveCubeMap()));
//viewMenu->addAction(SavCubMapAct);
///////////////////////////////////////////////////////////////////////////////
class LightingModule : public tool::EditorModule
{
	void OnAction( const char* pact ) final;
	void Activate( QMenuBar* qmb ) final;
	void DeActivate( QMenuBar* qmb ) final;

	EditorMainWindow& mEditWin;

public:
	LightingModule(EditorMainWindow& emw)
		: mEditWin( emw )
	{
	}
};
///////////////////////////////////////////////////////////////////////////////
void LightingModule::Activate( QMenuBar* qmb )
{
	mMenuBar = qmb;
	
	AddAction( "/Lighting/Test" );

}
///////////////////////////////////////////////////////////////////////////////
void LightingModule::DeActivate( QMenuBar* qmb )
{
	OrkAssert( qmb==mMenuBar );
	mMenuBar = 0;
}
///////////////////////////////////////////////////////////////////////////////
void LightingModule::OnAction( const char* pact )
{
	if( 0 == strcmp( "/Lighting/Test", pact ) )	{ orkprintf("yo\n"); }
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::LightingSetLightPos()
{
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::LightingHeadLightMode()
{
	gbheadlight = true;
	//mpActiveEditVP->SetHeadLightMode( true );
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::LightingSceneMode()
{
	gbheadlight = false;
	//mpActiveEditVP->SetHeadLightMode( false );
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::SaveCubeMap()
{
	//if( mpActiveEditVP )
	{
		//mpActiveEditVP->SaveCubeMap();
	}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RegisterLightingModule( EditorMainWindow& emw )
{
	emw.ModuleMgr().AddModule( "Lighting", new LightingModule( emw ) );
}
///////////////////////////////////////////////////////////////////////////////
/*
void EditorLightManager::GetStationaryLights( const Frustum& frustum, lev2::LightContainer& container )
{
	for( orklut<float,lev2::Light*>::const_iterator	it =  mGlobalStationaryLights.mPrioritizedLights.begin();
													it != mGlobalStationaryLights.mPrioritizedLights.end();
													it++ )
	{
		float prio			= it->first;
		lev2::Light* plight	= it->second;

		if( plight->IsInFrustum( frustum ) )
		{
			container.AddLight( plight );
		}

	}
}
void EditorLightManager::GetMovingLights( const Frustum& frustum, lev2::LightContainer& container )
{
	for( orklut<float,lev2::Light*>::const_iterator	it =  mGlobalMovingLights.mPrioritizedLights.begin();
													it != mGlobalMovingLights.mPrioritizedLights.end();
													it++ )
	{
		float prio			= it->first;
		lev2::Light* plight	= it->second;

		if( plight->IsInFrustum( frustum ) )
		{
			container.AddLight( plight );
		}

	}
}*/
}}
///////////////////////////////////////////////////////////////////////////////
