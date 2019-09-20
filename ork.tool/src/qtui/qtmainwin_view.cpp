////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <orktool/qtui/qtmainwin.h>
#include <orktool/qtui/qtconsole.h>
#include <orktool/toolcore/dataflow.h>
#include <orktool/qtui/qtdataflow.h>

#include "qtui_scenevp.h"
#include <pkg/ent/editor/edmainwin.h>
#include <ork/lev2/ui/panel.h>
#include <ork/lev2/ui/split_panel.h>
#include "outliner2.h"
#include <QtWidgets/QDockWidget>
#include <QtCore/QProcess>

#include <string>

using namespace std;

namespace ork { namespace tool {
    const std::string& getExecutableDir();
}}
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

static SceneEditorVP* gpvp = 0;


QWidget * EditorMainWindow::NewCamView( bool bfloat )
{
	static int viewnum = 0;
	std::string viewname = CreateFormattedString( "Camera:%d", viewnum+1 );

	lev2::CQtGfxWindow* pgfxwin = new lev2::CQtGfxWindow( nullptr );
	lev2::GfxEnv::GetRef().RegisterWinContext(pgfxwin);

	gpvp = new SceneEditorVP( "SceneViewport", mEditorBase, *this );
	pgfxwin->mRootWidget = gpvp;

	lev2::CTQT* pctqt = new lev2::CTQT( pgfxwin, this );

	pctqt->Show();

	pctqt->GetQWidget()->Enable();

	viewnum++;

	gpvp->Init();

	//////////////////////////////////////////////

	return pctqt->GetQWidget();

}

///////////////////////////////////////////////////////////////////////////


void EditorMainWindow::SceneObjPropEdit()
{
	mGedModelObj.Attach( 0 );

	int mainwin_w = width();
	int mainwin_h = height();
	int df_w = 384;
	int df_h = 768;
	int df_x = 0;
	int df_y = 192;

	auto pnl = new ui::SplitPanel( "ged.panel", df_x,df_y,df_w,df_h );
	auto pvp1 = new Outliner2View(mEditorBase);
	auto pvp2 = new tool::ged::GedVP( "props.vp", mGedModelObj );
	pnl->SetChild1(pvp1);
	pnl->SetChild2(pvp2);
	gpvp->AddChild(pnl);
	pnl->Snap();

	/////////////////////////////////////////////////////////////////////////////////////
	//
	/////////////////////////////////////////////////////////////////////////////////////
	object::Connect(	& mGedModelObj.GetSigPreNewObject(),
						& mEditorBase.GetSlotPreNewObject() );
	//////
	object::Connect(	& mGedModelObj.GetSigModelInvalidated(),
						& mEditorBase.GetSlotModelInvalidated() );
	//////
	object::Connect(	& mGedModelObj.GetSigNewObject(),
						& mEditorBase.GetSlotNewObject() );
	//////
	object::Connect(	& mGedModelObj.GetSigSpawnNewGed(),
						& this->GetSlotSpawnNewGed() );
	//////
	object::Connect(	& mEditorBase.GetSigObjectDeleted(),
						& mGedModelObj.GetSlotObjectDeleted() );
	//////
	object::Connect(	& mEditorBase.selectionManager().GetSigObjectSelected(),
						& mGedModelObj.GetSlotObjectSelected() );
	//////
	object::Connect(	& mEditorBase.selectionManager().GetSigObjectDeSelected(),
						& mGedModelObj.GetSlotObjectDeSelected() );
	/////////////////////////////////////////////////////////////////////////////////////
	//
	/////////////////////////////////////////////////////////////////////////////////////
}

void EditorMainWindow::NewOutliner2View()
{
}

///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::SlotSpawnNewGed( ork::Object* pobj )
{
	dataflow::graph_data* dflow_graph = rtti::autocast(pobj);
	bool is_dflow = (dflow_graph!=nullptr);

	rtti::Class* pclass = pobj->GetClass();
	std::string classname = pclass->Name().c_str();
	std::string viewname = CreateFormattedString( "%s:%p", classname.c_str(), pobj );

	ork::tool::ged::ObjModel* pnewobjmodel = new ork::tool::ged::ObjModel;
	pnewobjmodel->Attach( 0 );

	auto pvp = new tool::ged::GedVP( "props.vp", *pnewobjmodel );
	pvp->GetGedWidget().SetDeleteModel(true);

	ui::Widget* pnlw = nullptr;

	int mainwin_w = width();
	int mainwin_h = height();

	if( is_dflow )
	{

		auto pvpdf = new ork::tool::GraphVP(mDataflowEditor,*pnewobjmodel,"yo");

		int df_w = mainwin_w/4;
		int df_h = mainwin_h-256;
		int df_x = mainwin_w-df_w;
		int df_y = mainwin_h/2-(df_h/2);

		auto pnl = new ui::SplitPanel( "ged.panel", df_x,df_y,df_w,df_h );
		pnl->SetChild1(pvpdf);
		pnl->SetChild2(pvp);
		pnl->EnableCloseButton();

		gpvp->AddChild(pnl);

		pnl->Snap();
		pnlw = pnl;

		mDataflowEditor.Attach( dflow_graph );

		pvpdf->ReCenter();


	}
	else
	{
		int df_w = mainwin_w/3;
		int df_h = mainwin_h/2;
		int df_x = mainwin_w/2-(df_w/2);
		int df_y = mainwin_h-df_h;

		auto pnl = new ui::Panel( "ged.panel", df_x,df_y,df_w,df_h );
		pnl->SetChild(pvp);
		gpvp->AddChild(pnl);
		pnl->Snap();
		pnlw = pnl;
	}

	pnewobjmodel->SetChoiceManager( & mEditorBase.mChoiceMan );

	pnewobjmodel->GetPersistMapContainer().CloneFrom( mGedModelObj.GetPersistMapContainer() );

	/////////////////////////////////////////////////////////////////////////////////////
	//
	/////////////////////////////////////////////////////////////////////////////////////
	object::Connect(	& mEditorBase.GetSigObjectDeleted(),
						& pnewobjmodel->GetSlotObjectDeleted() );
	//////
	object::Connect(	& mGedModelObj.GetSigModelInvalidated(),
						& pnewobjmodel->GetSlotRelayModelInvalidated() );
	//////
	object::Connect(	& mGedModelObj.GetSigPropertyInvalidated(),
						& pnewobjmodel->GetSlotRelayPropertyInvalidated() );
	//////
	object::Connect(	& pnewobjmodel->GetSigPreNewObject(),
						& mEditorBase.GetSlotPreNewObject() );
	//////
	object::Connect(	& pnewobjmodel->GetSigModelInvalidated(),
						& mEditorBase.GetSlotModelInvalidated() );
	//////
	object::Connect(	& pnewobjmodel->GetSigModelInvalidated(),
						& mGedModelObj. GetSlotRelayModelInvalidated() );
	//////
	object::Connect(	& pnewobjmodel->GetSigPropertyInvalidated(),
						& mGedModelObj. GetSlotRelayPropertyInvalidated() );
	//////////////////////////////////////////////
	// lets not allow spawn of spawn yet.....
	//////////////////////////////////////////////
	object::Connect(	& pnewobjmodel->GetSigSpawnNewGed(),
						& this->GetSlotSpawnNewGed() );
	/////////////////////////////////////////////////////////////////////////////////////
	//
	/////////////////////////////////////////////////////////////////////////////////////
	pnewobjmodel->Attach( pobj );

}

///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::NewAssetAssist()
{
    auto p = new QProcess();
    auto edir = tool::getExecutableDir();
    auto bdir = tool::getDataDir();
    printf("bdir<%s>\n", bdir.c_str());
    p->setProgram(qs(edir+"/ork.assetassistant.py"));
    p->setWorkingDirectory(qs(bdir));
    p->start();
}

QDockWidget *EditorMainWindow::NewPyConView(bool bfloat)
{

	static int viewnum = 0;
	viewnum++;
	auto area = Qt::BottomDockWidgetArea;

	std::string viewname = CreateFormattedString( "OrkidPythonConsole:%d", viewnum );
	QDockWidget*gfxdock = new QDockWidget(tr(viewname.c_str()), this);
	gfxdock->setFloating( bfloat );
	gfxdock->setAllowedAreas(area);
	gfxdock->setAutoFillBackground(false);
	gfxdock->setObjectName(viewname.c_str());
	//mGedModelObj.Attach( 0 );

    if( 1 )
    {
    	auto pvp = new tool::vp_cons( viewname );
    	auto pgfxwin = new lev2::CQtGfxWindow( pvp );
    	auto pctqt = new lev2::CTQT( pgfxwin, gfxdock );
    	auto pqw = pctqt->GetQWidget();
    	gfxdock->setWidget( pqw );
    	pctqt->Show();
    	pctqt->GetQWidget()->Enable();
    	pvp->BindCTQT( pctqt );
    }
    else
    {
    	auto owin = new ork::tool::QtConsoleWindow(bfloat,0);
    	gfxdock->setWidget( owin->GroupBox() );
    }
    addDockWidget(area, gfxdock);
    gfxdock->setMinimumSize( 480, 256 );
    gfxdock->resize( 480, 240 );
    gfxdock->show();
	return gfxdock;
}
#if 0
///////////////////////////////////////////////////////////////////////////

QDockWidget *EditorMainWindow::NewToolView( bool bfloat )
{
	static int viewnum = 0;
	std::string viewname = CreateFormattedString( "Tool:%d", viewnum+1 );
	viewnum++;

	QDockWidget*gfxdock = new QDockWidget(tr(viewname.c_str()), this);
	gfxdock->setFloating( bfloat );
	gfxdock->setAllowedAreas(Qt::AllDockWidgetAreas );
	gfxdock->setAutoFillBackground(false);
	gfxdock->setObjectName(viewname.c_str());

	//mQedModelTool.Attach( 0 );
	//gfxdock->setWidget( mQedModelTool.GetQedWidget() );

	gfxdock->setMinimumSize( 500, 100 );
	addDockWidget(Qt::LeftDockWidgetArea, gfxdock);

	gfxdock->show();
	gfxdock->resize( 300, 500 );

	return gfxdock;
}
#endif
///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::NewDirView()
{/*
	static int viewnum = 0;
	std::string viewname = CreateFormattedString( "Dir:%d", viewnum+1 );
	viewnum++;

	QDockWidget*gfxdock = new QDockWidget(tr(viewname.c_str()), this);
	gfxdock->setFloating( true );
	gfxdock->setAllowedAreas(Qt::AllDockWidgetAreas );
	gfxdock->setAutoFillBackground(false);

	QTreeView *list = new QTreeView( gfxdock );
	list->setModel(mDirModel);
	list->setRootIndex(mDirModel->index("v:\\projects\\superman\\files\\"));

	list->resize( 150, 150 );

	gfxdock->setWidget( list );
	gfxdock->setMinimumSize( 100, 100 );
	addDockWidget(Qt::LeftDockWidgetArea, gfxdock);

	gfxdock->show();*/
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
