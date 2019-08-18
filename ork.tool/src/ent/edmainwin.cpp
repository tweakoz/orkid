////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/qtui/qtui.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtmainwin.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/particle/particle.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////
#include <pkg/ent/editor/edmainwin.h>
#include <pkg/ent/editor/qtui_scenevp.h>
#include <ork/util/hotkey.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/FileInputStream.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <pkg/ent/ReferenceArchetype.h>
#include <QtCore/QSettings>
#include <ork/kernel/opq.h>
#include <ork/application/application.h>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QInputDialog>

#if defined(ORK_OSX)
extern bool gPythonEnabled;
#endif


///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::EditorMainWindow, "EditorMainWindow" );
namespace ork { namespace ent {

SceneInst* GetEditorSceneInst()
{
	return gEditorMainWindow->mEditorBase.GetEditSceneInst();
}
void SceneTopoChanged()
{
	gEditorMainWindow->SlotUpdateAll();

	//GetModel().SigModelInvalidated();
}

EditorMainWindow *gEditorMainWindow;

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void RegisterMainWinDefaultModule( EditorMainWindow& emw );
void RegisterLightingModule( EditorMainWindow& emw );
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::Describe()
{	///////////////////////////////////////////////////////////
	RegisterAutoSignal( EditorMainWindow, NewObject );
	///////////////////////////////////////////////////////////
	RegisterAutoSlot( EditorMainWindow, UpdateAll );
	RegisterAutoSlot( EditorMainWindow, OnTimer );
	RegisterAutoSlot( EditorMainWindow, SceneInstInvalidated );
	RegisterAutoSlot( EditorMainWindow, ObjectSelected );
	RegisterAutoSlot( EditorMainWindow, ObjectDeSelected );
	RegisterAutoSlot( EditorMainWindow, SpawnNewGed );
	RegisterAutoSlot( EditorMainWindow, ClearSelection );
	RegisterAutoSlot( EditorMainWindow, PostNewObject );
	///////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::SlotSceneInstInvalidated( ork::Object* pSI )
{

}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::SlotObjectSelected( ork::Object* pobj )
{
	EntData* pdata = rtti::autocast(pobj);

	if( pdata )
	{
		CMatrix4 mtx;
		mtx = pdata->GetDagNode().GetTransformNode().GetTransform().GetMatrix();
		mEditorBase.SetSpawnMatrix( mtx );
		mEditorBase.ManipManager().AttachObject( pobj );
	}
	mGedModelObj.Attach( pobj );
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::SlotPostNewObject( ork::Object* pobj )
{
	ent::Archetype* parch = rtti::autocast(pobj);

	if( parch )
	{
		mGedModelObj.Attach(pobj);
	}
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::SlotObjectDeSelected( ork::Object* pobj )
{

}
void EditorMainWindow::SlotClearSelection()
{
	mGedModelObj.Attach(NULL);
}
void EditorMainWindow::SigNewObject( ork::Object* pOBJ )
{
	mSignalNewObject(&EditorMainWindow::SigNewObject,pOBJ);

}
///////////////////////////////////////////////////////////////////////////
EditorMainWindow::EditorMainWindow(QWidget *parent, const std::string& applicationClassName, QApplication & App)
	: MiniorkMainWindow( parent )
	, mpSplashScreen(0)
	, mQtApplication(App)
	, ConstructAutoSlot(UpdateAll)
	, ConstructAutoSlot(OnTimer)
	, ConstructAutoSlot(ObjectSelected)
	, ConstructAutoSlot(ObjectDeSelected)
	, ConstructAutoSlot(SpawnNewGed)
	, ConstructAutoSlot(ClearSelection)
	, ConstructAutoSlot(PostNewObject)
	, ConstructAutoSlot(SceneInstInvalidated)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

	SetupSignalsAndSlots();

	//////////////////////////////////
	//////////////////////////////////

	HotKeyConfiguration defaultconfig;
	defaultconfig.Default();

	defaultconfig.AddHotKey( "camera_fwd", "w" );
	defaultconfig.AddHotKey( "camera_left", "a" );
	defaultconfig.AddHotKey( "camera_bwd", "s" );
	defaultconfig.AddHotKey( "camera_right", "d" );
	defaultconfig.AddHotKey( "camera_x", "x" );
	defaultconfig.AddHotKey( "camera_y", "y" );
	defaultconfig.AddHotKey( "camera_z", "z" );

	defaultconfig.AddHotKey( "camera_rotl", "num4" );
	defaultconfig.AddHotKey( "camera_rotr", "num6" );
	defaultconfig.AddHotKey( "camera_rotu", "num8" );
	defaultconfig.AddHotKey( "camera_rotd", "num2" );
	defaultconfig.AddHotKey( "camera_realign", "num0" );

	defaultconfig.AddHotKey( "camera_in", "c" );
	defaultconfig.AddHotKey( "camera_out", "num1" );
	defaultconfig.AddHotKey( "camera_up", "num9" );
	defaultconfig.AddHotKey( "camera_down", "num3" );

	defaultconfig.AddHotKey( "camera_aper-", "[" );
	defaultconfig.AddHotKey( "camera_aper+", "]" );

	defaultconfig.AddHotKey( "camera_origin", "o" );
	defaultconfig.AddHotKey( "camera_pick2focus", "p" );
	defaultconfig.AddHotKey( "camera_focus2pick", "f" );

	defaultconfig.AddHotKey( "camera_mouse_dolly", "alt-mmb-none" );
	defaultconfig.AddHotKey( "camera_mouse_rot", "alt-lmb-none" );

	HotKeyManager::GetRef().AddHotKeyConfiguration( "editmode", defaultconfig );

	HotKeyManager::GetRef().Save();
	HotKeyManager::GetRef().Load();

	//////////////////////////////////
	//////////////////////////////////

	AddBuiltInActions();

	//////////////////////////////////
	//////////////////////////////////

	//QPixmap pixmap("editor/splash.png");
	//mpSplashScreen = new QSplashScreen(pixmap);
	//mpSplashScreen->show();

	f64 SplashTimeBase = CSystem::GetRef().GetLoResTime();

	mEditorBase.RegisterChoices();

	mGedModelObj.SetChoiceManager( & mEditorBase.mChoiceMan );

	/////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////


	object::Connect(	& this->GetSigNewObject(),
						& mEditorBase.GetSlotNewObject() );

	object::Connect(	& mEditorBase.SelectionManager().GetSigObjectSelected(),
						& this->GetSlotObjectSelected() );

	object::Connect(	& mEditorBase.SelectionManager().GetSigObjectDeSelected(),
						& this->GetSlotObjectDeSelected() );

	object::Connect(	& mEditorBase.SelectionManager().GetSigClearSelection(),
						& this->GetSlotClearSelection() );

	bool bconOK = object::Connect(	& mEditorBase, AddPooledLiteral("SigObjectDeleted"),
									& mDataflowEditor, AddPooledLiteral("SlotClear"));
	OrkAssert(bconOK);

	bconOK = object::Connect(		& mEditorBase, AddPooledLiteral("SigNewScene"),
									& mDataflowEditor, AddPooledLiteral("SlotClear"));
	OrkAssert(bconOK);

	bconOK = object::Connect(	& mGedModelObj.GetSigPostNewObject(),
						& this->GetSlotPostNewObject() );

	OrkAssert(bconOK);

	////////////////////////////////////

	auto genviewblk = [=]()
	{
		QDockWidget *pdw0 = NewCamView(false);
		SceneObjPropEdit();

		if( gPythonEnabled )
		{	//QDockWidget *pdw3 = NewPyConView(false);
		}

		//QDockWidget *pdw3 = NewDataflowView(false);
		////////////////////////////////////
		//tabifyDockWidget( pdw2, pdw3 );
		//tabifyDockWidget( pdw4, pdw2 );
		setCentralWidget( pdw0->widget() );
	};
	//MainThreadOpQ().push(genviewblk);
	genviewblk();
	////////////////////////////////////

	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

	//////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////

	//while( (CSystem::GetRef().GetSystemRelTime()-SplashTimeBase) < 2.5 )
	//{
	//	Sleep(10);
	//}

    //mpSplashScreen->finish(this);
    //delete mpSplashScreen;

	//connect( & mQtTimer, SIGNAL(timeout()), this, SLOT(SlotOnTimer()));
	//mQtTimer.setInterval(50);
	//mQtTimer.start();

	CreateFunctionMenus();

	////////////////////////////////////////////////

	ork::file::Path collapse_filename( "collapse_state.cst" );
	if( ork::CFileEnv::DoesFileExist( collapse_filename ) )
	{
		ork::tool::ged::PersistMapContainer& container = mGedModelObj.GetPersistMapContainer();

		stream::FileInputStream istream(collapse_filename.c_str());
		reflect::serialize::XMLDeserializer iser(istream);
		bool bOK = container.DeserializeInPlace(iser);
		OrkAssert(bOK);
	}

	////////////////////////////////////////////////

//	mApplicationModelObj.EnablePaint();

	mGedModelObj.EnablePaint();

	LoadLayout();

	this->activateWindow();

/*	auto lamb = [=]()
	{
		this->SlotSpawnNewGed( ork::Application::GetContext() );
	};
	MainThreadOpQ().push(Op(lamb));
*/
}

EditorMainWindow::~EditorMainWindow()
{
	ork::tool::ged::PersistMapContainer& container = mGedModelObj.GetPersistMapContainer();

	ork::file::Path collapse_filename( "collapse_state.cst" );
	stream::FileOutputStream ostream(collapse_filename.c_str());
	reflect::serialize::XMLSerializer oser(ostream);
	container.SerializeInPlace(oser);
}

///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::SlotOnTimer()
{
}

///////////////////////////////////////////////////////////////////////////
bool EditorMainWindow::event(QEvent *qevent)
{
	switch( qevent->type() )
	{
		case ork::tool::QQedRefreshEvent::gevtype:

			// NASA - we must refresh Application window here to reload ProdigyApplication object data
			// into window
//			mApplicationModelObj.Attach(NULL);
//			mApplicationModelObj.Attach(Application::GetContext());

			//mQedModelTool.ProcessQueue();
			//mQedModelObj.ProcessQueue();
			return true;
			break;
		default:
			break;
	}

	return MiniorkMainWindow::event(qevent);
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::SlotUpdateAll()
{
	if( mEditorBase.mpScene )
	{
		//mEditorBase.GetEditSceneInst()->GetData().AutoLoadAssets();
		//mEditorBase.GetEditSceneInst()->ComposeEntities();

		tool::QQedRefreshEvent* prev = new tool::QQedRefreshEvent;
		mQtApplication.postEvent( this, prev ); //, Qt::LowEventPriority );
	}

	//if( mQedModelObj.CurrentObject() )
	//{
	//	mQedModelObj.QueueObject( mQedModelObj.CurrentObject() );
	//}

}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::ArchExport()
{	auto lamb = [=]()
	{	this->mEditorBase.EditorArchExport();
	};
	UpdateSerialOpQ().push_sync(Op(lamb));
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::ArchImport()
{	auto lamb = [=]()
	{	this->mEditorBase.EditorArchImport();
	};
	UpdateSerialOpQ().push_sync(Op(lamb));
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::ArchMakeReferenced()
{	auto lamb = [=]()
	{	this->mEditorBase.EditorArchMakeReferenced();
	};
	UpdateSerialOpQ().push_sync(Op(lamb));
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::ArchMakeLocal()
{	auto lamb = [=]()
	{	this->mEditorBase.EditorArchMakeLocal();
	};
	UpdateSerialOpQ().push_sync(Op(lamb));
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::NewEntity()
{
	NewEntityReq ner;
	mEditorBase.QueueOpASync(ner);
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::NewEntities()
{	bool ok;
	/*int i = QInputDialog::getInteger(this, tr("New Entities..."), tr("Entity Count:"), 1, 1, 0x7FFFFFFF, 1, &ok);
	if(ok)
	{	auto lamb = [=]()
		{	this->mEditorBase.EditorNewEntities(i);
		};
		UpdateSerialOpQ().push_sync(Op(lamb));
	}*/
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::Group()
{
	mEditorBase.EditorGroup();
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::Dupe()
{
	mEditorBase.EditorDupe();
}
///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::AddBuiltInActions()
{
	/////////////////////////////////////
	if( 1 ) RegisterMainWinDefaultModule( *this );
	if( 0 ) RegisterLightingModule( *this );
	/////////////////////////////////////

	//QDockWidget*gfxdock = new QDockWidget(tr("Application"), this);
	//gfxdock->setFloating( false );
	//gfxdock->setAllowedAreas(Qt::AllDockWidgetAreas );
	//gfxdock->setAttribute( Qt::WA_DeleteOnClose );
	//gfxdock->setFeatures( QDockWidget::AllDockWidgetFeatures );
	//gfxdock->setAutoFillBackground(false);

	//mApplicationModelObj.Attach(NULL);

	//tool::ged::GedVP* pvp = new tool::ged::GedVP( "Application", mApplicationModelObj );
	//lev2::CQtGfxWindow* pgfxwin = new lev2::CQtGfxWindow( pvp );
	//lev2::CTQT* pctqt = new lev2::CTQT( pgfxwin, gfxdock );

	//pvp->GetGedWidget().BindCTQT( pctqt );

	//QWidget* pqw = pctqt->GetQWidget();

	//gfxdock->setWidget( pqw );

	//gfxdock->setMinimumSize( 256, 128 );
	//gfxdock->resize( 256, 256 );
	//addDockWidget(Qt::LeftDockWidgetArea, gfxdock);

	//pctqt->Show();
	//pctqt->GetQWidget()->Enable();

	ork::msleep(100);
	//pvp->SetTarget( pctqt->mpTarget );

//	mApplicationModelObj.Attach(Application::GetContext());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void EditorMainWindow::SaveLayout()
{
	QSettings settings("TweakoZ", "MiniorkEditor");

    settings.beginGroup("mainWindow");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("state", saveState());
    settings.endGroup();

}
void EditorMainWindow::LoadLayout()
{
	QSettings settings("TweakoZ", "MiniorkEditor");

    settings.beginGroup("mainWindow");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("state").toByteArray());
    settings.endGroup();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static void ReplaceArchetype(	ent::SceneData* pscene,
								const ent::Archetype* old_arch,
								ent::Archetype* new_arch
							)
{
	orkmap<PoolString,ent::EntData*> EntDatas;
	orkmap<PoolString, SceneObject*> scene_objects = pscene->GetSceneObjects();
	for( orkmap<PoolString, SceneObject*>::const_iterator it=scene_objects.begin(); it!=scene_objects.end(); it++ )
	{	ent::SceneObject* pso = it->second;
		ent::EntData* ped = rtti::autocast( pso );
		if( ped )
		{
			if( ped->GetArchetype() == old_arch )
			{
				EntDatas.insert(std::pair<PoolString,ent::EntData*>(it->first,ped));
			}
		}
	}
	/////////////////////////////////////////////////////////////
	SceneObject* pnonconst = pscene->FindSceneObjectByName( old_arch->GetName() );
	gEditorMainWindow->mEditorBase.EditorDeleteObject( pnonconst );
	pscene->AddSceneObject( new_arch );
	/////////////////////////////////////////////////////////////
	for( orkmap<PoolString, ent::EntData*>::const_iterator it=EntDatas.begin(); it!=EntDatas.end(); it++ )
	{	ent::EntData* ped = it->second;
		ped->SetArchetype( new_arch );
	}
	/////////////////////////////////////////////////////////////
	gEditorMainWindow->SigNewObject( new_arch );
}

///////////////////////////////////////////////////////////////////////////////

class EntArchDeRef : public ork::tool::ged::IOpsDelegate
{
	RttiDeclareConcrete( EntArchDeRef, ork::tool::ged::IOpsDelegate );
public:

	EntArchDeRef() {}
	~EntArchDeRef() {}

	void Execute( ork::Object* ptarget ) final
    {	SetProgress(0.0f);
		ent::EntData* pentdata = rtti::autocast( ptarget );
		if( 0 != pentdata )
		{	const ent::Archetype* parch = pentdata->GetArchetype();
			if( 0 != parch )
			{	const ent::ReferenceArchetype* prefarch = rtti::autocast( parch );
				if( 0 != prefarch )
				{	ent::SceneData* pscene = parch->GetSceneData();
					if( 0 != pscene )
					{	ArchetypeAsset* passet = prefarch->GetAsset();
						if( 0 != passet )
						{	ent::Archetype* pderefarch = passet->GetArchetype();
							if( 0 != pderefarch )
							{	ReplaceArchetype( pscene, parch, pderefarch );
								/////////////////////////////////////////////////////////////
								SetProgress(1.0f);
								gEditorMainWindow->mEditorBase.SelectionManager().AddObjectToSelection(pderefarch);
							}
						}
					}
				}
			}
		}
		tool::ged::IOpsDelegate::RemoveTask( EntArchDeRef::GetClassStatic(), ptarget );
	}
};

void EntArchDeRef::Describe() {}

///////////////////////////////////////////////////////////////////////////////

class EntArchReRef : public ork::tool::ged::IOpsDelegate
{
	RttiDeclareConcrete( EntArchReRef, ork::tool::ged::IOpsDelegate );
public:

	EntArchReRef() {}
	~EntArchReRef() {}


	template <typename T> void find_and_replace( T& source, const T& find, const T& replace )
	{
		size_t j = 0;
		size_t idiff = replace.length()-find.length();
		for (;(j = source.find( find, j )) != T::npos;)
		{
			if( idiff>0 )
			{
				size_t isourcelen = source.length();
				source.resize( isourcelen+idiff );
				size_t inslen = isourcelen-j;
				for( size_t i=(inslen-1); i<inslen; i-- )
				{
					size_t isrc = (j+i);
					size_t idst = (isrc+idiff);
					source[idst] = source[isrc];
				}
			}
			source.replace( j, find.length(), replace );
			j += idiff;
		}
	}

	void Execute( ork::Object* ptarget ) final // virtual
	{	SetProgress(0.0f);
		ent::EntData* pentdata = rtti::autocast( ptarget );
		if( 0 != pentdata )
		{	const ent::Archetype* parch = pentdata->GetArchetype();
			if( 0 != parch )
			{	const ent::ReferenceArchetype* prefarch = rtti::autocast( parch );
				if( 0 != prefarch )
				{	ent::SceneData* pscene = parch->GetSceneData();
					if( 0 != pscene )
					{
						const PoolString OriginalName = parch->GetName();
						/////////////////////////////////////////////////////////////////////
						ArrayString<512> assetname;
						MutableString(assetname).format("data://archetypes%s.mox", parch->GetName().c_str());
						file::Path::NameType newname( parch->GetName().c_str() );
						file::Path assetpath(assetname.c_str());
						file::Path absolutepath = assetpath.ToAbsolute();
						file::Path::NameType absolutepath_raw( absolutepath.c_str() );
						/////////////////////////////////////////////////////////////////////
						find_and_replace<file::Path::NameType>( absolutepath_raw, "\\pc\\", "\\src\\" );
						file::Path SrcFileName( absolutepath_raw );
						SrcFileName.SetExtension( ".mox" );
						stream::FileOutputStream ostream(SrcFileName.c_str());
						reflect::serialize::XMLSerializer oser(ostream);
						oser.Serialize(parch);
						/////////////////////////////////////////////////////////////////////
						find_and_replace<file::Path::NameType>( absolutepath_raw, "\\src\\", "\\pc\\" );
						file::Path PcFileName( absolutepath_raw );
						PcFileName.SetExtension( ".mox" );
						stream::FileOutputStream ostream2(PcFileName.c_str());
						reflect::serialize::XMLSerializer oser2(ostream2);
						oser2.Serialize(parch);
						/////////////////////////////////////////////////////////////////////
						gEditorMainWindow->mEditorBase.mpArchChoices->EnumerateChoices();
						gEditorMainWindow->mEditorBase.mpRefArchChoices->EnumerateChoices(true);
						/////////////////////////////////////////////////////////////////////
						ArchetypeAsset* passet =
							asset::AssetManager<ArchetypeAsset>::Load( assetname.c_str() );
						ent::ReferenceArchetype* newrefarch = new ent::ReferenceArchetype;
						newrefarch->SetAsset( passet );
						newrefarch->SetName( OriginalName );
						/////////////////////////////////////////////////////////////
						ReplaceArchetype( pscene, parch, newrefarch );
						/////////////////////////////////////////////////////////////
						SetProgress(1.0f);
						gEditorMainWindow->mEditorBase.SelectionManager().AddObjectToSelection(newrefarch);
					}
				}
			}
		}
		tool::ged::IOpsDelegate::RemoveTask( EntArchReRef::GetClassStatic(), ptarget );
		/////////////////////////////////////////////////////////////
	}
};

void EntArchReRef::Describe() {}

///////////////////////////////////////////////////////////////////////////////

class EntArchSplit : public ork::tool::ged::IOpsDelegate
{
	RttiDeclareConcrete( EntArchSplit, ork::tool::ged::IOpsDelegate );
public:

	EntArchSplit() {}
	~EntArchSplit() {}

	void Execute( ork::Object* ptarget ) final
    {
		SetProgress(0.0f);
		tool::ged::IOpsDelegate::RemoveTask( EntArchSplit::GetClassStatic(), ptarget );
	}
};

void EntArchSplit::Describe() {}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI( ork::ent::EntArchDeRef, "EntArchDeRef" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::EntArchReRef, "EntArchReRef" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::EntArchSplit, "EntArchSplit" );
