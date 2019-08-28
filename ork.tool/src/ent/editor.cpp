////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <orktool/toolcore/selection.h>

#include <ork/lev2/gfx/texman.h>
#include <orktool/toolcore/selection.h>
//#include "builtincommands.h"
#include <orktool/toolcore/dataflow.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/drawable.h>
///////////////////////////////////////////////////////////////////////////
#include <pkg/ent/entity.h>
#include <pkg/ent/ReferenceArchetype.h>
#include <pkg/ent/editor/editor.h>
///////////////////////////////////////////////////////////////////////////
//#include <ork/reflect/class.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/StringOutputStream.h>
#include <ork/stream/StringInputStream.h>
///////////////////////////////////////////////////////////////////////////
#include <ork/reflect/RegisterProperty.h>
///////////////////////////////////////////////////////////////////////////
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/application/application.h>
#include <QtWidgets/QFileDialog>
#include <ork/kernel/opq.h>
#include <ork/kernel/future.hpp>


INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SceneEditorBase, "SceneEditorBase" );

///////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////

NewEntityReq::shared_t NewEntityReq::makeShared(Future&f) {
    return std::make_shared<NewEntityReq>(f);
}


static Opq gImplSerQ(0,"eddummyopq");

static ork::PoolString gEdChanName;

const ork::PoolString& EditorChanName()
{
	return gEdChanName;
}

void SceneEditorBase::RegisterChoices()
{
}

void SceneEditorBase::Describe()
{
	gEdChanName = ork::AddPooledLiteral("Editor");

	///////////////////////////////////////////////////////////
	RegisterAutoSignal( SceneEditorBase, SceneTopoChanged );
	RegisterAutoSignal( SceneEditorBase, ObjectDeleted );
	RegisterAutoSignal( SceneEditorBase, NewScene );
	///////////////////////////////////////////////////////////
	RegisterAutoSlot( SceneEditorBase, PreNewObject );
	RegisterAutoSlot( SceneEditorBase, ModelInvalidated );
	RegisterAutoSlot( SceneEditorBase, NewObject );
	///////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////

void RefArchetypeChoices::EnumerateChoices(bool bforcenocache)
{
	clear();
	FindAssetChoices( "data://archetypes", "*.mox" );
}

///////////////////////////////////////////////////////////////////////////

RefArchetypeChoices::RefArchetypeChoices()
{
	EnumerateChoices(true);
}

///////////////////////////////////////////////////////////////////////////

EntData* NewEntityReq::GetEntity()
{
	AssertNotOnOpQ(MainThreadOpQ()); // prevent deadlock
	return mResult.GetResult().Get<EntData*>();
}

void NewEntityReq::SetEntity(EntData*pent)
{
	AssertOnOpQ(gImplSerQ);
	mResult.Signal<EntData*>(pent);
}

///////////////////////////////////////////////////////////////////////////

Archetype* NewArchReq::GetArchetype()
{
	AssertNotOnOpQ(MainThreadOpQ()); // prevent deadlock
	return mResult.GetResult().Get<Archetype*>();
}

void NewArchReq::SetArchetype(Archetype*parch)
{
	AssertOnOpQ(gImplSerQ);
	mResult.Signal<Archetype*>(parch);
}

///////////////////////////////////////////////////////////////////////////

SceneData* NewSceneReq::GetScene()
{
	AssertNotOnOpQ(MainThreadOpQ()); // prevent deadlock
	return mResult.GetResult().Get<SceneData*>();
}

void NewSceneReq::SetScene(SceneData*sd)
{
	AssertOnOpQ(gImplSerQ);
	mResult.Signal<SceneData*>(sd);
}

///////////////////////////////////////////////////////////////////////////

SceneData* GetSceneReq::GetScene()
{
	AssertNotOnOpQ(MainThreadOpQ()); // prevent deadlock
	return mResult.GetResult().Get<SceneData*>();
}

void GetSceneReq::SetScene(SceneData*sd)
{
	AssertOnOpQ(gImplSerQ);
	mResult.Signal<SceneData*>(sd);
}

///////////////////////////////////////////////////////////////////////////

SceneEditorBase::SceneEditorBase()
	: mbInit(true)
	, mApplication(0)
	, mpScene( 0 )
	, mpEditSceneInst( 0 )
	, mpExecSceneInst( 0 )
	, mpMdlChoices( new tool::CModelChoices )
	, mpAnmChoices( new tool::CAnimChoices )
	, mpAudStreamChoices( new tool::AudioStreamChoices )
	, mpAudBankChoices( new tool::AudioBankChoices )
	, mpTexChoices( new tool::CTextureChoices )
	, mpScriptChoices( new tool::ScriptChoices )
	, mpArchChoices( new ArchetypeChoices(*this) )
	, mpChsmChoices( new tool::ChsmChoices )
	, mpRefArchChoices( new RefArchetypeChoices )
	, mpFxShaderChoices( new tool::FxShaderChoices )
	, ConstructAutoSlot(ModelInvalidated)
	, ConstructAutoSlot(PreNewObject)
	, ConstructAutoSlot(NewObject)
{
	SetupSignalsAndSlots();

	mChoiceMan.AddChoiceList("chsm", mpChsmChoices);
	mChoiceMan.AddChoiceList("xgmodel", mpMdlChoices);
	mChoiceMan.AddChoiceList("xganim", mpAnmChoices);
	mChoiceMan.AddChoiceList("lev2::audiostream", mpAudStreamChoices);
	mChoiceMan.AddChoiceList("lev2::audiobank", mpAudBankChoices);
	mChoiceMan.AddChoiceList("lev2tex", mpTexChoices);
	mChoiceMan.AddChoiceList("script", mpScriptChoices);
	mChoiceMan.AddChoiceList("archetype", mpArchChoices);
	mChoiceMan.AddChoiceList("refarch", mpRefArchChoices);
	mChoiceMan.AddChoiceList("FxShader", mpFxShaderChoices);

	object::Connect(	& this->GetSigObjectDeleted(),
						& mSelectionManager.GetSlotObjectDeleted() );

	object::Connect(	& this->GetSigObjectDeleted(),
						& mManipManager.GetSlotObjectDeleted() );

	object::Connect(	& mSelectionManager.GetSigObjectSelected(),
						& mManipManager.GetSlotObjectSelected() );

	object::Connect(	& mSelectionManager.GetSigObjectDeSelected(),
						& mManipManager.GetSlotObjectDeSelected() );

	object::Connect(	& mSelectionManager.GetSigClearSelection(),
						& mManipManager.GetSlotClearSelection() );

///////////////////////////////////////////////////////////////////////////

	struct runloop_impl
	{
		static void* do_it(void* pdata)
		{
			SceneEditorBase* seb = (SceneEditorBase*) pdata;
			seb->RunLoop();
			return 0;
		}
	};
	pthread_t thr;
	int istat = pthread_create(&thr,nullptr,runloop_impl::do_it, (void*) this );
	assert(istat==0);
}

///////////////////////////////////////////////////////////////////////////

SceneEditorBase::~SceneEditorBase()
{
	mRunStatus=1;
	while(mRunStatus==1)
	{
		usleep(1000);
	}
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorBase::QueueSync()
{
	Future the_future;
	BarrierSyncReq R(the_future);
	QueueOpASync(R);
	the_future.WaitForSignal();
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorBase::QueueOpASync( const var_t& op )
{
	mSerialQ.push(op);
}
void SceneEditorBase::QueueOpSync( const var_t& op )
{
	QueueOpASync(op);
	QueueSync();
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorBase::RunLoop()
{
	SetCurrentThreadName( "SceneEdRunLoop" );

	OpqTest opqt(&gImplSerQ);

	var_t event;

	auto& updQ = UpdateSerialOpQ();

	auto disable_op = [&]()
	{
		gUpdateStatus.SetState(EUPD_STOP);
		this->DisableUpdates();
		this->DisableViews();
	};
	auto enable_op = [&]()
	{
		this->EnableViews();
		this->EnableUpdates();
		gUpdateStatus.SetState(EUPD_START);
	};

	auto do_it = [&]()
	{
		while( this->mSerialQ.try_pop(event) )
		{
			if( event.IsA<BarrierSyncReq>() )
			{
				auto& R = event.Get<BarrierSyncReq>();
				R.mFuture.Signal<bool>(true);
			}
			else if( event.IsA<LoadSceneReq>() )
			{
				const auto& R = event.Get<LoadSceneReq>();
				tool::GetGlobalDataFlowScheduler()->GraphSet().LockForWrite().clear();
				ImplLoadScene( R.mFileName );
				tool::GetGlobalDataFlowScheduler()->GraphSet().UnLock();
				if( R.GetOnLoaded().IsA<void_lambda_t>() )
				{
					R.GetOnLoaded().Get<void_lambda_t>()();
				}
			}
			else if( event.IsA<NewSceneReq::shared_t>() )
			{
				auto req = event.Get<NewSceneReq::shared_t>();
				Op(disable_op).QueueSync(updQ);
				auto s = ImplNewScene();
				Op(enable_op).QueueSync(updQ);
				req->SetScene(s);
			}
			else if( event.IsA<GetSceneReq::shared_t>() )
			{
				auto req = event.Get<GetSceneReq::shared_t>();
				Op(disable_op).QueueSync(updQ);
				auto s = ImplGetScene();
				Op(enable_op).QueueSync(updQ);
				req->SetScene(s);
			}
			else if( event.IsA<RunLocalReq>() )
			{
				const auto& R = event.Get<RunLocalReq>();
				Op(disable_op).QueueSync(updQ);
				ImplEnterRunLocalState();
				Op(enable_op).QueueSync(updQ);
			}
			else if( event.IsA<StopLocalReq>() )
			{
				const auto& R = event.Get<StopLocalReq>();
				Op(disable_op).QueueSync(updQ);
				ImplEnterEditState();
				Op(enable_op).QueueSync(updQ);
			}
			else if( event.IsA<NewEntityReq::shared_t>() )
			{
				auto req = event.Get<NewEntityReq::shared_t>();
				Op(disable_op).QueueSync(updQ);
				EntData* pent = ImplNewEntity(req->mArchetype);
				Op(enable_op).QueueSync(updQ);
				req->SetEntity(pent);
			}
			else if( event.IsA<NewArchReq>() )
			{
				auto& R = event.Get<NewArchReq>();
				Op(disable_op).QueueSync(updQ);
				auto parch = ImplNewArchetype(R.mClassName,R.mName);
				Op(enable_op).QueueSync(updQ);
				R.SetArchetype(parch);
			}
			else if( event.IsA<DeleteObjectReq>() )
			{
				const auto& R = event.Get<DeleteObjectReq>();
				Op(disable_op).QueueSync(updQ);
				ImplDeleteObject(R.mObject);
				Op(enable_op).QueueSync(updQ);
			}
			else
			{
				assert(false);
			}
		}
		usleep(1000);
	};

	///////////////////////////////////////
	// main loop
	///////////////////////////////////////
	mRunStatus = 0;
	while( mRunStatus==0 )
	{
		do_it();
	}
	///////////////////////////////////////
	// empty the queue before exiting
	do_it();
	///////////////////////////////////////
	mRunStatus=2;

}

///////////////////////////////////////////////////////////////////////////

void SceneEditorBase::EditorRefreshModels()
{
	mpMdlChoices->EnumerateChoices(true);
}
void SceneEditorBase::EditorRefreshAnims()
{
	mpAnmChoices->EnumerateChoices(true);
}
void SceneEditorBase::EditorRefreshTextures()
{
	mpTexChoices->EnumerateChoices(true);
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::NewSceneInst()
{
	if( mpEditSceneInst ){
		bool BOK = object::Disconnect(	this, AddPooledLiteral("SigSceneTopoChanged"),
										mpEditSceneInst, AddPooledLiteral("SlotSceneTopoChanged"));
		assert( BOK );
		SceneInst* psi2del = mpEditSceneInst;
		mpEditSceneInst = nullptr;
		delete psi2del;
	}
	if( mpScene ){
		mpEditSceneInst = new SceneInst( mpScene, ApplicationStack::Top() );
		bool bconOK = object::Connect(	this, AddPooledLiteral("SigSceneTopoChanged"),
										mpEditSceneInst, AddPooledLiteral("SlotSceneTopoChanged"));
		assert( bconOK );
		mpEditSceneInst->SetSceneInstMode(ESCENEMODE_EDIT);
	}
}
///////////////////////////////////////////////////////////////////////////
SceneData* SceneEditorBase::ImplGetScene()
{
	////////////////////////////////////
	// to prevent deadlock
	ork::AssertOnOpQ2( gImplSerQ );
	////////////////////////////////////
	SceneData* rval = nullptr;
	auto get_scene_op = [&]()
	{
		rval = mpScene;
	};
	Op(get_scene_op).QueueSync(UpdateSerialOpQ());
	return mpScene;
}
///////////////////////////////////////////////////////////////////////////
SceneData* SceneEditorBase::ImplNewScene()
{
	////////////////////////////////////
	// to prevent deadlock
	ork::AssertOnOpQ2( gImplSerQ );
	////////////////////////////////////
	auto new_scene_op = [&]()
	{

		auto& dfset = tool::GetGlobalDataFlowScheduler()->GraphSet();
		dfset.LockForWrite().clear();

		mSelectionManager.ClearSelection();
		ent::SceneData* poldscene = mpScene;
		mpScene = new ent::SceneData;
		mpArchChoices->EnumerateChoices();
		NewSceneInst();
		if( poldscene )
		{
			delete poldscene;
		}

		mpScene->EnterEditState();

		dfset.UnLock();
		SigNewScene();
		SigSceneTopoChanged();
	};
	Op(new_scene_op).QueueSync(UpdateSerialOpQ());
	return mpScene;
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ImplLoadScene( std::string fname )
{
	////////////////////////////////////
	// to prevent deadlock
	ork::AssertOnOpQ2( gImplSerQ );
	////////////////////////////////////

	if(fname.length()==0)
		return;

	////////////////////////////////////
	auto pre_load_op = [=]()
	{	this->DisableViews();
		lev2::GfxEnv::GetRef().GetGlobalLock().Lock(0x666);
		this->mSelectionManager.ClearSelection();
		ent::SceneData* poldscene = this->mpScene;
		this->mpScene = 0;
		////////////////////////////////////
		auto load_op = [=]()
		{	stream::FileInputStream istream(fname.c_str());
			reflect::serialize::XMLDeserializer iser(istream);
			rtti::ICastable* pcastable = nullptr;
			bool bloadOK = iser.Deserialize( pcastable );
			////////////////////////////////////
			auto post_load_op = [=]()
			{	if( bloadOK )
				{	ent::SceneData* pscene = rtti::autocast( pcastable );
					if( pscene )
					{
						this->mpScene = pscene;
						this->mpArchChoices->EnumerateChoices();
					}
					else
					{
						OrkNonFatalAssertFunction( "Are you sure you are trying to load a scene file? I think not.." );
					}
					this->NewSceneInst();
					if( poldscene )
					{
						delete poldscene;
					}
				}
				else
					mpScene = poldscene;

				lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();

				this->SigNewScene();
				this->SigSceneTopoChanged();
				this->EnableViews();

				printf( "YOYOY\n");
				fflush(stdout);
				ork::msleep(1000.0f);
			};
			Op(post_load_op).QueueASync(UpdateSerialOpQ());
		};
		Op(load_op).QueueASync(MainThreadOpQ());
	};
	Op(pre_load_op).QueueASync(UpdateSerialOpQ());
	////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorDupe()
{
	SigSceneTopoChanged();
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorGroup()
{
	if( mpScene )
	{
		const orkset<Object*> & SelSet = mSelectionManager.GetActiveSelection();

		if( SelSet.size() )
		{
			const CReal kmax = CFloat::TypeMax();
			CReal fmaxx = -kmax;
			CReal fmaxy = -kmax;
			CReal fmaxz = -kmax;
			CReal fminx = kmax;
			CReal fminy = kmax;
			CReal fminz = kmax;

			for( orkset<Object*>::const_iterator it=SelSet.begin(); it!=SelSet.end(); it++ )
			{
				SceneObject* pso = rtti::downcast<SceneObject*>( (*it) );

				EntData* pentdata = rtti::downcast< EntData* >( pso );
				SceneGroup* pgroup = rtti::downcast< SceneGroup* >( pso );

				if(pentdata||pgroup)
				{
					DagNode& Node = pentdata ? pentdata->GetDagNode() : pgroup->GetDagNode();

					CVector3 Pos = Node.GetTransformNode().GetTransform().GetPosition();

					fmaxx = (fmaxx>Pos.GetX()) ? fmaxx : Pos.GetX();
					fmaxy = (fmaxy>Pos.GetY()) ? fmaxy : Pos.GetY();
					fmaxz = (fmaxz>Pos.GetZ()) ? fmaxz : Pos.GetZ();

					fminx = (fminx<Pos.GetX()) ? fminx : Pos.GetX();
					fminy = (fminy<Pos.GetY()) ? fminy : Pos.GetY();
					fminz = (fminz<Pos.GetZ()) ? fminz : Pos.GetZ();
				}
			}

			SceneGroup* pgroup = new SceneGroup;

			CVector3 center( (fmaxx+fminx)*0.5f, (fmaxy+fminy)*0.5f, (fmaxz+fminz)*0.5f );

			pgroup->GetDagNode().GetTransformNode().GetTransform().SetPosition( center );

			PoolString ps = mpScene->NewObjectName();
			pgroup->SetName(ps);
			mpScene->AddSceneObject( pgroup );

			for( orkset<Object*>::const_iterator it=SelSet.begin(); it!=SelSet.end(); it++ )
			{
				SceneDagObject* pso = rtti::downcast<SceneDagObject*>( (*it) );

				OrkAssert( pso );

				pgroup->GetDagNode().AddChild( & pso->GetDagNode() );
				pgroup->AddChild( pso );
				pso->SetParentName( pgroup->GetName() );

				pso->GetDagNode().GetTransformNode().ReParent( & pgroup->GetDagNode().GetTransformNode() );

			}

			SigSceneTopoChanged();
		}

	}

}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorArchExport()
{
	if(mpScene)
	{
		const orkset<Object *> &selection = mSelectionManager.GetActiveSelection();
		if(selection.size() > 0)
		{
			for(orkset<Object *>::const_iterator it = selection.begin(); it != selection.end(); it++)
				if(ent::Archetype *archetype = rtti::autocast(*it))
					if(!archetype->GetClass()->IsSubclassOf(ent::ReferenceArchetype::GetClassStatic()))
					{
						ArrayString<512> assetname;
						MutableString(assetname).format("data://archetypes%s.mox", archetype->GetName().c_str());

						file::Path assetpath(assetname.c_str());
						file::Path absolutepath = assetpath.ToAbsolute();

						ConstString::size_type pcpos = ConstString(absolutepath.c_str()).find("\\pc\\");

						ArrayString<512> absassetname;
						MutableString mutstr(absassetname);
						mutstr += ConstString(absolutepath.c_str()).substr(0, pcpos);
						mutstr += "\\src\\";
						mutstr += ConstString(absolutepath.c_str()).substr(pcpos + 4);

						QString FileName = QFileDialog::getSaveFileName(0, "Save Archetype File", absassetname.c_str(), "OrkArchetypeFile (*.mox *.mob)");
						file::Path::NameType fname = FileName.toStdString().c_str();
						if(fname.length())
						{
							if(CFileEnv::filespec_to_extension(fname).length() == 0) fname += ".mox";

							stream::FileOutputStream ostream(fname.c_str());
							reflect::serialize::XMLSerializer oser(ostream);
							oser.Serialize(archetype);
						}
					}
		}
	}
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorArchImport()
{
	if(mpScene)
	{
		file::Path assetpath("data://archetypes/");
		file::Path absolutepath = assetpath.ToAbsolute();

		ConstString::size_type pcpos = ConstString(absolutepath.c_str()).find("\\pc\\");

		ArrayString<512> absassetname;
		MutableString mutstr(absassetname);
		mutstr += ConstString(absolutepath.c_str()).substr(0, pcpos);
		mutstr += "\\src\\";
		mutstr += ConstString(absolutepath.c_str()).substr(pcpos + 4);

		QString FileName = QFileDialog::getOpenFileName(NULL, "Load OrkArchetypeFile", absassetname.c_str(), "OrkArchetypeFile (*.mox *.mob)");
		std::string fname = FileName.toStdString().c_str();
		if(fname.length())
		{
			stream::FileInputStream istream(fname.c_str());
			reflect::serialize::XMLDeserializer iser(istream);

			rtti::ICastable *pcastable = 0;
			bool bOK = iser.Deserialize(pcastable);

			if(ent::Archetype *archetype = rtti::autocast(pcastable))
			{
				mpScene->AddSceneObject(archetype);

				SceneComposer scene_composer(mpScene);
				archetype->Compose(scene_composer);

				mpArchChoices->EnumerateChoices();
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorArchMakeReferenced()
{
	if(mpScene)
	{
		const orkset<Object *> &selection = mSelectionManager.GetActiveSelection();
		if(selection.size() > 0)
		{
			for(orkset<Object *>::const_iterator it = selection.begin(); it != selection.end(); it++)
				if(ent::Archetype *archetype = rtti::autocast(*it))
					if(!archetype->GetClass()->IsSubclassOf(ent::ReferenceArchetype::GetClassStatic()))
					{
						ArrayString<512> assetname;
						MutableString(assetname).format("data://archetypes%s.mox", archetype->GetName().c_str());

						file::Path assetpath(assetname.c_str());
						file::Path absolutepath = assetpath.ToAbsolute();

						ConstString::size_type pcpos = ConstString(absolutepath.c_str()).find("\\pc\\");

						ArrayString<512> absassetname;
						MutableString mutstr(absassetname);
						mutstr += ConstString(absolutepath.c_str()).substr(0, pcpos);
						mutstr += "\\src\\";
						mutstr += ConstString(absolutepath.c_str()).substr(pcpos + 4);

						QString FileName = QFileDialog::getSaveFileName(0, "Save Archetype File", absassetname.c_str(), "OrkArchetypeFile (*.mox *.mob)");
						file::Path::NameType fname = FileName.toStdString().c_str();
						if(fname.length())
						{
							if(CFileEnv::filespec_to_extension(fname).length() == 0) fname += ".mox";

							stream::FileOutputStream ostream(fname.c_str());
							reflect::serialize::XMLSerializer oser(ostream);
							oser.Serialize(archetype);
						}
					}
		}
	}
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorArchMakeLocal()
{
	if(mpScene)
	{
		const orkset<Object *> &selection = mSelectionManager.GetActiveSelection();
		if(selection.size() > 0)
		{
			for(orkset<Object *>::const_iterator it = selection.begin(); it != selection.end(); it++)
				if(ent::ReferenceArchetype *refarchetype = rtti::autocast(*it))
					if(ent::ArchetypeAsset *archasset = refarchetype->GetAsset())
						if(ent::Archetype *archetype = archasset->GetArchetype())
						{
						}
		}
	}
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorUnGroup(SceneGroup* pgrp)
{
	pgrp->UnGroupAll();
	mpScene->RemoveSceneObject( pgrp );
	delete( pgrp );
	SigSceneTopoChanged();
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorPlaceEntity()
{
	if( mpScene )
	{
		const orkset<ork::Object*> & selset = SelectionManager().GetActiveSelection();

		for( orkset<ork::Object*>::const_iterator it=selset.begin(); it!= selset.end(); it++ )
		{
			ent::EntData *pentdata = rtti::autocast(*it);

			if( pentdata )
			{
				pentdata->GetDagNode().GetTransformNode().GetTransform().SetMatrix(mSpawnMatrix);

			}
		}
	}
}

void SceneEditorBase::EditorLocateEntity(const CMatrix4 &matrix)
{
	ent::EntData *pentdata = 0;

	if(mpScene)
	{
		const orkset<ork::Object*> &selection = SelectionManager().GetActiveSelection();
		if(selection.size() == 1)
		{
			ork::Object *pobj = *selection.begin();
			if(ent::EntData *entdata = rtti::autocast(pobj))
			{
				entdata->GetDagNode().GetTransformNode().GetTransform().SetMatrix(matrix);
				SigSceneTopoChanged();
			}
		}
	}
}

bool SceneEditorBase::EditorGetEntityLocation(CMatrix4 &matrix)
{
	ent::EntData *pentdata = 0;

	if(mpScene)
	{
		const orkset<ork::Object*> &selection = SelectionManager().GetActiveSelection();
		if(selection.size() == 1)
		{
			ork::Object *pobj = *selection.begin();
			if(ent::EntData *entdata = rtti::autocast(pobj))
			{
				matrix = entdata->GetDagNode().GetTransformNode().GetTransform().GetMatrix();
				return true;
			}
		}
	}
	return false;
}
///////////////////////////////////////////////////////////////////////////
ent::EntData* SceneEditorBase::EditorNewEntity(const ent::Archetype* parchetype)
{
	Future new_ent;
	auto ner = NewEntityReq::makeShared(new_ent);
	ner->mArchetype = parchetype;
	QueueOpASync(ner);
	return new_ent.GetResult().Get<EntData*>();
}
///////////////////////////////////////////////////////////////////////////
ent::Archetype* SceneEditorBase::EditorNewArchetype(const std::string& classname, const std::string& name)
{
	Future new_arch;
	auto nar = NewArchReq::makeShared(new_arch);
	nar->mClassName = classname;
	nar->mName = name;
	QueueOpASync(nar);
	return new_arch.GetResult().Get<Archetype*>();
}
///////////////////////////////////////////////////////////////////////////
ent::EntData *SceneEditorBase::ImplNewEntity(const ent::Archetype* parchetype)
{
	////////////////////////////////////
	// to prevent deadlock
	ork::AssertOnOpQ2( gImplSerQ );
	////////////////////////////////////

	if( nullptr == mpScene ) return nullptr;

	ent::EntData *pentdata = 0;

	auto lamb = [&]()
	{
		if(nullptr!=parchetype)
		{
			const orkset<ork::Object*> &selection = SelectionManager().GetActiveSelection();
			// if archetype selected, assign to new entity
			// if entitiy selected, use its archetype
			if(selection.size() == 1)
			{
				ork::Object *pobj = *selection.begin();
				EntData* pentdata = rtti::autocast(pobj);
				bool is_ent = (pentdata!=nullptr);
				parchetype = is_ent ? pentdata->GetArchetype()
									: rtti::autocast(pobj);
			}
		}

		SlotPreNewObject();

		pentdata = new ent::EntData;
		//pentdata->GetDagNode().GetTransformNode().GetTransform()->SetPosition(mCursor);
		pentdata->GetDagNode().GetTransformNode().GetTransform().SetMatrix(mSpawnMatrix);
		pentdata->SetArchetype(parchetype);

		if(parchetype && ConstString(parchetype->GetName()).find("/arch/") != ConstString::npos)
		{
			PieceString name = PieceString(parchetype->GetName()).substr(6);
			pentdata->SetName( ork::AddPooledString( name ) );
		}
		else
			pentdata->SetName( ork::AddPooledLiteral( ent::EntData::GetClassStatic()->GetPreferredName() ) );

		mpScene->AddSceneObject( pentdata );

		////////////////////
		// create an instance for the editor to draw
		////////////////////

		ent::Entity* pent = new ent::Entity( *pentdata, mpEditSceneInst );

		if( parchetype )
		{
			parchetype->ComposeEntity( pent );
		}

		mpEditSceneInst->SetEntity( pentdata, pent );
		mpEditSceneInst->ActivateEntity( pent );

		////////////////////
		SigSceneTopoChanged();
		ClearSelection();
		AddObjectToSelection( pentdata );

	};
	Op(lamb).QueueSync(UpdateSerialOpQ());
	return pentdata;
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorNewEntities(int count)
{
	if(mpScene)
	{
		ent::Archetype *archetype = NULL;
		const orkset<ork::Object*> &selection = SelectionManager().GetActiveSelection();
		if(selection.size() == 1)
		{
			ork::Object *pobj = *selection.begin();
			archetype = rtti::autocast(pobj);
		}

		if(count > 0)
			for(int i = 0; i < count; i++)
				EditorNewEntity(archetype);
	}
}
///////////////////////////////////////////////////////////////////////////
ent::EntData *SceneEditorBase::EditorReplicateEntity()
{
	ent::EntData *pentdata = 0;

	if(mpScene)
	{
		const ent::Archetype *archetype = NULL;
		CQuaternion rotation;
		std::string name;

		const orkset<ork::Object*> &selection = SelectionManager().GetActiveSelection();
		if(selection.size() == 1)
		{
			ork::Object *pobj = *selection.begin();
			archetype = rtti::autocast(pobj);
			if(!archetype)
				if(ent::EntData *entdata = rtti::autocast(pobj))
				{
					archetype = entdata->GetArchetype();
					rotation = entdata->GetDagNode().GetTransformNode().GetTransform().GetRotation();
					name = entdata->GetName().c_str();
				}
		}

		SlotPreNewObject();

		pentdata = new ent::EntData;

		ork::CVector3 cursor_pos = mSpawnMatrix.GetTranslation();

		pentdata->GetDagNode().GetTransformNode().GetTransform().SetPosition(cursor_pos);
		pentdata->GetDagNode().GetTransformNode().GetTransform().SetRotation(rotation);
		pentdata->SetArchetype(archetype);

		if(name.empty())
			pentdata->SetName( ork::AddPooledLiteral( ent::EntData::GetClassStatic()->GetPreferredName() ) );
		else
			pentdata->SetName(name.c_str());
		mpScene->AddSceneObject( pentdata );

		////////////////////
		// create an instance for the editor to draw
		////////////////////

		ent::Entity* pent = new ent::Entity( *pentdata, mpEditSceneInst );
		if(archetype)
		{
			archetype->ComposeEntity( pent );
			archetype->LinkEntity( mpEditSceneInst, pent );
		}
		mpEditSceneInst->SetEntity( pentdata, pent );
		mpEditSceneInst->ActivateEntity( pent );

		////////////////////
		SigSceneTopoChanged();
		ClearSelection();
		AddObjectToSelection( pentdata );
	}
	return pentdata;
}

///////////////////////////////////////////////////////////////////////////
// query if an object references an archetype
///////////////////////////////////////////////////////////////////////////

bool QueryArchetypeReferenced( ork::Object* pobj, const ent::Archetype* parch )
{
	bool brval = false;

	/////////////////////////////

	FixedString<32> ArchSource;
	ArchSource.format( "%08x", parch );

	/////////////////////////////
	const reflect::IObjectFunctor *functor = rtti::downcast<object::ObjectClass*>(pobj->GetClass())->Description().FindFunctor("SlotArchetypeReferenced");
	if( functor )
	{
		reflect::IInvokation *invokation = functor->CreateInvokation();
		if(invokation->GetNumParameters() == 1)
		{
			bool bok = reflect::SetInvokationParameter(invokation, 0, ArchSource.c_str());
			OrkAssert(bok);

			ArrayString<32> resultdata;
			stream::StringOutputStream ostream(resultdata);
			reflect::serialize::XMLSerializer serializer(ostream);
			reflect::BidirectionalSerializer result_bidi(serializer);
			functor->invoke( pobj, invokation, &result_bidi);

			const char* presult = resultdata.c_str();

			if( 0 == strcmp( "true", presult ) )
			{
				return true;
			}

		}
		delete invokation;
	}
	return brval;
}

///////////////////////////////////////////////////////////////////////////

ork::CColor4 SceneEditorBase::GetModColor( const ork::Object* pobj ) const
{
	const Entity* pent = rtti::autocast( pobj );
	const ent::EntData* pentdata = & pent->GetEntData();
	const ent::Archetype* parch = pentdata->GetArchetype();
	const ork::tool::SelectManager &selectmgr = SelectionManager();

	if( pent )
	{
		const float finvsaturation = 0.3f;

		if(selectmgr.IsObjectSelected(pentdata))
		{
			return ork::CVector4( 1.0f, finvsaturation, finvsaturation, 1.0f );
		}
		else if(selectmgr.IsObjectSelected(parch))
		{
			return ork::CVector4( finvsaturation, finvsaturation, 1.0f, 1.0f );
		}
		else if( parch ) // is any archetype indirectly referenced by this entity selected (via spawner)
		{
			const orkset<ork::Object*> &selset = selectmgr.GetActiveSelection();

			if( 1 == selset.size() )
			{
				const ent::Archetype* prefarch = rtti::autocast( *selset.begin() );

				if( prefarch )
				{
					const ent::ComponentDataTable::LutType& clut = parch->GetComponentDataTable().GetComponents();
					for( ent::ComponentDataTable::LutType::const_iterator it = clut.begin(); it!= clut.end(); it++ )
					{	ent::ComponentData* pcompdata = it->second;
						if( pcompdata )
						{
							bool bisref = QueryArchetypeReferenced( pcompdata, prefarch );

							if( bisref )
							{
								return ork::CVector4( finvsaturation, 1.0f, finvsaturation, 1.0f );
							}
						}
					}
				}
			}
		}
	}
	return ork::CColor4::White();
}

///////////////////////////////////////////////////////////////////////////
// notify an object that an archetype has been deleted (IF the object cares)
///////////////////////////////////////////////////////////////////////////

void DynamicSignalArchetypeDeleted( ork::Object* pobj, ent::Archetype* parch )
{
	/////////////////////////////

	FixedString<32> ArchSource;
	ArchSource.format( "%08x", parch );

	/////////////////////////////
	const reflect::IObjectFunctor *functor = rtti::downcast<object::ObjectClass*>(pobj->GetClass())->Description().FindFunctor("SlotArchetypeDeleted");
	if( functor )
	{
		reflect::IInvokation *invokation = functor->CreateInvokation();
		if(invokation->GetNumParameters() == 1)
		{
			bool bok = reflect::SetInvokationParameter(invokation, 0, ArchSource.c_str());
			OrkAssert(bok);

			ArrayString<32> resultdata;
			stream::StringOutputStream ostream(resultdata);
			reflect::serialize::XMLSerializer serializer(ostream);
			reflect::BidirectionalSerializer result_bidi(serializer);
			functor->invoke( pobj, invokation, &result_bidi);
		}
		delete invokation;
	}
}

///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorDeleteObject(ork::Object* pobj)
{
	printf( "EditorDeleteObject pobj<%p>\n", pobj );

	DeleteObjectReq R;
	R.mObject = pobj;
	QueueOpASync(R);
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ImplDeleteObject(ork::Object* pobj)
{
	////////////////////////////////////
	// to prevent deadlock
	ork::AssertOnOpQ2( gImplSerQ );
	////////////////////////////////////
	if( nullptr == mpScene ) return;

	auto lamb = [=]()
	{
		SlotPreNewObject();

		/////////////////////////////////////////
		SceneObject* psobj = rtti::downcast<SceneObject*>( pobj );

		printf( "EDITORIMPLDELETE pobj<%p> psobj<%p>\n", pobj, psobj );

		if( nullptr == psobj )
			return;

		ork::ent::DrawableBuffer::ClearAndSyncReaders();

		OrkAssert( psobj );
		mpScene->RemoveSceneObject( psobj );
		/////////////////////////////////////////
		// Has an archetype has been deleted?
		/////////////////////////////////////////
		ent::Archetype* parch = rtti::autocast(pobj);
		if( parch )
		{
			mpArchChoices->EnumerateChoices();

			orkmap<PoolString, SceneObject*>& sobjs = mpScene->GetSceneObjects();

			for( orkmap<PoolString, SceneObject*>::const_iterator it=sobjs.begin(); it!=sobjs.end(); it++ )
			{
				SceneObject* itsobj = it->second;

				Archetype* otharch = rtti::autocast( itsobj );

				if( otharch && otharch!=parch )
				{
					const ent::ComponentDataTable::LutType& clut = otharch->GetComponentDataTable().GetComponents();
					for( ent::ComponentDataTable::LutType::const_iterator it = clut.begin(); it!= clut.end(); it++ )
					{	ent::ComponentData* pcompdata = it->second;
						if( pcompdata )
						{
							DynamicSignalArchetypeDeleted( pcompdata, parch );
						}
					}
				}
				DynamicSignalArchetypeDeleted( itsobj, parch );
			}
		}
		/////////////////////////////////////////
		// notify all listeners that this object is getting deleted
		// SigObjectDeleted(pobj);
		mSignalObjectDeleted(&SceneEditorBase::EditorDeleteObject,pobj);
		/////////////////////////////////////////
		delete( psobj );
		/////////////////////////////////////////
		SigSceneTopoChanged();

	};
	Op(lamb).QueueASync(UpdateSerialOpQ());
}
void SceneEditorBase::DisableUpdates()
{
	ork::AssertOnOpQ2( UpdateSerialOpQ() );
	ork::ent::DrawableBuffer::ClearAndSyncReaders();
	ork::event::Broadcaster& bcaster = ork::event::Broadcaster::GetRef();
	SceneInstEvent disviewev( 0, SceneInstEvent::ESIEV_DISABLE_UPDATE);
	bcaster.BroadcastNotifyOnChannel( & disviewev, SceneInst::EventChannel() );
}
void SceneEditorBase::EnableUpdates()
{
	ork::AssertOnOpQ2( UpdateSerialOpQ() );
	ork::ent::DrawableBuffer::ClearAndSyncReaders();
	ork::event::Broadcaster& bcaster = ork::event::Broadcaster::GetRef();
	SceneInstEvent disviewev( 0, SceneInstEvent::ESIEV_ENABLE_UPDATE);
	bcaster.BroadcastNotifyOnChannel( & disviewev, SceneInst::EventChannel() );
}
void SceneEditorBase::DisableViews()
{
	ork::AssertOnOpQ2( UpdateSerialOpQ() );
	ork::ent::DrawableBuffer::ClearAndSyncReaders();
	ork::event::Broadcaster& bcaster = ork::event::Broadcaster::GetRef();
	SceneInstEvent disviewev( 0, SceneInstEvent::ESIEV_DISABLE_VIEW );
	bcaster.BroadcastNotifyOnChannel( & disviewev, SceneInst::EventChannel() );
}
void SceneEditorBase::EnableViews()
{
	ork::AssertOnOpQ2( UpdateSerialOpQ() );
	ork::ent::DrawableBuffer::ClearAndSyncReaders();
	ork::event::Broadcaster& bcaster = ork::event::Broadcaster::GetRef();
	SceneInstEvent enaviewev( mpEditSceneInst, SceneInstEvent::ESIEV_ENABLE_VIEW );
	bcaster.BroadcastNotifyOnChannel( & enaviewev, SceneInst::EventChannel() );
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ImplEnterRunLocalState()
{
	////////////////////////////////////
	// to prevent deadlock
	ork::AssertOnOpQ2( gImplSerQ );
	////////////////////////////////////

	auto lamb = [&]()
	{
		DisableViews();
		tool::GetGlobalDataFlowScheduler()->GraphSet().LockForWrite().clear();
		ork::ent::DrawableBuffer::ClearAndSyncReaders();
		NewSceneInst();

		if( mpEditSceneInst )
		{
			switch( mpEditSceneInst->GetSceneInstMode() )
			{
				case ent::ESCENEMODE_RUN:
					mpEditSceneInst->SetSceneInstMode( ent::ESCENEMODE_EDIT );
					break;
				default:
					break;
			}
			//////////////////////////////////////////////////////////
			// RELOADABLE ASSETS
			//////////////////////////////////////////////////////////
	#if defined(ORKCONFIG_ASSET_UNLOAD)
			bool loaded = asset::AssetManager<lev2::AudioStream>::AutoLoad();
			     loaded = asset::AssetManager<lev2::XgmAnimAsset>::AutoLoad();
	#endif
				 //////////////////////////////////////////////////////////

			mpEditSceneInst->SetSceneInstMode( ent::ESCENEMODE_RUN );
		}
		ork::ent::DrawableBuffer::ClearAndSyncReaders();
		tool::GetGlobalDataFlowScheduler()->GraphSet().UnLock();
		EnableViews();
	};
	Op(lamb).QueueSync(UpdateSerialOpQ());
}
///////////////////////////////////////////////////////////////////////////
SceneInst* SceneEditorBase::GetEditSceneInst() const
{
	return mpEditSceneInst;
}
///////////////////////////////////////////////////////////////////////////
SceneInst* SceneEditorBase::GetExecSceneInst() const
{
	return mpExecSceneInst;
}
///////////////////////////////////////////////////////////////////////////
SceneInst* SceneEditorBase::GetActiveSceneInst() const
{
	return (mpExecSceneInst!=0) ? mpExecSceneInst : mpEditSceneInst;
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ImplEnterPauseState()
{
	////////////////////////////////////
	// to prevent deadlock
	ork::AssertOnOpQ2( gImplSerQ );
	////////////////////////////////////

	auto lamb = [&]()
	{
		if( mpEditSceneInst )
		{
			mpEditSceneInst->SetSceneInstMode( ent::ESCENEMODE_PAUSE );
		}
	};
	Op(lamb).QueueSync(UpdateSerialOpQ());
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ImplEnterEditState()
{
	////////////////////////////////////
	// to prevent deadlock
	ork::AssertOnOpQ2( gImplSerQ );
	////////////////////////////////////

	auto lamb = [&]()
	{
		DisableViews();

		tool::GetGlobalDataFlowScheduler()->GraphSet().LockForWrite().clear();
		ork::ent::DrawableBuffer::ClearAndSyncReaders();
		NewSceneInst();

		if( mpEditSceneInst )
		{
			//////////////////////////////////////////////////////////
			// UNLOADABLE ASSETS
			//////////////////////////////////////////////////////////
	#if defined(ORKCONFIG_ASSET_UNLOAD)
			ork::lev2::AudioDevice::GetDevice()->ReInitDevice();
			bool unloaded = asset::AssetManager<lev2::AudioStream>::AutoUnLoad();
			     unloaded = asset::AssetManager<lev2::XgmAnimAsset>::AutoUnLoad();
	#endif
			//////////////////////////////////////////////////////////

			mpEditSceneInst->SetSceneInstMode( ent::ESCENEMODE_EDIT );
		}
		ork::ent::DrawableBuffer::ClearAndSyncReaders();
		tool::GetGlobalDataFlowScheduler()->GraphSet().UnLock();

		EnableViews();
	};
	Op(lamb).QueueSync(UpdateSerialOpQ());
}
///////////////////////////////////////////////////////////////////////////
SceneObject* SceneEditorBase::FindSceneObject( const char* pname )
{
	return mpScene->FindSceneObjectByName(AddPooledString(pname));
}
const SceneObject* SceneEditorBase::FindSceneObject( const char* pname ) const
{
	return mpScene->FindSceneObjectByName(AddPooledString(pname));
}
///////////////////////////////////////////////////////////////////////////
bool SceneEditorBase::EditorRenameSceneObject( SceneObject* pobj, const char* pname )
{
	if( mpScene )
	{
		return mpScene->RenameSceneObject( pobj, pname );
	}
	return false;
}
ReferenceArchetype* SceneEditorBase::NewReferenceArchetype( const std::string& archassetname )
{
	ReferenceArchetype* rarch = nullptr;

	std::string str2 = CreateFormattedString( "data://archetypes/%s", archassetname.c_str() );
	std::string ExtRefName = CreateFormattedString( "/arch/ref/%s", archassetname.c_str() );


	ork::Object*pobj = asset::AssetManager<ArchetypeAsset>::Create(str2.c_str());
	asset::AssetManager<ArchetypeAsset>::AutoLoad();

	ArchetypeAsset* passet = rtti::autocast( pobj );

	orkprintf( "asset<%p> pth<%s>\n", passet, str2.c_str() );
	orkprintf( "archname<%s>\n", ExtRefName.c_str() );

	OrkAssert( passet );

	if( passet )
	{
		ReferenceArchetype* rarch = new ReferenceArchetype;
		rarch->SetName(ExtRefName.c_str());
		rarch->SetAsset( passet );
		mpScene->AutoLoadAssets();
		SlotNewObject(rarch);
	}
	return rarch;
}
Archetype* SceneEditorBase::ImplNewArchetype( const std::string& classname, const std::string& name )
{
	////////////////////////////////////
	// to prevent deadlock
	ork::AssertOnOpQ2( gImplSerQ );
	////////////////////////////////////

	if( nullptr == mpScene ) return nullptr;
	Archetype* rarch = nullptr;
	auto lamb = [&]()
	{

		SlotPreNewObject();
		std::string name = CreateFormattedString( "/arch/%s", classname.c_str() );
		ork::rtti::Class* pclass = ork::rtti::Class::FindClassNoCase(classname.c_str());
		printf( "NewArchetype classname<%s> class<%p> aname<%s>\n", classname.c_str(), pclass, name.c_str() );
		if( pclass )
		{
			rarch = rtti::autocast( pclass->CreateObject() );
			rarch->SetName(name.c_str());
			SlotNewObject(rarch);
		}
	};
	Op(lamb).QueueSync(UpdateSerialOpQ());
	return rarch;
}

///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::SigSceneTopoChanged()
{
	auto lamb = [=]()
	{
		ork::ent::DrawableBuffer::ClearAndSyncReaders();
	};

	if( OpqTest::GetContext()->mOPQ == & UpdateSerialOpQ() )
		lamb();
	else
		Op(lamb).QueueASync(UpdateSerialOpQ());

	mSignalSceneTopoChanged(&SceneEditorBase::SigSceneTopoChanged);

//	GetSigModelInvalidated
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::SigNewScene()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	mSignalNewScene( &SceneEditorBase::SigNewScene );
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ClearSelection()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	mSelectionManager.ClearSelection();
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::AddObjectToSelection( ork::Object* pobj )
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	Entity* pent = rtti::downcast<Entity*>( pobj );

	if( pent )
	{
		pobj = const_cast<EntData*>( & pent->GetEntData() );
	}

	mSelectionManager.AddObjectToSelection( pobj );

	/////////////////////////////////////////////////
	object::ObjectClass* pclass = rtti::safe_downcast<object::ObjectClass*>(pobj->GetClass());

	any16 anno = pclass->Description().GetClassAnnotation( "editor.3dxfable" );

	if( anno.IsSet() && anno.Get<bool>() )
	{
		ManipManager().AttachObject( pobj );
	}

	/////////////////////////////////////////////////

	const orkset<ork::Object*>& SelSet = mSelectionManager.GetActiveSelection();
	if( SelSet.size() == 1 )
	{
		ork::Object* pobj = *SelSet.begin();

		if( pobj )
		{
			EntData* pdata = rtti::autocast(pobj);

			if( pdata )
			{

			}
		}
	}

	/////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::GetSelected( orkset<ork::Object*>& SelSet )
{
	SelSet = mSelectionManager.GetActiveSelection();
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ToggleSelection( ork::Object* pobj )
{
	mSelectionManager.ToggleSelection( pobj );
	//bool bsel = mSelectionManager.IsObjectSelected( pobj );
}
///////////////////////////////////////////////////////////////////////////////
// abstract editor has created a new object, there may be more work to do
// that only the scene editor can do
///////////////////////////////////////////////////////////////////////////////

void SceneEditorBase::SlotNewObject( ork::Object* pobj )
{
	if( mpScene )
	{
		SceneObject* pso = rtti::autocast(pobj);

		ent::Archetype* parch = 0;

		if( pso )
		{
			SceneObject* psoe = mpScene->FindSceneObjectByName( pso->GetName() );
			if( false == mpScene->IsSceneObjectPresent(pso) )
			{
				pso->SetName( ork::AddPooledLiteral( pso->GetClass()->GetPreferredName() ) );
				mpScene->AddSceneObject(pso);
			}
			parch = rtti::autocast(pobj);
		}
		else
		{
			ArchetypeAsset* parchasset = rtti::autocast(pobj);
			if( parchasset )
			{
				parch = parchasset->GetArchetype();
			}
		}
		if( parch )
		{
			SceneComposer scene_composer( mpScene );
			{
				parch->Compose(scene_composer);
			}
			mpArchChoices->EnumerateChoices();
			mpScene->AutoLoadAssets();
			//mModel.Attach(NewObject);
		}
	}
	SigSceneTopoChanged();
	//SlotModelInvalidated();
//	SigModelInvalidated();
}

void SceneEditorBase::SlotPreNewObject()
{
	if( mpEditSceneInst )
	{
		mpEditSceneInst->SetSceneInstMode(ESCENEMODE_EDIT);
	}
}

void SceneEditorBase::SlotModelInvalidated()
{
	SigSceneTopoChanged();
}

///////////////////////////////////////////////////////////////////////////////
} }
