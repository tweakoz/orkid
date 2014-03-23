////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <orktool/qtui/qtmainwin.h>
#include <orktool/qtui/qtconsole.h>
#include <ork/lev2/gfx/testvp.h>
#include <orktool/toolcore/dataflow.h>
#include <orktool/qtui/qtdataflow.h>

#include <pkg/ent/editor/qtui_scenevp.h>
#include <pkg/ent/editor/edmainwin.h>
#include <ork/lev2/ui/panel.h>
#include <ork/lev2/ui/split_panel.h>
#include "outliner2.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

static SceneEditorVP* gpvp = 0;

QDockWidget * EditorMainWindow::NewCamView( bool bfloat )
{
	static int viewnum = 0;
	std::string viewname = CreateFormattedString( "Camera:%d", viewnum+1 );

	QDockWidget*gfxdock = new QDockWidget(tr(viewname.c_str()), this);
	gfxdock->setFloating( bfloat );
	gfxdock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	gfxdock->setAutoFillBackground(false); 
	gfxdock->setObjectName(viewname.c_str());

	//////////////
	// had to fix a deadlock by passing in nullptr to CQtGfxWindow
	//  and setting mRootWidget after the SceneEditorVP constructor
	//  the deadlock was wiating for a loader target to be instantiated
	// in lev2::TexLoader::LoadFileAsset
	//////////////

	lev2::CQtGfxWindow* pgfxwin = new lev2::CQtGfxWindow( nullptr );
	lev2::GfxEnv::GetRef().RegisterWinContext(pgfxwin);
	lev2::GfxEnv::GetRef().SetLoaderTarget( pgfxwin->GetContext() );

	gpvp = new SceneEditorVP( "SceneViewport", mEditorBase, *this );
	pgfxwin->mRootWidget = gpvp;

	lev2::CTQT* pctqt = new lev2::CTQT( pgfxwin, gfxdock );

	gfxdock->setWidget( pctqt->GetQWidget() );
	gfxdock->setMinimumSize( 100, 100 );
	//addDockWidget(Qt::NoDockWidgetArea, gfxdock);

	pctqt->Show();

	pctqt->GetQWidget()->Enable();

	viewnum++;



	gpvp->Init();
	//////////////////////////////////////////////

	bool bconOK = object::Connect(	& mEditorBase.SelectionManager(), AddPooledLiteral("SigObjectSelected"),
									& gpvp->SceneEditorView(), AddPooledLiteral("SlotObjectSelected") );
	OrkAssert(bconOK);
	
	    bconOK = object::Connect(	& mEditorBase.SelectionManager(), AddPooledLiteral("SigObjectDeSelected"),
									& gpvp->SceneEditorView(), AddPooledLiteral("SlotObjectDeSelected") );
	OrkAssert(bconOK);
	
	//////////////////////////////////////////////


	return gfxdock;

}

///////////////////////////////////////////////////////////////////////////


void EditorMainWindow::SceneObjPropEdit()
{
	mGedModelObj.Attach( 0 );

	auto pnl = new ui::SplitPanel( "ged.panel", 48,24,256,384 );
	auto pvp1 = new Outliner2View(mEditorBase);
	auto pvp2 = new tool::ged::GedVP( "props.vp", mGedModelObj );
	pnl->SetChild1(pvp1);
	pnl->SetChild2(pvp2);
	gpvp->AddChild(pnl);

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
	object::Connect(	& mEditorBase.SelectionManager().GetSigObjectSelected(),
						& mGedModelObj.GetSlotObjectSelected() );
	//////
	object::Connect(	& mEditorBase.SelectionManager().GetSigObjectDeSelected(),
						& mGedModelObj.GetSlotObjectDeSelected() );
	/////////////////////////////////////////////////////////////////////////////////////
	// 
	/////////////////////////////////////////////////////////////////////////////////////
}

void EditorMainWindow::NewOutliner2View()
{
}

void EditorMainWindow::NewDataflowView()
{
	static int viewnum = 0;
	std::string viewname = CreateFormattedString( "DataflowGraph:%d", viewnum+1 );
	viewnum++;

	auto pnl = new ui::Panel( "dataflow2.panel", 0,512,256,512 );
	auto pvp = new ork::tool::GraphVP(mDataflowEditor,mGedModelObj,viewname);
	pnl->SetChild(pvp);
	gpvp->AddChild(pnl);
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

	if( is_dflow )
	{

		auto pvpdf = new ork::tool::GraphVP(mDataflowEditor,*pnewobjmodel,"yo");

		auto pnl = new ui::SplitPanel( "ged.panel", 512,0,256,256 );
		pnl->SetChild1(pvpdf);
		pnl->SetChild2(pvp);
		pnl->EnableCloseButton();

		gpvp->AddChild(pnl);
		pnlw = pnl;

		mDataflowEditor.Attach( dflow_graph );


	}
	else
	{
		auto pnl = new ui::Panel( "ged.panel", 0,128,128,256 );
		pnl->SetChild(pvp);
		gpvp->AddChild(pnl);
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

#if 0


///////////////////////////////////////////////////////////////////////////

QDockWidget *EditorMainWindow::NewPyConView(bool bfloat)
{
	static int viewnum = 0;
	viewnum++;

	std::string viewname = CreateFormattedString( "PythonConsole:%d", viewnum );
	QDockWidget*gfxdock = new QDockWidget(tr(viewname.c_str()), this);
	gfxdock->setFloating( bfloat );
	gfxdock->setAllowedAreas(Qt::BottomDockWidgetArea);
	gfxdock->setAutoFillBackground(false); 
	gfxdock->setObjectName(viewname.c_str());
	//mGedModelObj.Attach( 0 );
	tool::vp_cons* pvp = new tool::vp_cons( viewname );
	lev2::CQtGfxWindow* pgfxwin = new lev2::CQtGfxWindow( pvp );
	lev2::CTQT* pctqt = new lev2::CTQT( pgfxwin, gfxdock );
	QWidget* pqw = pctqt->GetQWidget();
	gfxdock->setWidget( pqw );
	gfxdock->setMinimumSize( 64, 64 );
	gfxdock->resize( 288, 800 );
	addDockWidget(Qt::BottomDockWidgetArea, gfxdock);
	pctqt->Show();
	pctqt->GetQWidget()->Enable();
	pvp->BindCTQT( pctqt );


/*	std::string viewname = CreateFormattedString( "PythonConsole:%d", viewnum+1 );
	viewnum++;

	ork::tool::QtConsoleWindow* owin = new ork::tool::QtConsoleWindow(bfloat,0);

	QDockWidget*gfxdock = new QDockWidget(tr(viewname.c_str()), this);
	gfxdock->setFloating( bfloat );
	gfxdock->setAllowedAreas(Qt::BottomDockWidgetArea ); //| Qt::RightDockWidgetArea);
	//gfxdock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	gfxdock->setAutoFillBackground(false); 
	gfxdock->setObjectName(viewname.c_str());

	gfxdock->setWidget( owin->GroupBox() );
	
	gfxdock->setMinimumSize( 256, 100 );
	addDockWidget(Qt::LeftDockWidgetArea, gfxdock);

	gfxdock->show();

	*/

	return gfxdock;
}

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
{
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

	gfxdock->show();
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
