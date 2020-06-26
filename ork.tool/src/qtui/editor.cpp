////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <orktool/toolcore/selection.h>

#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/texman.h>
#include <orktool/toolcore/dataflow.h>
#include <orktool/toolcore/selection.h>
#include <pkg/ent/entity.h>
///////////////////////////////////////////////////////////////////////////
#include <pkg/ent/ReferenceArchetype.h>
#include <pkg/ent/editor/editor.h>
#include <pkg/ent/entity.h>
///////////////////////////////////////////////////////////////////////////
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/stream/StringOutputStream.h>
///////////////////////////////////////////////////////////////////////////
#include <ork/reflect/properties/register.h>
///////////////////////////////////////////////////////////////////////////
#include <QtWidgets/QFileDialog>
#include <ork/application/application.h>
#include <ork/kernel/future.hpp>
#include <ork/kernel/opq.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/lev2_asset.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneEditorBase, "SceneEditorBase");

///////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////

NewEntityReq::shared_t NewEntityReq::makeShared(Future& f) {
  return std::make_shared<NewEntityReq>(f);
}

static auto gImplSerQ = std::make_shared<opq::OperationsQueue>(0, "eddummyopq");

static ork::PoolString gEdChanName;

const ork::PoolString& EditorChanName() {
  return gEdChanName;
}

void SceneEditorBase::RegisterChoices() {
}

void SceneEditorBase::Describe() {
  gEdChanName = ork::AddPooledLiteral("Editor");

  ///////////////////////////////////////////////////////////
  RegisterAutoSignal(SceneEditorBase, SceneTopoChanged);
  RegisterAutoSignal(SceneEditorBase, ObjectDeleted);
  RegisterAutoSignal(SceneEditorBase, NewScene);
  ///////////////////////////////////////////////////////////
  RegisterAutoSlot(SceneEditorBase, PreNewObject);
  RegisterAutoSlot(SceneEditorBase, ModelInvalidated);
  RegisterAutoSlot(SceneEditorBase, NewObject);
  ///////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////

void RefArchetypeChoices::EnumerateChoices(bool bforcenocache) {
  clear();
  FindAssetChoices("data://archetypes", "*.mox");
}

///////////////////////////////////////////////////////////////////////////

RefArchetypeChoices::RefArchetypeChoices() {
  EnumerateChoices(true);
}

///////////////////////////////////////////////////////////////////////////

EntData* NewEntityReq::GetEntity() {
  ork::opq::assertNotOnQueue(opq::mainSerialQueue()); // prevent deadlock
  return mResult.GetResult().Get<EntData*>();
}

void NewEntityReq::SetEntity(EntData* pent) {
  ork::opq::assertOnQueue(gImplSerQ);
  mResult.Signal<EntData*>(pent);
}

///////////////////////////////////////////////////////////////////////////

Archetype* NewArchReq::GetArchetype() {
  ork::opq::assertNotOnQueue(opq::mainSerialQueue()); // prevent deadlock
  return mResult.GetResult().Get<Archetype*>();
}

void NewArchReq::SetArchetype(Archetype* parch) {
  ork::opq::assertOnQueue(gImplSerQ);
  mResult.Signal<Archetype*>(parch);
}

///////////////////////////////////////////////////////////////////////////

SystemData* NewSystemReq::system() {
  ork::opq::assertNotOnQueue(opq::mainSerialQueue()); // prevent deadlock
  return mResult.GetResult().Get<SystemData*>();
}

void NewSystemReq::setSystem(SystemData* parch) {
  ork::opq::assertOnQueue(gImplSerQ);
  mResult.Signal<SystemData*>(parch);
}

///////////////////////////////////////////////////////////////////////////

SceneData* NewSceneReq::GetScene() {
  ork::opq::assertNotOnQueue(opq::mainSerialQueue()); // prevent deadlock
  return mResult.GetResult().Get<SceneData*>();
}

void NewSceneReq::SetScene(SceneData* sd) {
  ork::opq::assertOnQueue(gImplSerQ);
  mResult.Signal<SceneData*>(sd);
}

///////////////////////////////////////////////////////////////////////////

SceneData* GetSceneReq::GetScene() {
  ork::opq::assertNotOnQueue(opq::mainSerialQueue()); // prevent deadlock
  return mResult.GetResult().Get<SceneData*>();
}

void GetSceneReq::SetScene(SceneData* sd) {
  ork::opq::assertOnQueue(gImplSerQ);
  mResult.Signal<SceneData*>(sd);
}

///////////////////////////////////////////////////////////////////////////

SceneEditorBase::SceneEditorBase()
    : mbInit(true)
    , mApplication(nullptr)
    , mpScene(nullptr)
    , mpEditSimulation(nullptr)
    , mpMdlChoices(new tool::ModelChoices)
    , mpAnmChoices(new tool::AnimChoices)
    , mpAudStreamChoices(new tool::AudioStreamChoices)
    , mpAudBankChoices(new tool::AudioBankChoices)
    , mpTexChoices(new tool::TextureChoices)
    , mpScriptChoices(new tool::ScriptChoices)
    , mpArchChoices(new ArchetypeChoices(*this))
    , mpChsmChoices(new tool::ChsmChoices)
    , mpRefArchChoices(new RefArchetypeChoices)
    , mpFxShaderChoices(new tool::FxShaderChoices)
    , ConstructAutoSlot(ModelInvalidated)
    , ConstructAutoSlot(PreNewObject)
    , ConstructAutoSlot(NewObject) {
  SetupSignalsAndSlots();

  _choicemanager = std::make_shared<util::ChoiceManager>();

  _choicemanager->AddChoiceList("chsm", mpChsmChoices);
  _choicemanager->AddChoiceList("xgmodel", mpMdlChoices);
  _choicemanager->AddChoiceList("xganim", mpAnmChoices);
  _choicemanager->AddChoiceList("lev2::audiostream", mpAudStreamChoices);
  _choicemanager->AddChoiceList("lev2::audiobank", mpAudBankChoices);
  _choicemanager->AddChoiceList("lev2tex", mpTexChoices);
  _choicemanager->AddChoiceList("script", mpScriptChoices);
  _choicemanager->AddChoiceList("archetype", mpArchChoices);
  _choicemanager->AddChoiceList("refarch", mpRefArchChoices);
  _choicemanager->AddChoiceList("FxShader", mpFxShaderChoices);

  object::Connect(&this->GetSigObjectDeleted(), &mselectionManager.GetSlotObjectDeleted());

  object::Connect(&this->GetSigObjectDeleted(), &mManipManager.GetSlotObjectDeleted());

  object::Connect(&mselectionManager.GetSigObjectSelected(), &mManipManager.GetSlotObjectSelected());

  object::Connect(&mselectionManager.GetSigObjectDeSelected(), &mManipManager.GetSlotObjectDeSelected());

  object::Connect(&mselectionManager.GetSigClearSelection(), &mManipManager.GetSlotClearSelection());

  ///////////////////////////////////////////////////////////////////////////

  struct runloop_impl {
    static void* do_it(void* pdata) {
      SceneEditorBase* seb = (SceneEditorBase*)pdata;
      seb->RunLoop();
      return 0;
    }
  };
  pthread_t thr;
  int istat = pthread_create(&thr, nullptr, runloop_impl::do_it, (void*)this);
  assert(istat == 0);
}

///////////////////////////////////////////////////////////////////////////

SceneEditorBase::~SceneEditorBase() {
  mRunStatus = 1;
  while (mRunStatus == 1) {
    usleep(1000);
  }
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorBase::QueueSync() {
  Future the_future;
  opq::BarrierSyncReq R(the_future);
  QueueOpASync(R);
  the_future.WaitForSignal();
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorBase::QueueOpASync(const var_t& op) {
  mSerialQ.push(op);
}
void SceneEditorBase::QueueOpSync(const var_t& op) {
  QueueOpASync(op);
  QueueSync();
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorBase::RunLoop() {
  SetCurrentThreadName("SceneEdRunLoop");

  opq::TrackCurrent opqt(gImplSerQ);

  var_t event;

  auto updQ = opq::updateSerialQueue();

  auto disable_op = [&]() {
    gUpdateStatus.SetState(EUPD_STOP);
    this->DisableUpdates();
    this->DisableViews();
  };
  auto enable_op = [&]() {
    this->EnableViews();
    this->EnableUpdates();
    gUpdateStatus.SetState(EUPD_START);
  };

  auto do_it = [&]() {
    while (this->mSerialQ.try_pop(event)) {

      printf("SceneEditorBase::mSerialQ gotevent\n");

      if (event.IsA<opq::BarrierSyncReq>()) {
        auto& R = event.Get<opq::BarrierSyncReq>();
        R.mFuture.Signal<bool>(true);
      } else if (event.IsA<LoadSceneReq>()) {
        usleep(2 << 20);
        const auto& R = event.Get<LoadSceneReq>();
        auto& DFLOWG  = tool::GetGlobalDataFlowScheduler()->GraphSet();
        DFLOWG.LockForWrite().clear();
        ImplLoadScene(R.mFileName);
        DFLOWG.UnLock();
        if (R.GetOnLoaded().IsA<void_lambda_t>()) {
          R.GetOnLoaded().Get<void_lambda_t>()();
        }
      } else if (event.IsA<NewSceneReq::shared_t>()) {
        auto req = event.Get<NewSceneReq::shared_t>();
        opq::Op(disable_op).QueueSync(updQ);
        auto s = ImplNewScene();
        opq::Op(enable_op).QueueSync(updQ);
        req->SetScene(s);
      } else if (event.IsA<GetSceneReq::shared_t>()) {
        auto req = event.Get<GetSceneReq::shared_t>();
        opq::Op(disable_op).QueueSync(updQ);
        auto s = ImplGetScene();
        opq::Op(enable_op).QueueSync(updQ);
        req->SetScene(s);
      } else if (event.IsA<RunLocalReq>()) {
        const auto& R = event.Get<RunLocalReq>();
        opq::Op(disable_op).QueueSync(updQ);
        ImplEnterRunLocalState();
        opq::Op(enable_op).QueueSync(updQ);
      } else if (event.IsA<StopLocalReq>()) {
        const auto& R = event.Get<StopLocalReq>();
        opq::Op(disable_op).QueueSync(updQ);
        ImplEnterEditState();
        opq::Op(enable_op).QueueSync(updQ);
      } else if (event.IsA<NewEntityReq::shared_t>()) {
        auto req = event.Get<NewEntityReq::shared_t>();
        opq::Op(disable_op).QueueSync(updQ);
        EntData* pent = ImplNewEntity(req->mArchetype);
        opq::Op(enable_op).QueueSync(updQ);
        req->SetEntity(pent);
      } else if (event.IsA<NewArchReq>()) {
        auto& R = event.Get<NewArchReq>();
        opq::Op(disable_op).QueueSync(updQ);
        auto parch = ImplNewArchetype(R.mClassName, R.mName);
        opq::Op(enable_op).QueueSync(updQ);
        R.SetArchetype(parch);
      } else if (event.IsA<NewSystemReq>()) {
        auto& R = event.Get<NewSystemReq>();
        opq::Op(disable_op).QueueSync(updQ);
        auto system = ImplNewSystem(R.mClassName);
        opq::Op(enable_op).QueueSync(updQ);
        R.setSystem(system);
      } else if (event.IsA<DeleteObjectReq>()) {
        const auto& R = event.Get<DeleteObjectReq>();
        opq::Op(disable_op).QueueSync(updQ);
        ImplDeleteObject(R.mObject);
        opq::Op(enable_op).QueueSync(updQ);
      } else {
        assert(false);
      }
    }
    usleep(1000);
  };

  ///////////////////////////////////////
  // main loop
  ///////////////////////////////////////
  mRunStatus = 0;
  while (mRunStatus == 0) {
    do_it();
  }
  ///////////////////////////////////////
  // empty the queue before exiting
  do_it();
  ///////////////////////////////////////
  mRunStatus = 2;
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorBase::EditorRefreshModels() {
  mpMdlChoices->EnumerateChoices(true);
}
void SceneEditorBase::EditorRefreshAnims() {
  mpAnmChoices->EnumerateChoices(true);
}
void SceneEditorBase::EditorRefreshTextures() {
  mpTexChoices->EnumerateChoices(true);
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::NewSimulation() {
  if (mpEditSimulation) {
    bool BOK = object::Disconnect(
        this, AddPooledLiteral("SigSceneTopoChanged"), mpEditSimulation, AddPooledLiteral("SlotSceneTopoChanged"));
    assert(BOK);
    Simulation* psi2del = mpEditSimulation;
    mpEditSimulation    = nullptr;
    delete psi2del;
  }
  if (mpScene) {
    mpEditSimulation = new Simulation(mpScene, ApplicationStack::Top());
    bool bconOK =
        object::Connect(this, AddPooledLiteral("SigSceneTopoChanged"), mpEditSimulation, AddPooledLiteral("SlotSceneTopoChanged"));
    assert(bconOK);
    mpEditSimulation->SetSimulationMode(ESCENEMODE_EDIT);
  }
}
///////////////////////////////////////////////////////////////////////////
SceneData* SceneEditorBase::ImplGetScene() {
  ////////////////////////////////////
  // to prevent deadlock
  ork::opq::assertOnQueue2(gImplSerQ);
  ////////////////////////////////////
  SceneData* rval   = nullptr;
  auto get_scene_op = [&]() { rval = mpScene; };
  opq::Op(get_scene_op).QueueSync(opq::updateSerialQueue());
  return mpScene;
}
///////////////////////////////////////////////////////////////////////////
SceneData* SceneEditorBase::ImplNewScene() {
  ////////////////////////////////////
  // to prevent deadlock
  ork::opq::assertOnQueue2(gImplSerQ);
  ////////////////////////////////////
  auto new_scene_op = [&]() {
    auto& DFLOWG = tool::GetGlobalDataFlowScheduler()->GraphSet();
    DFLOWG.LockForWrite().clear();

    mselectionManager.ClearSelection();
    ent::SceneData* poldscene = mpScene;
    mpScene                   = new ent::SceneData;
    mpScene->defaultSetup(opq::updateSerialQueue());
    mpArchChoices->EnumerateChoices();
    NewSimulation();
    if (poldscene) {
      delete poldscene;
    }

    mpScene->EnterEditState();

    DFLOWG.UnLock();
    SigNewScene();
    SigSceneTopoChanged();
  };
  opq::Op(new_scene_op).QueueSync(opq::updateSerialQueue());
  return mpScene;
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ImplLoadScene(std::string fname) {
  ////////////////////////////////////
  // to prevent deadlock
  ork::opq::assertOnQueue2(gImplSerQ);
  ////////////////////////////////////

  if (fname.length() == 0)
    return;

  ////////////////////////////////////
  auto pre_load_op = [=]() {
    this->DisableViews();
    lev2::GfxEnv::GetRef().GetGlobalLock().Lock(0x666);
    this->mselectionManager.ClearSelection();
    ent::SceneData* poldscene = this->mpScene;
    this->mpScene             = 0;
    ////////////////////////////////////
    auto load_op = [=]() {
      stream::FileInputStream istream(fname.c_str());
      reflect::serialize::JsonDeserializer iser(istream);
      rtti::ICastable* pcastable = nullptr;
      bool bloadOK               = iser.deserializeObject(pcastable);
      ////////////////////////////////////
      auto post_load_op = [=]() {
        if (bloadOK) {
          ent::SceneData* pscene = rtti::autocast(pcastable);
          if (pscene) {
            this->mpScene = pscene;
            this->mpArchChoices->EnumerateChoices();
          } else {
            OrkNonFatalAssertFunction("Are you sure you are trying to load a scene file? I think not..");
          }
          this->NewSimulation();
          if (poldscene) {
            delete poldscene;
          }
        } else
          mpScene = poldscene;

        lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();

        this->SigNewScene();
        this->SigSceneTopoChanged();
        this->EnableViews();

        printf("YOYOY\n");
        fflush(stdout);
        ork::msleep(1000.0f);
      };
      opq::Op(post_load_op).QueueASync(opq::updateSerialQueue());
    };
    opq::Op(load_op).QueueASync(opq::mainSerialQueue());
  };
  opq::Op(pre_load_op).QueueASync(opq::updateSerialQueue());
  ////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorDupe() {
  SigSceneTopoChanged();
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorGroup() {
  if (mpScene) {
    const orkset<Object*>& SelSet = mselectionManager.getActiveSelection();

    if (SelSet.size()) {
      const float kmax = std::numeric_limits<float>::max();
      float fmaxx      = -kmax;
      float fmaxy      = -kmax;
      float fmaxz      = -kmax;
      float fminx      = kmax;
      float fminy      = kmax;
      float fminz      = kmax;

      for (orkset<Object*>::const_iterator it = SelSet.begin(); it != SelSet.end(); it++) {
        SceneObject* pso = rtti::downcast<SceneObject*>((*it));

        EntData* pentdata  = rtti::downcast<EntData*>(pso);
        SceneGroup* pgroup = rtti::downcast<SceneGroup*>(pso);

        if (pentdata || pgroup) {
          DagNode& Node = pentdata ? pentdata->GetDagNode() : pgroup->GetDagNode();

          fvec3 Pos = Node.GetTransformNode().GetTransform().GetPosition();

          fmaxx = (fmaxx > Pos.GetX()) ? fmaxx : Pos.GetX();
          fmaxy = (fmaxy > Pos.GetY()) ? fmaxy : Pos.GetY();
          fmaxz = (fmaxz > Pos.GetZ()) ? fmaxz : Pos.GetZ();

          fminx = (fminx < Pos.GetX()) ? fminx : Pos.GetX();
          fminy = (fminy < Pos.GetY()) ? fminy : Pos.GetY();
          fminz = (fminz < Pos.GetZ()) ? fminz : Pos.GetZ();
        }
      }

      SceneGroup* pgroup = new SceneGroup;

      fvec3 center((fmaxx + fminx) * 0.5f, (fmaxy + fminy) * 0.5f, (fmaxz + fminz) * 0.5f);

      pgroup->GetDagNode().GetTransformNode().GetTransform().SetPosition(center);

      PoolString ps = mpScene->NewObjectName();
      pgroup->SetName(ps);
      mpScene->AddSceneObject(pgroup);

      for (orkset<Object*>::const_iterator it = SelSet.begin(); it != SelSet.end(); it++) {
        SceneDagObject* pso = rtti::downcast<SceneDagObject*>((*it));

        OrkAssert(pso);

        pgroup->GetDagNode().AddChild(&pso->GetDagNode());
        pgroup->AddChild(pso);
        pso->SetParentName(pgroup->GetName());

        pso->GetDagNode().GetTransformNode().ReParent(&pgroup->GetDagNode().GetTransformNode());
      }

      SigSceneTopoChanged();
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorArchExport() {
  if (mpScene) {
    const orkset<Object*>& selection = mselectionManager.getActiveSelection();
    if (selection.size() > 0) {
      for (orkset<Object*>::const_iterator it = selection.begin(); it != selection.end(); it++)
        if (ent::Archetype* archetype = rtti::autocast(*it))
          if (!archetype->GetClass()->IsSubclassOf(ent::ReferenceArchetype::GetClassStatic())) {
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

            QString FileName =
                QFileDialog::getSaveFileName(0, "Save Archetype File", absassetname.c_str(), "OrkArchetypeFile (*.mox *.mob)");
            file::Path::NameType fname = FileName.toStdString().c_str();
            if (fname.length()) {
              if (FileEnv::filespec_to_extension(fname).length() == 0)
                fname += ".mox";

              stream::FileOutputStream ostream(fname.c_str());
              reflect::serialize::JsonSerializer oser(ostream);
              oser.Serialize(archetype);
            }
          }
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorArchImport() {
  if (mpScene) {
    file::Path assetpath("data://archetypes/");
    file::Path absolutepath = assetpath.ToAbsolute();

    ConstString::size_type pcpos = ConstString(absolutepath.c_str()).find("\\pc\\");

    ArrayString<512> absassetname;
    MutableString mutstr(absassetname);
    mutstr += ConstString(absolutepath.c_str()).substr(0, pcpos);
    mutstr += "\\src\\";
    mutstr += ConstString(absolutepath.c_str()).substr(pcpos + 4);

    QString FileName =
        QFileDialog::getOpenFileName(NULL, "Load OrkArchetypeFile", absassetname.c_str(), "OrkArchetypeFile (*.mox *.mob)");
    std::string fname = FileName.toStdString().c_str();
    if (fname.length()) {
      stream::FileInputStream istream(fname.c_str());
      reflect::serialize::JsonDeserializer iser(istream);

      rtti::ICastable* pcastable = 0;
      bool bOK                   = iser.deserializeObject(pcastable);

      if (ent::Archetype* archetype = rtti::autocast(pcastable)) {
        mpScene->AddSceneObject(archetype);

        SceneComposer scene_composer(mpScene);
        archetype->Compose(scene_composer);

        mpArchChoices->EnumerateChoices();
      }
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorArchMakeReferenced() {
  if (mpScene) {
    const orkset<Object*>& selection = mselectionManager.getActiveSelection();
    if (selection.size() > 0) {
      for (orkset<Object*>::const_iterator it = selection.begin(); it != selection.end(); it++)
        if (ent::Archetype* archetype = rtti::autocast(*it))
          if (!archetype->GetClass()->IsSubclassOf(ent::ReferenceArchetype::GetClassStatic())) {
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

            QString FileName =
                QFileDialog::getSaveFileName(0, "Save Archetype File", absassetname.c_str(), "OrkArchetypeFile (*.mox *.mob)");
            file::Path::NameType fname = FileName.toStdString().c_str();
            if (fname.length()) {
              if (FileEnv::filespec_to_extension(fname).length() == 0)
                fname += ".mox";

              stream::FileOutputStream ostream(fname.c_str());
              reflect::serialize::JsonSerializer oser(ostream);
              oser.Serialize(archetype);
            }
          }
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorArchMakeLocal() {
  if (mpScene) {
    const orkset<Object*>& selection = mselectionManager.getActiveSelection();
    if (selection.size() > 0) {
      for (orkset<Object*>::const_iterator it = selection.begin(); it != selection.end(); it++)
        if (ent::ReferenceArchetype* refarchetype = rtti::autocast(*it))
          if (ent::ArchetypeAsset* archasset = refarchetype->GetAsset())
            if (ent::Archetype* archetype = archasset->GetArchetype()) {
            }
    }
  }
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorUnGroup(SceneGroup* pgrp) {
  pgrp->UnGroupAll();
  mpScene->RemoveSceneObject(pgrp);
  delete (pgrp);
  SigSceneTopoChanged();
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorPlaceEntity() {
  if (mpScene) {
    const orkset<ork::Object*>& selset = selectionManager().getActiveSelection();

    for (orkset<ork::Object*>::const_iterator it = selset.begin(); it != selset.end(); it++) {
      ent::EntData* pentdata = rtti::autocast(*it);

      if (pentdata) {
        pentdata->GetDagNode().GetTransformNode().GetTransform().SetMatrix(mSpawnMatrix);
      }
    }
  }
}

void SceneEditorBase::EditorLocateEntity(const fmtx4& matrix) {
  ent::EntData* pentdata = 0;

  if (mpScene) {
    const orkset<ork::Object*>& selection = selectionManager().getActiveSelection();
    if (selection.size() == 1) {
      ork::Object* pobj = *selection.begin();
      if (ent::EntData* entdata = rtti::autocast(pobj)) {
        entdata->GetDagNode().GetTransformNode().GetTransform().SetMatrix(matrix);
        SigSceneTopoChanged();
      }
    }
  }
}

bool SceneEditorBase::EditorGetEntityLocation(fmtx4& matrix) {
  ent::EntData* pentdata = 0;

  if (mpScene) {
    const orkset<ork::Object*>& selection = selectionManager().getActiveSelection();
    if (selection.size() == 1) {
      ork::Object* pobj = *selection.begin();
      if (ent::EntData* entdata = rtti::autocast(pobj)) {
        matrix = entdata->GetDagNode().GetTransformNode().GetTransform().GetMatrix();
        return true;
      }
    }
  }
  return false;
}
///////////////////////////////////////////////////////////////////////////
ent::EntData* SceneEditorBase::EditorNewEntity(const ent::Archetype* parchetype) {
  Future new_ent;
  auto ner        = NewEntityReq::makeShared(new_ent);
  ner->mArchetype = parchetype;
  QueueOpASync(ner);
  return new_ent.GetResult().Get<EntData*>();
}
///////////////////////////////////////////////////////////////////////////
ent::Archetype* SceneEditorBase::EditorNewArchetype(const std::string& classname, const std::string& name) {
  Future new_arch;
  auto nar        = NewArchReq::makeShared(new_arch);
  nar->mClassName = classname;
  nar->mName      = name;
  QueueOpASync(nar);
  return new_arch.GetResult().Get<Archetype*>();
}
///////////////////////////////////////////////////////////////////////////
SystemData* SceneEditorBase::EditorNewSystem(const std::string& classname) {
  Future new_sys;
  auto nar        = NewSystemReq::makeShared(new_sys);
  nar->mClassName = classname;
  QueueOpASync(nar);
  return new_sys.GetResult().Get<SystemData*>();
}
///////////////////////////////////////////////////////////////////////////
ent::EntData* SceneEditorBase::ImplNewEntity(const ent::Archetype* parchetype) {
  ////////////////////////////////////
  // to prevent deadlock
  ork::opq::assertOnQueue2(gImplSerQ);
  ////////////////////////////////////

  if (nullptr == mpScene)
    return nullptr;

  ent::EntData* pentdata = 0;

  auto lamb = [&]() {
    if (nullptr != parchetype) {
      const orkset<ork::Object*>& selection = selectionManager().getActiveSelection();
      // if archetype selected, assign to new entity
      // if entitiy selected, use its archetype
      if (selection.size() == 1) {
        ork::Object* pobj = *selection.begin();
        EntData* pentdata = rtti::autocast(pobj);
        bool is_ent       = (pentdata != nullptr);
        parchetype        = is_ent ? pentdata->GetArchetype() : rtti::autocast(pobj);
      }
    }

    SlotPreNewObject();

    pentdata = new ent::EntData;
    // pentdata->GetDagNode().GetTransformNode().GetTransform()->SetPosition(mCursor);
    pentdata->GetDagNode().GetTransformNode().GetTransform().SetMatrix(mSpawnMatrix);
    pentdata->SetArchetype(parchetype);

    if (parchetype && ConstString(parchetype->GetName()).find("/arch/") != ConstString::npos) {
      PieceString name = PieceString(parchetype->GetName()).substr(6);
      pentdata->SetName(ork::AddPooledString(name));
    } else
      pentdata->SetName(ork::AddPooledLiteral(ent::EntData::GetClassStatic()->GetPreferredName()));

    mpScene->AddSceneObject(pentdata);

    ////////////////////
    // create an instance for the editor to draw
    ////////////////////

    ent::Entity* pent = new ent::Entity(pentdata, mpEditSimulation);

    if (parchetype) {
      parchetype->ComposeEntity(pent);
    }

    mpEditSimulation->SetEntity(pentdata, pent);
    mpEditSimulation->ActivateEntity(pent);

    ////////////////////
    SigSceneTopoChanged();
    ClearSelection();
    AddObjectToSelection(pentdata);
  };
  opq::Op(lamb).QueueSync(opq::updateSerialQueue());
  return pentdata;
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorNewEntities(int count) {
  if (mpScene) {
    ent::Archetype* archetype             = NULL;
    const orkset<ork::Object*>& selection = selectionManager().getActiveSelection();
    if (selection.size() == 1) {
      ork::Object* pobj = *selection.begin();
      archetype         = rtti::autocast(pobj);
    }

    if (count > 0)
      for (int i = 0; i < count; i++)
        EditorNewEntity(archetype);
  }
}
///////////////////////////////////////////////////////////////////////////
ent::EntData* SceneEditorBase::EditorReplicateEntity() {
  ent::EntData* pentdata = 0;

  if (mpScene) {
    const ent::Archetype* archetype = NULL;
    fquat rotation;
    std::string name;

    const orkset<ork::Object*>& selection = selectionManager().getActiveSelection();
    if (selection.size() == 1) {
      ork::Object* pobj = *selection.begin();
      archetype         = rtti::autocast(pobj);
      if (!archetype)
        if (ent::EntData* entdata = rtti::autocast(pobj)) {
          archetype = entdata->GetArchetype();
          rotation  = entdata->GetDagNode().GetTransformNode().GetTransform().GetRotation();
          name      = entdata->GetName().c_str();
        }
    }

    SlotPreNewObject();

    pentdata = new ent::EntData;

    ork::fvec3 cursor_pos = mSpawnMatrix.GetTranslation();

    pentdata->GetDagNode().GetTransformNode().GetTransform().SetPosition(cursor_pos);
    pentdata->GetDagNode().GetTransformNode().GetTransform().SetRotation(rotation);
    pentdata->SetArchetype(archetype);

    if (name.empty())
      pentdata->SetName(ork::AddPooledLiteral(ent::EntData::GetClassStatic()->GetPreferredName()));
    else
      pentdata->SetName(name.c_str());
    mpScene->AddSceneObject(pentdata);

    ////////////////////
    // create an instance for the editor to draw
    ////////////////////

    ent::Entity* pent = new ent::Entity(pentdata, mpEditSimulation);
    if (archetype) {
      archetype->ComposeEntity(pent);
      archetype->LinkEntity(mpEditSimulation, pent);
    }
    mpEditSimulation->SetEntity(pentdata, pent);
    mpEditSimulation->ActivateEntity(pent);

    ////////////////////
    SigSceneTopoChanged();
    ClearSelection();
    AddObjectToSelection(pentdata);
  }
  return pentdata;
}

///////////////////////////////////////////////////////////////////////////
// query if an object references an archetype
///////////////////////////////////////////////////////////////////////////

bool QueryArchetypeReferenced(ork::Object* pobj, const ent::Archetype* parch) {
  bool brval = false;

  /////////////////////////////

  FixedString<32> ArchSource;
  ArchSource.format("%08x", parch);

  /////////////////////////////
  const reflect::IObjectFunctor* functor =
      rtti::downcast<object::ObjectClass*>(pobj->GetClass())->Description().FindFunctor("SlotArchetypeReferenced");
  if (functor) {
    reflect::IInvokation* invokation = functor->CreateInvokation();
    if (invokation->GetNumParameters() == 1) {
      bool bok = reflect::SetInvokationParameter(invokation, 0, ArchSource.c_str());
      OrkAssert(bok);

      ArrayString<32> resultdata;
      stream::StringOutputStream ostream(resultdata);
      reflect::serialize::JsonSerializer serializer(ostream);
      reflect::BidirectionalSerializer result_bidi(serializer);
      functor->invoke(pobj, invokation, &result_bidi);

      const char* presult = resultdata.c_str();

      if (0 == strcmp("true", presult)) {
        return true;
      }
    }
    delete invokation;
  }
  return brval;
}

///////////////////////////////////////////////////////////////////////////

ork::fcolor4 SceneEditorBase::GetModColor(const ork::Object* pobj) const {
  const Entity* pent                        = rtti::autocast(pobj);
  const ent::EntData* pentdata              = pent->data();
  const ent::Archetype* parch               = pentdata->GetArchetype();
  const ork::tool::SelectManager& selectmgr = selectionManager();

  if (pent) {
    const float finvsaturation = 0.3f;

    if (selectmgr.IsObjectSelected(pentdata)) {
      return ork::fvec4(1.0f, finvsaturation, finvsaturation, 1.0f);
    } else if (selectmgr.IsObjectSelected(parch)) {
      return ork::fvec4(finvsaturation, finvsaturation, 1.0f, 1.0f);
    } else if (parch) // is any archetype indirectly referenced by this entity selected (via spawner)
    {
      const orkset<ork::Object*>& selset = selectmgr.getActiveSelection();

      if (1 == selset.size()) {
        const ent::Archetype* prefarch = rtti::autocast(*selset.begin());

        if (prefarch) {
          const ent::ComponentDataTable::LutType& clut = parch->GetComponentDataTable().GetComponents();
          for (ent::ComponentDataTable::LutType::const_iterator it = clut.begin(); it != clut.end(); it++) {
            ent::ComponentData* pcompdata = it->second;
            if (pcompdata) {
              bool bisref = QueryArchetypeReferenced(pcompdata, prefarch);

              if (bisref) {
                return ork::fvec4(finvsaturation, 1.0f, finvsaturation, 1.0f);
              }
            }
          }
        }
      }
    }
  }
  return ork::fcolor4::White();
}

///////////////////////////////////////////////////////////////////////////
// notify an object that an archetype has been deleted (IF the object cares)
///////////////////////////////////////////////////////////////////////////

void DynamicSignalArchetypeDeleted(ork::Object* pobj, ent::Archetype* parch) {
  /////////////////////////////

  FixedString<32> ArchSource;
  ArchSource.format("%08x", parch);

  /////////////////////////////
  const reflect::IObjectFunctor* functor =
      rtti::downcast<object::ObjectClass*>(pobj->GetClass())->Description().FindFunctor("SlotArchetypeDeleted");
  if (functor) {
    reflect::IInvokation* invokation = functor->CreateInvokation();
    if (invokation->GetNumParameters() == 1) {
      bool bok = reflect::SetInvokationParameter(invokation, 0, ArchSource.c_str());
      OrkAssert(bok);

      ArrayString<32> resultdata;
      stream::StringOutputStream ostream(resultdata);
      reflect::serialize::JsonSerializer serializer(ostream);
      reflect::BidirectionalSerializer result_bidi(serializer);
      functor->invoke(pobj, invokation, &result_bidi);
    }
    delete invokation;
  }
}

///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::EditorDeleteObject(ork::Object* pobj) {
  printf("EditorDeleteObject pobj<%p>\n", pobj);

  DeleteObjectReq R;
  R.mObject = pobj;
  QueueOpASync(R);
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ImplDeleteObject(ork::Object* pobj) {
  ////////////////////////////////////
  // to prevent deadlock
  ork::opq::assertOnQueue2(gImplSerQ);
  ////////////////////////////////////
  if (nullptr == mpScene)
    return;

  auto lamb = [=]() {
    SlotPreNewObject();

    /////////////////////////////////////////
    SceneObject* psobj = rtti::downcast<SceneObject*>(pobj);

    printf("EDITORIMPLDELETE pobj<%p> psobj<%p>\n", pobj, psobj);

    if (nullptr == psobj)
      return;

    ork::lev2::DrawableBuffer::ClearAndSyncReaders();

    OrkAssert(psobj);
    mpScene->RemoveSceneObject(psobj);
    /////////////////////////////////////////
    // Has an archetype has been deleted?
    /////////////////////////////////////////
    ent::Archetype* parch = rtti::autocast(pobj);
    if (parch) {
      mpArchChoices->EnumerateChoices();

      orkmap<PoolString, SceneObject*>& sobjs = mpScene->GetSceneObjects();

      for (orkmap<PoolString, SceneObject*>::const_iterator it = sobjs.begin(); it != sobjs.end(); it++) {
        SceneObject* itsobj = it->second;

        Archetype* otharch = rtti::autocast(itsobj);

        if (otharch && otharch != parch) {
          const ent::ComponentDataTable::LutType& clut = otharch->GetComponentDataTable().GetComponents();
          for (ent::ComponentDataTable::LutType::const_iterator it = clut.begin(); it != clut.end(); it++) {
            ent::ComponentData* pcompdata = it->second;
            if (pcompdata) {
              DynamicSignalArchetypeDeleted(pcompdata, parch);
            }
          }
        }
        DynamicSignalArchetypeDeleted(itsobj, parch);
      }
    }
    /////////////////////////////////////////
    // notify all listeners that this object is getting deleted
    // SigObjectDeleted(pobj);
    mSignalObjectDeleted(&SceneEditorBase::EditorDeleteObject, pobj);
    /////////////////////////////////////////
    delete (psobj);
    /////////////////////////////////////////
    SigSceneTopoChanged();
  };
  opq::Op(lamb).QueueASync(opq::updateSerialQueue());
}
void SceneEditorBase::DisableUpdates() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  ork::lev2::DrawableBuffer::ClearAndSyncReaders();
  msgrouter::channel("Simulation")->postType<SimulationEvent>(nullptr, SimulationEvent::ESIEV_DISABLE_UPDATE);
}
void SceneEditorBase::EnableUpdates() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  ork::lev2::DrawableBuffer::ClearAndSyncReaders();
  msgrouter::channel("Simulation")->postType<SimulationEvent>(nullptr, SimulationEvent::ESIEV_ENABLE_UPDATE);
}
void SceneEditorBase::DisableViews() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  ork::lev2::DrawableBuffer::ClearAndSyncReaders();
  msgrouter::channel("Simulation")->postType<SimulationEvent>(nullptr, SimulationEvent::ESIEV_DISABLE_VIEW);
}
void SceneEditorBase::EnableViews() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  ork::lev2::DrawableBuffer::ClearAndSyncReaders();
  msgrouter::channel("Simulation")->postType<SimulationEvent>(mpEditSimulation, SimulationEvent::ESIEV_ENABLE_VIEW);
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ImplEnterRunLocalState() {
  ////////////////////////////////////
  // to prevent deadlock
  ork::opq::assertOnQueue2(gImplSerQ);
  ////////////////////////////////////

  auto lamb = [&]() {
    DisableViews();
    tool::GetGlobalDataFlowScheduler()->GraphSet().LockForWrite().clear();
    ork::lev2::DrawableBuffer::ClearAndSyncReaders();
    NewSimulation();

    if (mpEditSimulation) {
      switch (mpEditSimulation->GetSimulationMode()) {
        case ent::ESCENEMODE_RUN:
          mpEditSimulation->SetSimulationMode(ent::ESCENEMODE_EDIT);
          break;
        default:
          break;
      }
        //////////////////////////////////////////////////////////
        // RELOADABLE ASSETS
        //////////////////////////////////////////////////////////
#if defined(ORKCONFIG_ASSET_UNLOAD)
      bool loaded = asset::AssetManager<lev2::AudioStream>::AutoLoad();
      loaded      = asset::AssetManager<lev2::XgmAnimAsset>::AutoLoad();
#endif
      //////////////////////////////////////////////////////////

      mpEditSimulation->SetSimulationMode(ent::ESCENEMODE_RUN);
    }
    ork::lev2::DrawableBuffer::ClearAndSyncReaders();
    tool::GetGlobalDataFlowScheduler()->GraphSet().UnLock();
    EnableViews();
    msgrouter::channel("Simulation")->postType<SimulationEvent>(mpEditSimulation, SimulationEvent::ESIEV_ENABLE_VIEW);
  };
  opq::Op(lamb).QueueSync(opq::updateSerialQueue());
}
///////////////////////////////////////////////////////////////////////////
Simulation* SceneEditorBase::GetEditSimulation() const {
  return mpEditSimulation;
}
///////////////////////////////////////////////////////////////////////////
Simulation* SceneEditorBase::GetActiveSimulation() const {
  return mpEditSimulation;
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ImplEnterPauseState() {
  ////////////////////////////////////
  // to prevent deadlock
  ork::opq::assertOnQueue2(gImplSerQ);
  ////////////////////////////////////

  auto lamb = [&]() {
    if (mpEditSimulation) {
      mpEditSimulation->SetSimulationMode(ent::ESCENEMODE_PAUSE);
    }
  };
  opq::Op(lamb).QueueSync(opq::updateSerialQueue());
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ImplEnterEditState() {
  ////////////////////////////////////
  // to prevent deadlock
  ork::opq::assertOnQueue2(gImplSerQ);
  ////////////////////////////////////

  auto lamb = [&]() {
    DisableViews();

    tool::GetGlobalDataFlowScheduler()->GraphSet().LockForWrite().clear();
    ork::lev2::DrawableBuffer::ClearAndSyncReaders();
    NewSimulation();

    if (mpEditSimulation) {
      //////////////////////////////////////////////////////////
      // UNLOADABLE ASSETS
      //////////////////////////////////////////////////////////
#if defined(ORKCONFIG_ASSET_UNLOAD)
      ork::lev2::AudioDevice::GetDevice()->ReInitDevice();
      bool unloaded = asset::AssetManager<lev2::AudioStream>::AutoUnLoad();
      unloaded      = asset::AssetManager<lev2::XgmAnimAsset>::AutoUnLoad();
#endif
      //////////////////////////////////////////////////////////

      mpEditSimulation->SetSimulationMode(ent::ESCENEMODE_EDIT);
    }
    ork::lev2::DrawableBuffer::ClearAndSyncReaders();
    tool::GetGlobalDataFlowScheduler()->GraphSet().UnLock();

    EnableViews();
  };
  opq::Op(lamb).QueueSync(opq::updateSerialQueue());
}
///////////////////////////////////////////////////////////////////////////
SceneObject* SceneEditorBase::FindSceneObject(const char* pname) {
  return mpScene->FindSceneObjectByName(AddPooledString(pname));
}
const SceneObject* SceneEditorBase::FindSceneObject(const char* pname) const {
  return mpScene->FindSceneObjectByName(AddPooledString(pname));
}
///////////////////////////////////////////////////////////////////////////
bool SceneEditorBase::EditorRenameSceneObject(SceneObject* pobj, const char* pname) {
  if (mpScene) {
    return mpScene->RenameSceneObject(pobj, pname);
  }
  return false;
}
ReferenceArchetype* SceneEditorBase::NewReferenceArchetype(const std::string& archassetname) {
  ReferenceArchetype* rarch = nullptr;

  std::string str2       = CreateFormattedString("data://archetypes/%s", archassetname.c_str());
  std::string ExtRefName = CreateFormattedString("/arch/ref/%s", archassetname.c_str());

  auto arch_asset = asset::AssetManager<ArchetypeAsset>::Create(str2.c_str());
  asset::AssetManager<ArchetypeAsset>::AutoLoad();

  orkprintf("asset<%p> pth<%s>\n", arch_asset.get(), str2.c_str());
  orkprintf("archname<%s>\n", ExtRefName.c_str());

  OrkAssert(arch_asset);

  if (arch_asset) {
    ReferenceArchetype* rarch = new ReferenceArchetype;
    rarch->SetName(ExtRefName.c_str());
    rarch->SetAsset(arch_asset.get());
    mpScene->AutoLoadAssets();
    SlotNewObject(rarch);
  }
  return rarch;
}
Archetype* SceneEditorBase::ImplNewArchetype(const std::string& classname, const std::string& name) {
  ////////////////////////////////////
  // to prevent deadlock
  ork::opq::assertOnQueue2(gImplSerQ);
  ////////////////////////////////////

  if (nullptr == mpScene)
    return nullptr;
  Archetype* rarch = nullptr;
  auto lamb        = [&]() {
    SlotPreNewObject();
    std::string name         = CreateFormattedString("/arch/%s", classname.c_str());
    ork::rtti::Class* pclass = ork::rtti::Class::FindClassNoCase(classname.c_str());
    auto pclazz              = dynamic_cast<const object::ObjectClass*>(pclass);
    printf("NewArchetype classname<%s> pclazz<%p> aname<%s>\n", classname.c_str(), pclazz, name.c_str());
    if (pclazz) {
      rarch = rtti::autocast(pclazz->CreateObject());
      rarch->SetName(name.c_str());
      SlotNewObject(rarch);
    }
  };
  opq::Op(lamb).QueueSync(opq::updateSerialQueue());
  return rarch;
}
SystemData* SceneEditorBase::ImplNewSystem(const std::string& classname) { ////////////////////////////////////
  // to prevent deadlock
  ork::opq::assertOnQueue2(gImplSerQ);
  ////////////////////////////////////
  if (nullptr == mpScene)
    return nullptr;
  SystemData* system = nullptr;
  auto lamb          = [&]() {
    SlotPreNewObject();
    auto pclass = ork::rtti::Class::FindClassNoCase(classname.c_str());
    auto pclazz = dynamic_cast<const object::ObjectClass*>(pclass);
    printf("NewSystem classname<%s> pclazz<%p>\n", classname.c_str(), pclazz);
    if (pclazz) {
      system = rtti::autocast(pclazz->CreateObject());
      SlotNewObject(system);
    }
  };
  opq::Op(lamb).QueueSync(opq::updateSerialQueue());
  return system;
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::SigSceneTopoChanged() {
  auto lamb = [=]() { ork::lev2::DrawableBuffer::ClearAndSyncReaders(); };

  if (opq::TrackCurrent::is(opq::updateSerialQueue())) {
    // we are already on update thread, just execute the lambda directly
    lamb();
  } else {
    // enqueue lambda on update thread
    opq::Op(lamb).QueueASync(opq::updateSerialQueue());
  }
  mSignalSceneTopoChanged(&SceneEditorBase::SigSceneTopoChanged);

  //	GetSigModelInvalidated
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::SigNewScene() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  mSignalNewScene(&SceneEditorBase::SigNewScene);
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ClearSelection() {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  mselectionManager.ClearSelection();
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::AddObjectToSelection(ork::Object* pobj) {
  ork::opq::assertOnQueue2(opq::updateSerialQueue());
  Entity* pent = rtti::downcast<Entity*>(pobj);

  if (pent) {
    pobj = const_cast<EntData*>(pent->data());
  }

  mselectionManager.AddObjectToSelection(pobj);

  /////////////////////////////////////////////////
  object::ObjectClass* pclass = rtti::safe_downcast<object::ObjectClass*>(pobj->GetClass());

  auto anno = pclass->Description().classAnnotation("editor.3dxfable");

  if (anno.IsSet() && anno.Get<bool>()) {
    ManipManager().AttachObject(pobj);
  }

  /////////////////////////////////////////////////

  const orkset<ork::Object*>& SelSet = mselectionManager.getActiveSelection();
  if (SelSet.size() == 1) {
    ork::Object* pobj = *SelSet.begin();

    if (pobj) {
      EntData* pdata = rtti::autocast(pobj);

      if (pdata) {
      }
    }
  }

  /////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::GetSelected(orkset<ork::Object*>& SelSet) {
  SelSet = mselectionManager.getActiveSelection();
}
///////////////////////////////////////////////////////////////////////////
void SceneEditorBase::ToggleSelection(ork::Object* pobj) {
  mselectionManager.ToggleSelection(pobj);
  // bool bsel = mselectionManager.IsObjectSelected( pobj );
}
///////////////////////////////////////////////////////////////////////////////
// abstract editor has created a new object, there may be more work to do
// that only the scene editor can do
///////////////////////////////////////////////////////////////////////////////

void SceneEditorBase::SlotNewObject(ork::Object* pobj) {
  if (mpScene) {
    SceneObject* pso = rtti::autocast(pobj);

    ent::Archetype* parch = 0;

    if (pso) {
      SceneObject* psoe = mpScene->FindSceneObjectByName(pso->GetName());
      if (false == mpScene->IsSceneObjectPresent(pso)) {
        pso->SetName(ork::AddPooledLiteral(pso->GetClass()->GetPreferredName()));
        mpScene->AddSceneObject(pso);
      }
      parch = rtti::autocast(pobj);
    } else {
      ArchetypeAsset* parchasset = rtti::autocast(pobj);
      if (parchasset) {
        parch = parchasset->GetArchetype();
      }
    }
    if (parch) {
      SceneComposer scene_composer(mpScene);
      { parch->Compose(scene_composer); }
      mpArchChoices->EnumerateChoices();
      mpScene->AutoLoadAssets();
      // mModel.Attach(NewObject);
    }
  }
  SigSceneTopoChanged();
  // SlotModelInvalidated();
  //	SigModelInvalidated();
}

void SceneEditorBase::SlotPreNewObject() {
  StopLocalReq();
}

void SceneEditorBase::SlotModelInvalidated() {
  SigSceneTopoChanged();
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
