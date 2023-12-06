////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTypedMap.hpp>

#include <ork/kernel/orklut.hpp>
#include <ork/kernel/opq.h>

#include <ork/ecs/ReferenceArchetype.h>
#include <ork/ecs/scene.h>
#include <ork/ecs/system.h>
#include <ork/ecs/entity.inl>
#include <ork/lev2/imgui/imgui_ged.inl>

template class ork::orklut<const ork::object::ObjectClass*, ork::ecs::systemdata_ptr_t>;

using namespace std::literals;
using namespace ork::reflect;

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::SceneData, "EcsSceneData");

#if defined(ORK_CONFIG_DARWIN)
static dispatch_queue_t TheEditorQueue() {
  static dispatch_queue_t EQ = dispatch_queue_create("com.TweakoZ.OrkTool.EditorQueue", NULL);
  return EQ;
}

dispatch_queue_t EditOnlyQueue() {
  return TheEditorQueue();
}
dispatch_queue_t MainQueue() {
  return dispatch_get_main_queue();
}
#endif

namespace ork {

///////////////////////////////////////////////////////////
// EDITORQUEUE
//   Queue which processes only when scene not running,
//     and ALWAYS from the main thread
///////////////////////////////////////////////////////////

static bool gbQRUNMODE = false;

void EnterRunMode() {
  OrkAssert(false == gbQRUNMODE);
  gbQRUNMODE = true;
#if defined(ORK_CONFIG_DARWIN) // TODO: need to replace GCD on platforms other than DARWIN
  dispatch_sync(
      EditOnlyQueue(),
      ^{
          // printf( "EDITORQ ENTERING RUNMODE, SUSPENDING EDITORQUEUE\n" );
      });
  dispatch_suspend(EditOnlyQueue());
#endif
  // printf( "EDITORQ ENTERING RUNMODE, EDITORQUEUE SUSPENDED\n" );
}
void LeaveRunMode() {
  ////////////////////////////////////////
  bool bRESUME = false;
  if (gbQRUNMODE)
    bRESUME = true;
  gbQRUNMODE = false;
  ////////////////////////////////////////
  // printf( "EDITORQ LEAVING RUNMODE, STARTING EDITORQUEUE...\n" );
#if defined(ORK_CONFIG_DARWIN) // TODO: need to replace GCD on platforms other than DARWIN
  if (bRESUME)
    dispatch_resume(EditOnlyQueue());
  ////////////////////////////////////////
  dispatch_sync(
      EditOnlyQueue(),
      ^{
          // printf( "EDITORQ LEAVING RUNMODE, EDITORQUEUE STARTED.\n" );
      });
#endif
  ////////////////////////////////////////
}

///////////////////////////////////////////////////////////

} // namespace ork

namespace ork {
namespace ecs {
using namespace ::ork;
using namespace ::ork::object;
using namespace ::ork::reflect;
using namespace ::ork::rtti;
using namespace ::ork::lev2::editor;

void SceneData::describeX(ObjectClass* clazz) {

  clazz->annotate("editor.object.props", "ScriptFile SceneObjects SystemData"s);

  /////////////////////
  prophandler_t sohandler = [](const EditorContext& ctx, object_ptr_t obj, const reflect::ObjectProperty* prop){
      using map_t = orkmap<PoolString, sceneobject_ptr_t>;
      imgui::DirectObjectMapPropUI<map_t>(ctx, obj,prop);
    };
  clazz->directObjectMapProperty("SceneObjects", &SceneData::_sceneObjects)
    ->annotate("editor.prop.handler",sohandler);
  /////////////////////
  clazz->directObjectMapProperty("SystemData", &SceneData::_systemDatas)
    ->annotate("editor.factorylistbase", "SystemData");
  /////////////////////

  clazz->directProperty("ScriptFile", &SceneData::_sceneScriptPath)
      ->annotate("editor.class", "ged.factory.filelist")
      ->annotate("editor.filetype", "lua")
      ->annotate("editor.filebase", "src://scripts/");
}
///////////////////////////////////////////////////////////////////////////////
SceneData::SceneData(){
}
///////////////////////////////////////////////////////////////////////////////
SceneData::~SceneData() {
}
///////////////////////////////////////////////////////////////////////////////
sceneobject_ptr_t SceneData::findSceneObjectByName(const PoolString& name) {
  auto it = _sceneObjects.find(name);
  return (it == _sceneObjects.end()) ? 0 : it->second;
}
///////////////////////////////////////////////////////////////////////////////
sceneobject_constptr_t SceneData::findSceneObjectByName(const PoolString& name) const {
  auto it = _sceneObjects.find(name);
  auto o  = (it == _sceneObjects.end()) ? 0 : it->second;
  // printf( "findSceneObject<%s:%p>\n", name.c_str(), o );
  return o;
}

///////////////////////////////////////////////////////////////////////////////
void SceneData::AddSceneObject(sceneobject_ptr_t object) {

  auto it = _sceneObjects.find(object->GetName());
  if(it!=_sceneObjects.end()){

      ArrayString<512> buffer;
      MutableString name_attempt(buffer);
      name_attempt = object->GetName();
      ArrayString<512> basebuffer;
      MutableString basestr(basebuffer);

       //printf( "DupeName AddSceneObject<%s:%p>\n", object->GetName().c_str(), object.get() );

      int counter = 0;

      int i = int(name_attempt.size()) - 1;
      for (; i >= 0; i--)
        if (!isdigit(name_attempt.c_str()[i]))
          break;
      basestr = name_attempt.substr(0, i + 1);

      name_attempt = basestr;
      name_attempt += CreateFormattedString("%d", ++counter).c_str();
      PoolString pooled_name = AddPooledString(name_attempt);
      //while (_sceneObjects.contains(pooled_name)) {
      while (_sceneObjects.find(pooled_name)!=_sceneObjects.end()) {
        name_attempt = basestr;
        name_attempt += CreateFormattedString("%d", ++counter).c_str();
        pooled_name = AddPooledString(name_attempt);
      }
      object->SetName(pooled_name);
  }

   //printf( "AddSceneObject<%s:%p>\n", object->GetName().c_str(), object.get() );
  _sceneObjects.insert(std::make_pair(object->GetName(), object));
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::RemoveSceneObject(sceneobject_ptr_t object) {
  const PoolString& Name = object->GetName();
  auto it                = _sceneObjects.find(Name);
  OrkAssert(it != _sceneObjects.end());
  OrkAssert(it->second == object);
  _sceneObjects.erase(it);
}
///////////////////////////////////////////////////////////////////////////
bool SceneData::IsSceneObjectPresent(sceneobject_ptr_t ptest) const {
  for (auto it : _sceneObjects) {
    sceneobject_ptr_t pso = it.second;
    if (ptest == pso)
      return true;
  }
  return false;
}
///////////////////////////////////////////////////////////////////////////////
PoolString SceneData::NewObjectName() const {
  PoolString rv;
  bool bdone = false;
  while (bdone == false) {
    static int ict      = 0;
    std::string objname = CreateFormattedString("SceneObj%d", ict);
    PoolString test     = FindPooledString(objname.c_str());
    if (test.c_str()) {
      ict++;
    } else {
      rv    = AddPooledString(objname.c_str());
      bdone = true;
    }
  }
  return rv;
}
///////////////////////////////////////////////////////////////////////////////
bool SceneData::RenameSceneObject(sceneobject_ptr_t pobj, const char* pname) {
  _sceneObjects.erase(pobj->GetName());

  PoolString pooled_name = AddPooledString(pname);
  if (_sceneObjects.find(ork::AddPooledString(pname)) != _sceneObjects.end()) {
    ArrayString<512> basebuffer;
    ArrayString<512> buffer;
    MutableString basestr(basebuffer);
    MutableString name_attempt(buffer);
    name_attempt = pname;

    int counter = 0;

    if (name_attempt.size()) {
      int i = int(name_attempt.size()) - 1;
      for (; i >= 0; i--)
        if (!isdigit(name_attempt.c_str()[i]))
          break;
      basestr = name_attempt.substr(0, i + 1);
    }

    name_attempt = basestr;
    name_attempt += CreateFormattedString("%d", ++counter).c_str();
    pooled_name = AddPooledString(name_attempt);
    while (_sceneObjects.find(pooled_name) != _sceneObjects.end()) {
      name_attempt = basestr;
      name_attempt += CreateFormattedString("%d", ++counter).c_str();
      pooled_name = AddPooledString(name_attempt);
    }
  }

  pobj->SetName(pooled_name);
  _sceneObjects.insert(std::make_pair(pooled_name, pobj));

  return true;
}
///////////////////////////////////////////////////////////////////////////
void SceneData::prepareForSimulation() {

  //////////////////////////////
  // delete null objects
  //////////////////////////////

  for (auto it = _sceneObjects.begin(); it != _sceneObjects.end(); it++) {
    if (it->second == 0) {
      _sceneObjects.erase(it);
      it = _sceneObjects.begin();
    }
  }

  //////////////////////////////
  // set object names
  //////////////////////////////
  for (auto it : _sceneObjects) {
    auto sobj = it.second;
    sobj->SetName(it.first);
  }
  //////////////////////////////
  // fixup dagnode hierarchy
  //////////////////////////////
  for (auto it : _sceneObjects ) {
    sceneobject_ptr_t sobj = it.second;
    auto pdag              = std::dynamic_pointer_cast<SceneDagObject>(sobj);
    if (pdag) {
      const PoolString& parname = pdag->GetParentName();
      auto dnode                = pdag->_dagnode;
      if (0 != strcmp(parname.c_str(), "scene")) {
        sceneobject_ptr_t spobj = findSceneObjectByName(parname);
        // OrkAssert( spobj != 0 );
        auto pardag = std::dynamic_pointer_cast<SceneDagObject>(spobj);
        if (pardag) {
          auto pgrp = std::dynamic_pointer_cast<SceneGroup>(spobj);
          if (pgrp) {
            pgrp->addChild(pdag);
          }
        } else {
          // pgrp->addChild( pdag );
        }
      }
    }
  }
  ///////////////////////////////
  // 1 time fixup
  ///////////////////////////////
  for (auto it : _sceneObjects ) {
    sceneobject_ptr_t sobj = it.second;
    auto spawndata              = std::dynamic_pointer_cast<SpawnData>(sobj);
    if (spawndata) {
      auto parch = std::const_pointer_cast<Archetype>(spawndata->GetArchetype());
      if (parch) {
        if (false == IsSceneObjectPresent(parch)) {
          parch->SetName(NewObjectName());
          AddSceneObject(parch);
        }
      }
    }
  }
  //////////////////////////////
  // Recompose Archetypes's
  //////////////////////////////

  SceneComposer scene_composer(this);

  // TODO: Fix this...Archetypes are not in _sceneObjects
  for (auto it : _sceneObjects ) {
    auto sobj    = it.second;
    auto as_arch = std::dynamic_pointer_cast<Archetype>(sobj);
    if (as_arch) {
      //as_arch->compose(scene_composer);
    }
  }
}

bool SceneData::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) {
  cleanup();
  for( auto item : _sceneObjects ){
    auto k = item.first;
    auto v = item.second;
    v->SetName(k.c_str());
  }
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::cleanup() {
  //////////////////////////////////////////
  // clean up dead systems
  //////////////////////////////////////////
  std::set<std::string> keys_to_delete;
  for (auto it : _systemDatas) {
    auto pscd = it.second;
    if (pscd == nullptr) {
      keys_to_delete.insert(it.first);
    }
  }
  for (auto item : keys_to_delete) {
    auto ite = _systemDatas.find(item);
    assert(ite != _systemDatas.end());
    _systemDatas.erase(ite);
  }
  //////////////////////////////////////////
  // create always required systems
  //////////////////////////////////////////
  prepareForSimulation();
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::addSystemData(systemdata_ptr_t pcomp) {
  auto classname = pcomp->GetClass()->Name();
  OrkAssert(_systemDatas.find(classname) == _systemDatas.end());
  printf("ADDSYS<%s:%p>\n", classname.c_str(), (void*)pcomp.get());
  _systemDatas[classname] = pcomp;
}
///////////////////////////////////////////////////////////////////////////////
SceneComposer::SceneComposer(SceneData* psd)
    : _scenedata(psd) {
}
///////////////////////////////////////////////////////////////////////////////
SceneComposer::~SceneComposer() {
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ecs

} // namespace ork
