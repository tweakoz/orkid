////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "scene.h"
#include "archetype.inl"

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ecs {

///////////////////////////////////////////////////////////////////////////////
/// SceneData is the "model" of the scene that is serialized and edited, and
/// thats it.... this should never get subclassed
///////////////////////////////////////////////////////////////////////////////

template <typename T> std::set<spawndata_ptr_t> SceneData::findEntitiesWithComponent() const {
  std::set<spawndata_ptr_t> rval;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::set<spawndata_ptr_t> SceneData::findEntitiesOfArchetype() const {
  std::set<spawndata_ptr_t> rval;

  for (auto item : _sceneObjects) {
    spawndata_ptr_t isa_ent = std::dynamic_pointer_cast<SpawnData>(item.second);

    if (isa_ent) {
      auto arch = isa_ent->GetArchetype();

      if (arch && arch->GetClass() == T::GetClassStatic()) {
        rval.insert(isa_ent);
      }
    }
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::set<std::shared_ptr<const T>> SceneData::findAllTypedComponents() const {
  std::set<std::shared_ptr<const T>> rval;

  for( auto item : _sceneObjects ){
    auto sobj = item.second;
    if( auto as_sdata = std::dynamic_pointer_cast<const SpawnData>(sobj) ){
      auto componentdata = as_sdata->template typedComponent<T>();
      if(componentdata){
        rval.insert(componentdata);
      }
    }
  }

  return rval;
}

template <typename T>
std::shared_ptr<const T> SceneData::findTypedSceneObjectByName(const PoolString& name) const{
  auto so = findSceneObjectByName(name);
  std::shared_ptr<T> rval = dynamic_pointer_cast<const T>(so);
  return rval;
}

template <typename T>
std::shared_ptr<T> SceneData::findTypedSceneObjectByName(const PoolString& name){
  auto so = findSceneObjectByName(name);
  std::shared_ptr<T> rval = dynamic_pointer_cast<T>(so);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> SceneData::createSceneObject(const PoolString& pstr){
  std::shared_ptr<T> rval = std::make_shared<T>();
  rval->SetName(pstr);
  AddSceneObject(rval);
  return rval;  
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> SceneData::findTypedObject(const PoolString& pstr) {
  std::shared_ptr<T> rval;
  auto cd = findSceneObjectByName(pstr);
  if (cd) {
    rval = std::dynamic_pointer_cast<T>(cd);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<const T> SceneData::findTypedObject(const PoolString& pstr) const {
  std::shared_ptr<const T> rval;
  auto cd = findSceneObjectByName(pstr);
  if (cd) {
    rval = std::dynamic_pointer_cast<const T>(cd);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> SceneData::getTypedSystemData() {
  std::shared_ptr<T> rval;
  auto systemdataclass = T::GetClassStatic();
  auto it              = _systemDatas.find(systemdataclass->Name());
  if (it == _systemDatas.end()) {
    // CREATE IT !
    rval                                  = std::make_shared<T>();
    _systemDatas[systemdataclass->Name()] = rval;
  } else {
    rval = std::dynamic_pointer_cast<T>(it->second);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> SceneComposer::Register() {
  std::shared_ptr<T> rval;
  ork::object::ObjectClass* pclass = T::GetClassStatic();
  auto it                          = _scenedata->_systemDatas.find(pclass->Name());
  auto pobj                        = (it == _scenedata->_systemDatas.end()) ? 0 : it->second;
  if (nullptr == pobj) {
    auto X                                   = pclass->createShared();
    rval                                     = std::dynamic_pointer_cast<T>(X);
    _scenedata->_systemDatas[pclass->Name()] = rval;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::ecs
