////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/asset/AssetManager.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/audiobank.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>

#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/register.h>
#include <pkg/ent/ReferenceArchetype.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/CompositingSystem.h>
#include <pkg/ent/LightingSystem.h>

#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/stream/StringInputStream.h>

#include <ork/kernel/opq.h>
#include <ork/kernel/orklut.hpp>
#include <ork/math/basicfilters.h>

#include <pkg/ent/EditorCamera.h>
#include <pkg/ent/ModelArchetype.h>
#include <pkg/ent/ModelComponent.h>
#include "../misc/VrSystem.h"

template class ork::orklut<const ork::object::ObjectClass*, ork::ent::SystemData*>;

using namespace std::literals;
using namespace ork::reflect;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneData, "Ent3dSceneData");

#define VERBOSE 0
#define PRINT_CONDITION (ork::PieceString(pent->GetEntData().GetName()).find("rocket") != ork::PieceString::npos)

#if VERBOSE
#define DEBUG_PRINT                                                                                                                \
  if (PRINT_CONDITION)                                                                                                             \
  orkprintf
#else
#define DEBUG_PRINT(...)
#endif

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
namespace ent {

UpdateStatus gUpdateStatus;

void UpdateStatus::SetState(EUpdateState est) {
  meStatus = est;
}

void SceneData::Describe() {

  RegisterMapProperty("SceneObjects", &SceneData::_sceneObjects);

  auto p = RegisterMapProperty("SystemData", &SceneData::_systemDatas);
  /////////////////////
  // temporary
  //  (we want a custom solution)
  /////////////////////
  annotatePropertyForEditor<SceneData>("SystemData", "editor.factorylistbase", "SystemData");
  /////////////////////

  RegisterProperty("ScriptFile", &SceneData::_sceneScriptPath);
  annotatePropertyForEditor<SceneData>("ScriptFile", "editor.class", "ged.factory.filelist");
  annotatePropertyForEditor<SceneData>("ScriptFile", "editor.filetype", "lua");
  annotatePropertyForEditor<SceneData>("ScriptFile", "editor.filebase", "src://scripts/");

  annotateClassForEditor<SceneData>("editor.object.props", "ScriptFile SystemData"s);
}
///////////////////////////////////////////////////////////////////////////////
SceneData::SceneData()
    : _sceneDataMode(ESCENEDATAMODE_NEW) {
}
///////////////////////////////////////////////////////////////////////////////
SceneData::~SceneData() {
  for (auto it : _sceneObjects) {
    SceneObject* pobj = it.second;
    delete pobj;
  }

  for (auto it : _systemDatas) {
    SystemData* pobj = it.second;
    delete pobj;
  }
}
///////////////////////////////////////////////////////////////////////////////
SceneObject* SceneData::FindSceneObjectByName(const PoolString& name) {
  auto it = _sceneObjects.find(name);
  return (it == _sceneObjects.end()) ? 0 : it->second;
}
///////////////////////////////////////////////////////////////////////////////
const SceneObject* SceneData::FindSceneObjectByName(const PoolString& name) const {
  auto it              = _sceneObjects.find(name);
  const SceneObject* o = (it == _sceneObjects.end()) ? 0 : it->second;
  // printf( "FindSceneObject<%s:%p>\n", name.c_str(), o );
  return o;
}

///////////////////////////////////////////////////////////////////////////////
void SceneData::AddSceneObject(SceneObject* object) {
  ArrayString<512> basebuffer;
  ArrayString<512> buffer;
  MutableString basestr(basebuffer);
  MutableString name_attempt(buffer);
  name_attempt = object->GetName();

  int counter = 0;

  int i = int(name_attempt.size()) - 1;
  for (; i >= 0; i--)
    if (!isdigit(name_attempt.c_str()[i]))
      break;
  basestr = name_attempt.substr(0, i + 1);

  name_attempt = basestr;
  name_attempt += CreateFormattedString("%d", ++counter).c_str();
  PoolString pooled_name = AddPooledString(name_attempt);
  while (_sceneObjects.find(pooled_name) != _sceneObjects.end()) {
    name_attempt = basestr;
    name_attempt += CreateFormattedString("%d", ++counter).c_str();
    pooled_name = AddPooledString(name_attempt);
  }
  object->SetName(pooled_name);

  // printf( "AddSceneObject<%s:%p>\n", object->GetName().c_str(), object );
  _sceneObjects.insert(std::make_pair(object->GetName(), object));
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::RemoveSceneObject(SceneObject* object) {
  const PoolString& Name                        = object->GetName();
  orkmap<PoolString, SceneObject*>::iterator it = _sceneObjects.find(Name);
  OrkAssert(it != _sceneObjects.end());
  OrkAssert(it->second == object);
  _sceneObjects.erase(it);
}
///////////////////////////////////////////////////////////////////////////
bool SceneData::IsSceneObjectPresent(SceneObject* ptest) const {
  for (orkmap<PoolString, SceneObject*>::const_iterator it = _sceneObjects.begin(); it != _sceneObjects.end(); it++) {
    SceneObject* pso = it->second;
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
bool SceneData::RenameSceneObject(SceneObject* pobj, const char* pname) {
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
///////////////////////////////////////////////////////////////////////////////
void SceneData::OnSceneDataMode(ESceneDataMode emode) {
  switch (emode) {
    case ESCENEDATAMODE_NEW:
      break;
    case ESCENEDATAMODE_INIT:
      break;
    case ESCENEDATAMODE_EDIT:
      PrepareForEdit();
      break;
    case ESCENEDATAMODE_RUN:
      break;
  }
  _sceneDataMode = emode;
}
///////////////////////////////////////////////////////////////////////////////

struct MatchNullSceneObject //: public std::unary_function<
                            // std::pair<PoolString, SceneObject* >, bool >
{
  bool operator()(const orkmap<PoolString, SceneObject*>::value_type& item) {
    return item.second == 0;
  }
};

///////////////////////////////////////////////////////////////////////////
void SceneData::PrepareForEdit() {
  AutoLoadAssets();

  //////////////////////////////
  // delete null objects
  //////////////////////////////

  for (orkmap<PoolString, SceneObject*>::iterator cur = _sceneObjects.begin(); cur != _sceneObjects.end(); cur++) {
    if (cur->second == 0) {
      _sceneObjects.erase(cur);
      cur = _sceneObjects.begin();
    }
  }

  //////////////////////////////
  // set object names
  //////////////////////////////
  for (orkmap<PoolString, SceneObject*>::iterator it = _sceneObjects.begin(); it != _sceneObjects.end(); it++) {
    SceneObject* sobj = it->second;
    sobj->SetName(it->first);
  }
  //////////////////////////////
  // fixup dagnode hierarchy
  //////////////////////////////
  for (orkmap<PoolString, SceneObject*>::iterator it = _sceneObjects.begin(); it != _sceneObjects.end(); it++) {
    SceneObject* sobj    = it->second;
    SceneDagObject* pdag = rtti::downcast<SceneDagObject*>(sobj);
    if (pdag) {
      const PoolString& parname = pdag->GetParentName();
      DagNode& dnode            = pdag->GetDagNode();
      if (0 != strcmp(parname.c_str(), "scene")) {
        SceneObject* spobj = this->FindSceneObjectByName(parname);
        // OrkAssert( spobj != 0 );
        SceneDagObject* pardag = rtti::autocast(spobj);
        if (pardag) {
          SceneGroup* pgrp = rtti::autocast(pardag);
          if (pgrp) {
            pgrp->AddChild(pdag);
          }
        } else {
          // pgrp->AddChild( pdag );
        }
      }
    }
  }
  ///////////////////////////////
  // 1 time fixup
  ///////////////////////////////
  for (orkmap<PoolString, SceneObject*>::iterator it = _sceneObjects.begin(); it != _sceneObjects.end(); it++) {
    SceneObject* sobj = it->second;
    EntData* pent     = rtti::downcast<EntData*>(sobj);
    if (pent) {
      Archetype* parch = const_cast<Archetype*>(pent->GetArchetype());
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
  for (orkmap<PoolString, SceneObject*>::iterator it = _sceneObjects.begin(); it != _sceneObjects.end(); it++) {
    SceneObject* sobj = it->second;
    if (Archetype* archetype = rtti::autocast(sobj)) {
      archetype->Compose(scene_composer);
    }
  }
}

void SceneData::AutoLoadAssets() const {
  bool loaded;
  do {
    loaded = false;
    loaded = asset::AssetManager<ArchetypeAsset>::AutoLoad() || loaded;
  } while (loaded);
  lev2::autoloadAssets(false);
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::EnterEditState() {
  OnSceneDataMode(ESCENEDATAMODE_EDIT);
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::EnterInitState() {
  OnSceneDataMode(ESCENEDATAMODE_INIT);
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::EnterRunState() {
  OnSceneDataMode(ESCENEDATAMODE_RUN);
}
///////////////////////////////////////////////////////////////////////////////
bool SceneData::postDeserialize(reflect::serdes::IDeserializer&) {
  cleanup();
  EnterEditState();
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::cleanup() {
  //////////////////////////////////////////
  // clean up dead systems
  //////////////////////////////////////////
  std::set<PoolString> keys_to_delete;
  for (auto it : _systemDatas) {
    const SystemData* pscd = it.second;
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
  auto csdname = "CompositingSystemData"_pool;
  auto its     = _systemDatas.find(csdname);
  if (its == _systemDatas.end()) {
    auto csd = new CompositingSystemData;
    csd->defaultSetup();
    _systemDatas[csdname] = csd;
  }
  auto lsdname = "LightingSystemData"_pool;
  its          = _systemDatas.find(lsdname);
  if (its == _systemDatas.end()) {
    auto lsd              = new LightingSystemData;
    _systemDatas[lsdname] = lsd;
  }
  auto vsdname = "VrSystemData"_pool;
  its          = _systemDatas.find(vsdname);
  if (its == _systemDatas.end()) {
    auto vsd              = new VrSystemData;
    _systemDatas[vsdname] = vsd;
  }
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::defaultSetup(opq::opq_ptr_t editopq) {
  auto updq = opq::updateSerialQueue();
  opq::Op editOP([=]() {
    auto updlock = updq->scopedLock();
    //////////////////////////////////////////
    // do required stuff
    //////////////////////////////////////////
    cleanup();
    auto composer = std::make_shared<SceneComposer>(this);
    //////////////////////////////////////////
    // add a few basic ents
    //////////////////////////////////////////
    auto edcamname     = "spawncam"_pool;
    auto edcamarchname = "/arch/edcam"_pool;
    auto ite           = _sceneObjects.find(edcamname);
    auto ita           = _sceneObjects.find(edcamarchname);
    if (ite == _sceneObjects.end() and ita == _sceneObjects.end()) {
      auto arch                    = new EditorCamArchetype;
      _sceneObjects[edcamarchname] = arch;
      auto ent                     = new EntData;
      ent->SetArchetype(arch);
      _sceneObjects[edcamname] = ent;
      arch->Compose(*composer);
      auto ecd            = arch->GetTypedComponent<EditorCamControllerData>();
      ecd->_camera->mfLoc = 5.0f;
    }
    //////////////////////////////////////////
    auto objectname     = "spawnloc"_pool;
    auto objectarchname = "/arch/object"_pool;
    ite                 = _sceneObjects.find(objectname);
    ita                 = _sceneObjects.find(objectarchname);
    if (ite == _sceneObjects.end() and ita == _sceneObjects.end()) {
      auto arch                     = new ModelArchetype;
      _sceneObjects[objectarchname] = arch;
      auto ent                      = new EntData;
      ent->SetArchetype(arch);
      _sceneObjects[objectname] = ent;
      ////////////////////////////////////////////
      // load model asset
      ////////////////////////////////////////////
      std::shared_ptr<lev2::XgmModelAsset> model_asset;
      opq::Op([&model_asset]() {
        model_asset = asset::AssetManager<lev2::XgmModelAsset>::Load("src://environ/objects/misc/ref/torus");
      }).QueueSync(opq::mainSerialQueue());
      ////////////////////////////////////////////
      // perform edit
      ////////////////////////////////////////////
      arch->Compose(*composer);
      auto mcd = arch->GetTypedComponent<ModelComponentData>();
      mcd->SetModel(model_asset.get());
      ////////////////////////////////////////////
    }
    //////////////////////////////////////////
    AutoLoadAssets();
    cleanup();
    EnterEditState();
  });
  editOP.QueueASync(editopq);
}
///////////////////////////////////////////////////////////////////////////////
void SceneData::addSystemData(SystemData* pcomp) {
  auto classname = pcomp->GetClass()->Name();
  OrkAssert(_systemDatas.find(classname) == _systemDatas.end());
  _systemDatas[classname] = pcomp;
}
///////////////////////////////////////////////////////////////////////////////
SceneComposer::SceneComposer(SceneData* psd)
    : mpSceneData(psd) {
}
///////////////////////////////////////////////////////////////////////////////
SceneComposer::~SceneComposer() {
  /*for (orklut<const ork::object::ObjectClass *, SystemData *>::const_iterator
           it = _systemDatas.begin();
       it != _systemDatas.end(); it++) {
    const ork::object::ObjectClass *pclass = it->first;
    SystemData *psc = ork::rtti::autocast(it->second);
    if (nullptr == psc) {
      OrkAssert(pclass->IsSubclassOf(SystemData::GetClassStatic()));
      psc = ork::rtti::autocast(pclass->CreateObject());
    }
    mpSceneData->addSystemData(psc);
  }*/
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ent

template class orklut<PoolString, const lev2::CameraData*>;

} // namespace ork
