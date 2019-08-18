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
#include <orktool/toolcore/FunctionManager.h>
#include <QtWidgets/qmessagebox.h>
#include <ork/util/hotkey.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/debug.h>
#include <QtCore/QSettings>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////
class MainWinDefaultModule : public tool::EditorModule
{
	void OnAction( const char* pact ) final;
	void Activate( QMenuBar* qmb ) final;
	void DeActivate( QMenuBar* qmb ) final;

	EditorMainWindow& mEditWin;

public:
	MainWinDefaultModule(EditorMainWindow& emw)
		: mEditWin( emw )
	{
	}
};
///////////////////////////////////////////////////////////////////////////////
void MainWinDefaultModule::Activate( QMenuBar* qmb )
{
	mMenuBar = qmb;

	AddAction( "/Scene/NewScene"  ,QKeySequence(Qt::CTRL + Qt::Key_N) );
	AddAction( "/Scene/OpenScene" ,QKeySequence(Qt::CTRL + Qt::Key_O));
	AddAction( "/Scene/SaveScene" ,QKeySequence(Qt::CTRL + Qt::Key_S));
	AddAction( "/Scene/ExportArchetype" );
	AddAction( "/Scene/ImportArchetype" );

	AddAction( "/View/PyCon",QKeySequence(Qt::CTRL + Qt::Key_P));
    AddAction( "/View/AssetAssist",QKeySequence(Qt::CTRL + Qt::Key_A)  );
	//AddAction( "/View/Outliner" );
	//AddAction( "/View/Outliner2" );
	//AddAction( "/View/DataflowEditor" );
	//AddAction( "/View/ToolEditor" );
	//AddAction( "/View/PropEditor" );
	AddAction( "/View/SaveLayout" );
	AddAction( "/View/LoadLayout" );

	AddAction( "/Game/Local/Run", QKeySequence(Qt::CTRL + Qt::Key_Period) );
	AddAction( "/Game/Local/Stop", QKeySequence(tr("Ctrl+,")) );

	AddAction( "/Entity/New Entity" ,QKeySequence(Qt::CTRL + Qt::Key_Comma) );
	AddAction( "/Entity/New Entities..." ,QKeySequence(tr("Ctrl+Shift+E")) );
	AddAction( "/Entity/Group" );

	AddAction( "/&Archetype/E&xport" ,QKeySequence(tr("Ctrl+Shift+X")) );
	AddAction( "/&Archetype/I&mport" ,QKeySequence(tr("Ctrl+Shift+M")) );
	//AddAction( "/&Archetype/Make &Referenced" ,QKeySequence(tr("Ctrl+Shift+R")) );
	//AddAction( "/&Archetype/Make &Local" ,QKeySequence(tr("Ctrl+Shift+L")) );

	AddAction( "/Refresh/Models" );
	AddAction( "/Refresh/Anims" );
	AddAction( "/Refresh/Textures" );
	AddAction( "/Refresh/Chsms" );

    AddAction( "/Project/SetProjectFolder" ,QKeySequence(tr("Ctrl+Shift+P")) );

	//mEditWin.NewToolView(false);
}
///////////////////////////////////////////////////////////////////////////////
void MainWinDefaultModule::DeActivate( QMenuBar* qmb )
{
	OrkAssert( qmb==mMenuBar );
	mMenuBar = 0;
}
///////////////////////////////////////////////////////////////////////////////
void MainWinDefaultModule::OnAction( const char* pact )
{
	     if( 0 == strcmp( "/Scene/NewScene", pact ) )			{	mEditWin.NewScene();		}
	else if( 0 == strcmp( "/Scene/OpenScene", pact ) )			{	mEditWin.OpenSceneFile();	}
	else if( 0 == strcmp( "/Scene/ExportArchetype", pact))		{	mEditWin.SaveSelected();	}
	else if( 0 == strcmp( "/Scene/ImportArchetype", pact ) )	{	mEditWin.MergeFile();		}
	///////////////////////////////////////////////////////
//	else if( 0 == strcmp( "/View/Outliner",pact) )				{	mEditWin.NewOutlinerView(false); }
//	else if( 0 == strcmp( "/View/Outliner2",pact) )				{	mEditWin.NewOutliner2View(false); }
//	else if( 0 == strcmp( "/View/PyCon",pact) )					{	mEditWin.NewPyConView(true); }
//    else if( 0 == strcmp( "/View/AssetAssist",pact) )           {   mEditWin.NewAssetAssist(); }
//	else if( 0 == strcmp( "/View/DataflowEditor",pact) )		{	mEditWin.NewDataflowView(); }
//	else if( 0 == strcmp( "/View/ToolEditor",pact) )			{	mEditWin.NewToolView(false); }
	else if( 0 == strcmp( "/View/SaveLayout",pact) )			{	mEditWin.SaveLayout(); }
	else if( 0 == strcmp( "/View/LoadLayout",pact) )			{	mEditWin.LoadLayout(); }
	///////////////////////////////////////////////////////
	else if( 0 == strcmp( "/Game/Local/Run",pact) )				{	mEditWin.RunLocal(); }
	else if( 0 == strcmp( "/Game/Local/Stop",pact) )			{	mEditWin.StopLocal(); }
	///////////////////////////////////////////////////////
	else if( 0 == strcmp( "/Entity/New Entity",pact) )			{	mEditWin.NewEntity(); }
	else if( 0 == strcmp( "/Entity/New Entities...",pact) )		{	mEditWin.NewEntities(); }
	else if( 0 == strcmp( "/Entity/Group",pact) )				{	mEditWin.Group(); }
	///////////////////////////////////////////////////////
	else if( 0 == strcmp( "/&Archetype/E&xport",pact) )			{	mEditWin.ArchExport(); }
	else if( 0 == strcmp( "/&Archetype/I&mport",pact) )			{	mEditWin.ArchImport(); }
	//else if( 0 == strcmp( "/&Archetype/Make &Referenced",pact) ){	mEditWin.ArchMakeReferenced(); }
	//else if( 0 == strcmp( "/&Archetype/Make &Local",pact) )		{	mEditWin.ArchMakeLocal(); }
	///////////////////////////////////////////////////////
	else if( 0 == strcmp( "/Refresh/Models",pact) )				{	mEditWin.RefreshModels(); }
	else if( 0 == strcmp( "/Refresh/Anims",pact) )				{	mEditWin.RefreshAnims(); }
	else if( 0 == strcmp( "/Refresh/Textures",pact) )			{	mEditWin.RefreshTextures(); }
////////////////////////////////////////////////////////////////////////////////////////////////
	else if( 0 == strcmp( "/Scene/SaveScene", pact ) )	{
		mEditWin.SaveSceneFile();

	}
    else if( 0 == strcmp( "/Project/SetProjectFolder", pact ) )  {

        //auto current = qs(tool::getDataDir());
        //QString newdir = QFileDialog::getExistingDirectory(NULL, "Select Project Root", current, QFileDialog::ShowDirsOnly );
        //tool::setDataDir(newdir.toStdString());
    }

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RegisterMainWinDefaultModule( EditorMainWindow& emw )
{
	emw.ModuleMgr().AddModule( "Default", new MainWinDefaultModule( emw ) );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::NewScene()
{
	mEditorBase.QueueOpASync(NewSceneReq());
	SetSceneFile(QString("UNTITLED"));
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::RunLocal()
{
	mEditorBase.QueueOpASync(RunLocalReq());
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::StopLocal()
{
	mEditorBase.QueueOpASync(StopLocalReq());
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::RunGame()
{
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::RunLevel()
{
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::RefreshAnims()
{
	mEditorBase.EditorRefreshAnims();
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::RefreshModels()
{
	mEditorBase.EditorRefreshModels();
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::RefreshHFSMs()
{
	//mEditorBase.EditorRefreshHFSMs();
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::RefreshTextures()
{
	mEditorBase.EditorRefreshTextures();
}

}}
///////////////////////////////////////////////////////////////////////////////
