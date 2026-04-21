////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/rtti/RTTIX.inl>
#include <ork/object/Object.h>
#include <ork/math/cmatrix4.h>
#include <ork/file/path.h>

#include "types.h"
#include "scene_import_data.h"
#include <ork/kernel/varmap.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

/*struct UpdateStatus {
  UpdateStatus()
      : meStatus(EUPD_RUNNING) {
  }
  EUpdateState meStatus;
  void SetState(EUpdateState est);
  EUpdateState GetState() const {
    return meStatus;
  }
};*/

///////////////////////////////////////////////////////////////////////////////
/// SceneData is the "model" of the scene that is serialized and edited, and thats it....
/// this should never get subclassed
///////////////////////////////////////////////////////////////////////////////

struct SceneData final : public ork::Object {
  DeclareConcreteX(SceneData, ork::Object);

public:
  using SystemDataLut = orkmap<std::string, systemdata_ptr_t>;

  SceneData();
  ~SceneData() final;

  PoolString NewObjectName() const;

  void cleanup();
  void prepareForSimulation();

  //scenedata_ptr_t defaultSetup(opq::opq_ptr_t editopq);

  //////////////////////////////////////////////////////////

  sceneobject_constptr_t findSceneObjectByName(const PoolString& name) const;

  template <typename T>
  std::shared_ptr<T> findTypedSceneObjectByName(const PoolString& name);

  template <typename T>
  std::shared_ptr<const T> findTypedSceneObjectByName(const PoolString& name) const;

  sceneobject_ptr_t findSceneObjectByName(const PoolString& name);
  void AddSceneObject(sceneobject_ptr_t object);
  void RemoveSceneObject(sceneobject_ptr_t object);
  bool RenameSceneObject(sceneobject_ptr_t pobj, const char* pname);
  const orkmap<PoolString, sceneobject_ptr_t>& GetSceneObjects() const {
    return _sceneObjects;
  }
  orkmap<PoolString, sceneobject_ptr_t>& GetSceneObjects() {
    return _sceneObjects;
  }

  bool IsSceneObjectPresent(sceneobject_ptr_t) const;

  ////////////////////////////////////////////////////////////////

  template <typename T> std::shared_ptr<T> createSceneObject(const PoolString& pstr);

  ////////////////////////////////////////////////////////////////

  template <typename T> std::shared_ptr<T> findTypedObject(const PoolString& pstr);
  template <typename T> std::shared_ptr<const T> findTypedObject(const PoolString& pstr) const;

  template <typename T> std::set<spawndata_ptr_t> findEntitiesWithComponent() const;
  template <typename T> std::set<spawndata_ptr_t> findEntitiesOfArchetype() const;

  template <typename T> std::set<std::shared_ptr<const T>> findAllTypedComponents() const;

  //////////////////////////////////////////////////////////

  template <typename T> std::shared_ptr<T> getTypedSystemData();

  const SystemDataLut& getSystemDatas() const {
    return _systemDatas;
  }
  void addSystemData(systemdata_ptr_t pcomp);
  systemdata_ptr_t addSystemWithClassName(std::string clazzname);

  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) final;

  varmap::varmap_ptr_t generateSceneGraphParams() const;

  //////////////////////////////////////////////////////////
  // Rewrite every reference to `old_name` as `new_name` across every
  // SceneGraphSystemData's declared layers and every Archetype's
  // SceneGraphComponentData node declarations. Used after load_scene()
  // to retarget a loaded scene's layer names without touching the
  // .ecs file — call BEFORE start_simulation() / stage_simulation().
  //////////////////////////////////////////////////////////

  void remapLayerName(const std::string& old_name, const std::string& new_name);

  //////////////////////////////////////////////////////////

  orkmap<PoolString, sceneobject_ptr_t> _sceneObjects;
  SystemDataLut _systemDatas;
  file::Path _sceneScriptPath;
  orkmap<std::string, sceneimportdata_ptr_t> _imports;  // namespace → import record
  // Prefix prepended to every scenegraph node-name declared by this
  // scene's archetype SceneGraphComponents and SceneGraphSystem-level
  // nodedatas. Applied once in prepareForSimulation (idempotent). Lets
  // multiple scenes share one lev2 Scene without node-name collisions —
  // downstream consumers (sync_node_state, find-by-name) see the
  // prefixed names directly. Empty = no prefixing.
  std::string _node_prefix;

  void addImport(sceneimportdata_ptr_t import);
  void removeImport(const std::string& ns);
  const orkmap<std::string, sceneimportdata_ptr_t>& getImports() const;
};

///////////////////////////////////////////////////////////////////////////////

struct SceneComposer {

  SceneComposer(SceneData* psd);
  ~SceneComposer();

  template <typename T> std::shared_ptr<T> Register();

  SceneData* GetSceneData() const {
    return _scenedata;
  }


  SceneData* _scenedata = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs {

#include "simulation.h" // temp
