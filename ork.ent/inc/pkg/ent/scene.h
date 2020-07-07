////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/orkstl.h>
#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>
#include <ork/object/ObjectClass.h>
#include <ork/kernel/tempstring.h>
#include <ork/kernel/opq.h>
#include "componentfamily.h"
#include <ork/event/Event.h>
#include <ork/kernel/any.h>
#include <ork/kernel/future.hpp>
#include <ork/math/cmatrix4.h>
#include <ork/file/path.h>
#include "types.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork {
class Application;

namespace lev2 {
class XgmModel;
class XgmModelInst;
class IRenderer;
class LightManager;
class CameraData;
} // namespace lev2

namespace ent {

///////////////////////////////////////////////////////////////////////////////

enum EUpdateState {
  EUPD_STOPPED,
  EUPD_START,
  EUPD_RUNNING,
  EUPD_STOP,
};

struct UpdateStatus {
  UpdateStatus()
      : meStatus(EUPD_RUNNING) {
  }
  EUpdateState meStatus;
  void SetState(EUpdateState est);
  EUpdateState GetState() const {
    return meStatus;
  }
};

extern UpdateStatus gUpdateStatus;

///////////////////////////////////////////////////////////////////////////////

enum ESceneDataMode {
  ESCENEDATAMODE_NEW = 0,
  ESCENEDATAMODE_INIT,
  ESCENEDATAMODE_EDIT,
  ESCENEDATAMODE_RUN,
};

enum ESimulationMode {
  ESCENEMODE_ATTACHED = 0, // attached to a SceneData
  ESCENEMODE_EDIT,         // editing
  ESCENEMODE_RUN,          // running
  ESCENEMODE_SINGLESTEP,   // single stepping
  ESCENEMODE_PAUSE,        // pausing
};

///////////////////////////////////////////////////////////////////////////////
/// SceneData is the "model" of the scene that is serialized and edited, and thats it....
/// this should never get subclassed
///////////////////////////////////////////////////////////////////////////////

class SceneData : public ork::Object {
  RttiDeclareConcrete(SceneData, ork::Object);

public:
  typedef orkmap<PoolString, SystemData*> SystemDataLut;

  SceneData();
  ~SceneData(); /*virtual*/

  ESceneDataMode GetSceneDataMode() const {
    return _sceneDataMode;
  }

  void AutoLoadAssets() const;

  PoolString NewObjectName() const;

  void cleanup();
  void defaultSetup(opq::opq_ptr_t editopq);

  //////////////////////////////////////////////////////////

  const SceneObject* FindSceneObjectByName(const PoolString& name) const;
  SceneObject* FindSceneObjectByName(const PoolString& name);
  void AddSceneObject(SceneObject* object);
  void RemoveSceneObject(SceneObject* object);
  bool RenameSceneObject(SceneObject* pobj, const char* pname);
  const orkmap<PoolString, SceneObject*>& GetSceneObjects() const {
    return _sceneObjects;
  }
  orkmap<PoolString, SceneObject*>& GetSceneObjects() {
    return _sceneObjects;
  }

  bool IsSceneObjectPresent(SceneObject*) const;

  ////////////////////////////////////////////////////////////////

  template <typename T> T* FindTypedObject(const PoolString& pstr);
  template <typename T> const T* FindTypedObject(const PoolString& pstr) const;

  template <typename T> std::set<EntData*> FindEntitiesWithComponent() const;
  template <typename T> std::set<EntData*> FindEntitiesOfArchetype() const;

  //////////////////////////////////////////////////////////

  void EnterEditState();
  void EnterInitState();
  void EnterRunState();

  //////////////////////////////////////////////////////////

  template <typename T> T* getTypedSystemData() const;

  const SystemDataLut& getSystemDatas() const {
    return _systemDatas;
  }
  void addSystemData(SystemData* pcomp);

  void OnSceneDataMode(ESceneDataMode emode);
  void PrepareForEdit();
  bool postDeserialize(reflect::serdes::IDeserializer&) final;

  //////////////////////////////////////////////////////////

  orkmap<PoolString, SceneObject*> _sceneObjects;
  SystemDataLut _systemDatas;
  ESceneDataMode _sceneDataMode;
  file::Path _sceneScriptPath;
};

///////////////////////////////////////////////////////////////////////////////

struct SceneComposer {
  SceneData* mpSceneData;

  template <typename T> T* Register();

  SceneData* GetSceneData() const {
    return mpSceneData;
  }

  SceneComposer(SceneData* psd);
  ~SceneComposer();
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ent
} // namespace ork

#include "simulation.h" // temp
