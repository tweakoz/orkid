////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/util/choiceman.h>
#include <orktool/toolcore/builtinchoices.h>
#include <orktool/toolcore/selection.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <orktool/manip/manip.h>
#include <ork/reflect/Functor.h>
#include <ork/reflect/Command.h>
#include <ork/object/AutoConnector.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/any.h>

namespace ork { namespace dataflow {
struct scheduler;
}} // namespace ork::dataflow

namespace ork { namespace ent {

class SceneEditorBase;

const ork::PoolString& EditorChanName();

class ArchetypeChoices : public ork::util::ChoiceList {
  SceneEditorBase& mSceneEditor;

public:
  virtual void EnumerateChoices(bool bforcenocache = false);

  ArchetypeChoices(SceneEditorBase& SceneEditor);
};

///////////////////////////////////////////////////////////////////////////

class SystemDataChoices : public ork::util::ChoiceList {
  SceneEditorBase& mSceneEditor;

public:
  virtual void EnumerateChoices(bool bforcenocache = false);

  SystemDataChoices(SceneEditorBase& SceneEditor);
};

///////////////////////////////////////////////////////////////////////////

class RefArchetypeChoices : public ork::util::ChoiceList {
public:
  virtual void EnumerateChoices(bool bforcenocache);
  RefArchetypeChoices();
};

///////////////////////////////////////////////////////////////////////////

class SceneEditorBase : public ork::AutoConnector {
  RttiDeclareAbstract(SceneEditorBase, ork::AutoConnector);

  ///////////////////////////////////////////////
  // private signals and slots
  ///////////////////////////////////////////////

  DeclarePublicSignal(SceneTopoChanged);
  DeclarePublicSignal(ObjectDeleted);
  DeclarePublicSignal(NewScene);

  DeclarePublicAutoSlot(ModelInvalidated);
  DeclarePublicAutoSlot(PreNewObject);
  DeclarePublicAutoSlot(NewObject);

  void SigSceneTopoChanged();
  void SigNewScene();
  void SigObjectDeleted(ork::Object* pobj);

  void SlotModelInvalidated();
  void SlotPreNewObject();
  void SlotNewObject(ork::Object* pobj);

  //////////////////////////////////////////////////

  void NewSimulation();

public:
  SceneEditorBase();
  ~SceneEditorBase();

  typedef svar160_t var_t;

  ork::Application* mApplication;
  bool mbInit;
  ork::util::choicemanager_ptr_t _choicemanager;

  std::shared_ptr<tool::ModelChoices> mpMdlChoices;
  std::shared_ptr<tool::ChsmChoices> mpChsmChoices;
  std::shared_ptr<tool::AnimChoices> mpAnmChoices;
  std::shared_ptr<tool::TextureChoices> mpTexChoices;
  std::shared_ptr<tool::ScriptChoices> mpScriptChoices;
  std::shared_ptr<tool::AudioStreamChoices> mpAudStreamChoices;
  std::shared_ptr<tool::AudioBankChoices> mpAudBankChoices;
  std::shared_ptr<tool::FxShaderChoices> mpFxShaderChoices;
  std::shared_ptr<ArchetypeChoices> mpArchChoices;
  std::shared_ptr<RefArchetypeChoices> mpRefArchChoices;

  ent::SceneData* mpScene;

  Simulation* GetActiveSimulation() const;
  Simulation* GetEditSimulation() const;
  Simulation* GetExecSimulation() const;
  SceneData* GetSceneData() const {
    return mpScene;
  }

  ///////////////////////////////////////////////

  // void SetCursor( const ork::fvec3& v3 ) { mCursor=v3; }
  void setSpawnMatrix(const ork::fmtx4& mtx) {
    mSpawnMatrix = mtx;
  }

  ///////////////////////////////////////////////

  const tool::SelectManager& selectionManager() const {
    return mselectionManager;
  }
  tool::SelectManager& selectionManager() {
    return mselectionManager;
  }
  lev2::ManipManager& ManipManager() {
    return mManipManager;
  }

  ///////////////////////////////////////////////

  bool EditorRenameSceneObject(SceneObject* pobj, const char* pname);
  SceneObject* FindSceneObject(const char* pname);
  const SceneObject* FindSceneObject(const char* pname) const;

  ///////////////////////////////////////////////

  ReferenceArchetype* NewReferenceArchetype(const std::string& archassetname);
  Archetype* EditorNewArchetype(const std::string& classname, const std::string& name);
  SystemData* EditorNewSystem(const std::string& classname);

  ///////////////////////////////////////////////

  void EditorLocateEntity(const fmtx4& matrix);
  bool EditorGetEntityLocation(fmtx4& matrix);

  void EditorNewEntities(int count);
  ent::EntData* EditorReplicateEntity();
  void EditorPlaceEntity();
  void EditorGroup();
  void EditorArchExport();
  void EditorArchImport();
  void EditorArchMakeReferenced();
  void EditorArchMakeLocal();
  void EditorDupe();
  void EditorRefreshModels();
  void EditorRefreshAnims();

  void EditorRefreshTextures();
  void RegisterChoices();

  ///////////////////////////////////////////////

  void ClearSelection();
  void ToggleSelection(ork::Object* pobj);
  void AddObjectToSelection(ork::Object* pobj);
  void EditorUnGroup(SceneGroup* pgrp);

  void GetSelected(orkset<ork::Object*>& SelSet);

  ork::fcolor4 GetModColor(const ork::Object* pobj) const;

  void QueueOpASync(const var_t& op);
  void QueueOpSync(const var_t& op);
  void QueueSync();

  ent::EntData* EditorNewEntity(const ent::Archetype* parchetype = NULL);
  void EditorDeleteObject(ork::Object* pobj);

private:
  void DisableViews();
  void EnableViews();
  void DisableUpdates();
  void EnableUpdates();

  ////////////////////////////
  // impl functions must be serialized on the runloop
  ////////////////////////////

  SceneData* ImplNewScene();
  SceneData* ImplGetScene();
  void ImplDeleteObject(ork::Object* pobj);
  EntData* ImplNewEntity(const ent::Archetype* parchetype = NULL);
  Archetype* ImplNewArchetype(const std::string& classname, const std::string& name);
  SystemData* ImplNewSystem(const std::string& classname);
  void ImplLoadScene(std::string filename);
  void ImplEnterRunLocalState();
  void ImplEnterPauseState();
  void ImplEnterEditState();

  void RunLoop();

  int mRunStatus;

  tool::SelectManager mselectionManager;
  lev2::ManipManager mManipManager;

  ork::MpMcBoundedQueue<var_t> mSerialQ;
  ork::fmtx4 mSpawnMatrix;

  Simulation* mpEditSimulation;
};

///////////////////////////////////////////////////////////////////////////////

struct DeleteObjectReq {
  DeleteObjectReq()
      : mObject(nullptr) {
  }
  ork::Object* mObject;
};
struct NewEntityReq {
  typedef std::shared_ptr<NewEntityReq> shared_t;

  NewEntityReq(Future& f = Future::gnilfut)
      : mArchetype(nullptr)
      , mResult(f) {
  }
  const ent::Archetype* mArchetype;
  EntData* GetEntity();
  void SetEntity(EntData* pent);

  static shared_t makeShared(Future& f = Future::gnilfut);

private:
  Future& mResult;
};
struct NewArchReq {
  typedef std::shared_ptr<NewArchReq> shared_t;
  NewArchReq(Future& f = Future::gnilfut)
      : mResult(f) {
  }
  std::string mClassName;
  std::string mName;
  Archetype* GetArchetype();
  void SetArchetype(Archetype* parch);
  static inline shared_t makeShared(Future& f = Future::gnilfut) {
    return std::make_shared<NewArchReq>(f);
  }

private:
  Future& mResult;
};
struct NewSystemReq {
  typedef std::shared_ptr<NewSystemReq> shared_t;
  NewSystemReq(Future& f = Future::gnilfut)
      : mResult(f) {
  }
  std::string mClassName;
  SystemData* system();
  void setSystem(SystemData* sys);
  static inline shared_t makeShared(Future& f = Future::gnilfut) {
    return std::make_shared<NewSystemReq>(f);
  }

private:
  Future& mResult;
};
struct LoadSceneReq {
  void SetOnLoadedOp(const void_lambda_t& l) {
    mOnLoaded.Set<void_lambda_t>(l);
  }
  std::string mFileName;
  const any64& GetOnLoaded() const {
    return mOnLoaded;
  }

private:
  any64 mOnLoaded;
};
struct NewSceneReq {
  typedef std::shared_ptr<NewSceneReq> shared_t;
  NewSceneReq(Future& f = Future::gnilfut)
      : mResult(f) {
  }
  SceneData* GetScene();
  void SetScene(SceneData* parch);
  Future& mResult;
  static inline shared_t makeShared(Future& f = Future::gnilfut) {
    return std::make_shared<NewSceneReq>(f);
  }
};

struct GetSceneReq {
  typedef std::shared_ptr<GetSceneReq> shared_t;
  GetSceneReq(Future& f = Future::gnilfut)
      : mResult(f) {
  }
  SceneData* GetScene();
  void SetScene(SceneData* parch);
  Future& mResult;
  static inline shared_t makeShared(Future& f = Future::gnilfut) {
    return std::make_shared<GetSceneReq>(f);
  }
};
struct RunLocalReq {};
struct StopLocalReq {};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::ent
