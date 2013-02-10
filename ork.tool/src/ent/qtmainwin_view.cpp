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
#include "outliner.h"

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

	gpvp = new SceneEditorVP( "yo", mEditorBase, *this );
	lev2::CQtGfxWindow* pgfxwin = new lev2::CQtGfxWindow( gpvp );

	lev2::CTQT* pctqt = new lev2::CTQT( pgfxwin, gfxdock );

	gfxdock->setWidget( pctqt->GetQWidget() );
	gfxdock->setMinimumSize( 100, 100 );
	//addDockWidget(Qt::NoDockWidgetArea, gfxdock);

	pctqt->Show();

	pctqt->GetQWidget()->Enable();

	viewnum++;


	lev2::GfxEnv::GetRef().SetLoaderTarget( pgfxwin->GetContext() );
	lev2::GfxEnv::GetRef().RegisterWinContext(pgfxwin);

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

QDockWidget* EditorMainWindow::SceneObjPropEdit(bool bfloat)
{
	std::string viewname = CreateFormattedString( "SceneObjProperties" );
	QDockWidget*gfxdock = new QDockWidget(tr(viewname.c_str()), this);
	gfxdock->setFloating( bfloat );
	gfxdock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	gfxdock->setAutoFillBackground(false); 
	gfxdock->setObjectName(viewname.c_str());
	mGedModelObj.Attach( 0 );
	tool::ged::GedVP* pvp = new tool::ged::GedVP( viewname, mGedModelObj );
	lev2::CQtGfxWindow* pgfxwin = new lev2::CQtGfxWindow( pvp );
	lev2::CTQT* pctqt = new lev2::CTQT( pgfxwin, gfxdock );
	pvp->GetGedWidget().BindCTQT( pctqt );
	QWidget* pqw = pctqt->GetQWidget();
	gfxdock->setWidget( pqw );
	gfxdock->setMinimumSize( 64, 64 );
	gfxdock->resize( 288, 800 );
	addDockWidget(Qt::LeftDockWidgetArea, gfxdock);
	pctqt->Show();
	pctqt->GetQWidget()->Enable();
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
	return gfxdock;
}

QDockWidget* EditorMainWindow::NewOutliner2View( bool bfloat )
{
	ork::ent::SceneInst* psi = mEditorBase.GetActiveSceneInst();
	if( psi )
	{
		ork::ent::SceneData* psd = const_cast<ork::ent::SceneData*>( & psi->GetData() );

		SlotSpawnNewGed(psd);
	}	
	return 0;
}

///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::SlotSpawnNewGed( ork::Object* pobj )
{	static int viewnum = 0;
	rtti::Class* pclass = pobj->GetClass();
	std::string classname = pclass->Name().c_str();
	std::string viewname = CreateFormattedString( "%s:%p", classname.c_str(), pobj );
	viewnum++;
	QDockWidget*gfxdock = new QDockWidget(tr(viewname.c_str()), this);
	gfxdock->setFloating( false );
	gfxdock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	gfxdock->setAutoFillBackground(false); 
	gfxdock->setObjectName(viewname.c_str());
	ork::tool::ged::ObjModel* pnewobjmodel = new ork::tool::ged::ObjModel;
	pnewobjmodel->Attach( 0 );
	tool::ged::GedVP* pvp = new tool::ged::GedVP( viewname, *pnewobjmodel );
	lev2::CQtGfxWindow* pgfxwin = new lev2::CQtGfxWindow( pvp );
	lev2::CTQT* pctqt = new lev2::CTQT( pgfxwin, gfxdock );
	pvp->GetGedWidget().BindCTQT( pctqt );
	pvp->GetGedWidget().SetDeleteModel(true);
	QWidget* pqw = pctqt->GetQWidget();
	gfxdock->setWidget( pqw );
	gfxdock->setMinimumSize( 100, 100 );
	gfxdock->resize( 256, 768 );
	gfxdock->setAttribute( Qt::WA_DeleteOnClose );
	
	gfxdock->setFeatures( 
			QDockWidget::DockWidgetClosable
		|	QDockWidget::DockWidgetMovable
		|	QDockWidget::DockWidgetFloatable
		|	QDockWidget::DockWidgetVerticalTitleBar
	);
	addDockWidget(Qt::RightDockWidgetArea, gfxdock);
	pctqt->Show();
	pctqt->GetQWidget()->Enable();
	pnewobjmodel->SetChoiceManager( & mEditorBase.mChoiceMan );

	/////////////////////////////////////////////////////////////////////////////////////
	// clone collapse state

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

QDockWidget *EditorMainWindow::NewDataflowView(bool bfloat)
{
	static int viewnum = 0;
	std::string viewname = CreateFormattedString( "DataflowGraph:%d", viewnum+1 );
	viewnum++;

	QDockWidget*gfxdock = new QDockWidget(tr(viewname.c_str()), this);
	gfxdock->setFloating( bfloat );
	gfxdock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	gfxdock->setAutoFillBackground(false); 
	gfxdock->setObjectName(viewname.c_str());

	ork::tool::GraphVP* pvp = new ork::tool::GraphVP( mDataflowEditor, mGedModelObj, viewname );
	lev2::CQtGfxWindow* pgfxwin = new lev2::CQtGfxWindow( pvp );
	lev2::CTQT* pctqt = new lev2::CTQT( pgfxwin, gfxdock );

	QWidget* pqw = pctqt->GetQWidget();

	gfxdock->setWidget( pqw );
	
	gfxdock->setMinimumSize( 256, 128 );
	addDockWidget(Qt::RightDockWidgetArea, gfxdock);
	setCorner(Qt::TopLeftCorner,Qt::LeftDockWidgetArea );
	gfxdock->resize( 256, 128 );

	pctqt->Show();
	pctqt->GetQWidget()->Enable();
	pgfxwin->GetContext()->FBI()->SetClearColor( CColor3(0.2f,0.2f,0.25f) );
	
	//pvp->SetTarget( pgfxwin->GetContext() );

	return gfxdock;
}

///////////////////////////////////////////////////////////////////////////

QDockWidget *EditorMainWindow::NewOutlinerView(bool bfloat)
{
	static int viewnum = 0;
	std::string viewname = CreateFormattedString( "Outliner:%d", viewnum+1 );
	viewnum++;

	QtOutlinerWindow* owin = new QtOutlinerWindow(true,0,mEditorBase);

	QDockWidget*gfxdock = new QDockWidget(tr(viewname.c_str()), this);
	gfxdock->setFloating( bfloat );
	gfxdock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	gfxdock->setAutoFillBackground(false); 
	gfxdock->setObjectName(viewname.c_str());

	gfxdock->setWidget( owin );
	
	gfxdock->setMinimumSize( 256, 100 );
	addDockWidget(Qt::LeftDockWidgetArea, gfxdock);

	gfxdock->show();

	bool bconOK;
	//bconOK = object::Connect(	&mEditorBase, AddPooledLiteral("SigUpdateAll"),
	//								& owin->Model(), AddPooledLiteral("SlotRefresh") );
	//OrkAssert( bconOK );
	bconOK = object::Connect(	&mEditorBase, AddPooledLiteral("SigSceneTopoChanged"),
								& owin->Model(), AddPooledLiteral("SlotSceneTopoChanged"));
	OrkAssert( bconOK );

	///////////////////////////////////////////////////

    bconOK = object::Connect(	& mEditorBase.SelectionManager(), AddPooledLiteral("SigObjectSelected"),
								& owin->View(), AddPooledLiteral("SlotObjectSelected"));
	OrkAssert( bconOK );
	bconOK = object::Connect(	& mEditorBase.SelectionManager(), AddPooledLiteral("SigObjectDeSelected"),
								& owin->View(), AddPooledLiteral("SlotObjectDeSelected"));
	OrkAssert( bconOK );

	///////////////////////////////////////////////////

	return gfxdock;
}

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
