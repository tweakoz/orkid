////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

//#include <ork/object/Object.h>

//#include <ork/kernel/string/ArrayString.h>
#include <ork/lev2/gfx/renderer/drawable.h>

#include <ork/rtti/RTTIX.inl>

#include "types.h"
#include "component.h"
#include "componenttable.h"
#include "componenttable.h"
#include "sceneobject.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

struct SpawnData final : public SceneDagObject {

  DeclareConcreteX(SpawnData, SceneDagObject);

public:
  SpawnData();
  ~SpawnData();

  bool postDeserialize(reflect::serdes::IDeserializer&) final;

  archetype_constptr_t GetArchetype() const {
    return _archetype;
  }
  void SetArchetype(archetype_constptr_t parch);

  void archetypeGetter(object_ptr_t& val) const;
  void archetypeSetter(object_ptr_t const& val);

  template <typename T> std::shared_ptr<const T> typedComponent() const;

  ConstString GetUserProperty(const ConstString& key) const;

  void setAutoSpawn(bool val) { _autospawn=val; }
  bool autoSpawn() const { return _autospawn; }

private:
  bool _autospawn = true;
  archetype_constptr_t _archetype;
  orklut<ConstString, ConstString> mUserProperties;
};

///////////////////////////////////////////////////////////////////////////////
// an INSTANCE of an EntData is an Entity
///////////////////////////////////////////////////////////////////////////////
struct Entity : public lev2::DrawableOwner {

public:
  typedef std::function<fmtx4()> _rendermtxprovider_t;

  ////////////////////////////////////////////////////////////////
  // Component Interface

  const ComponentTable& GetComponents() const;
  ComponentTable& GetComponents();
  Entity* Self() {
    return this;
  }
  void PrintName();
  fvec3 GetEntityPosition() const; // e this should eb gone BUT some entities have NO componenets

  template <typename T> T* typedComponent(bool bsubclass = false);
  template <typename T> const T* typedComponent(bool bsubclass = false) const;

  Component* GetComponentByClass(rtti::Class* clazz);
  Component* GetComponentByClassName(ork::PoolString classname);

  dagnodedata_constptr_t GetDagNode() const {
    return _dagnode;
  }
  dagnodedata_ptr_t GetDagNode() {
    return _dagnode;
  }

  void setTransform(decompxf_ptr_t xf); // set this (Entity) matrix
  void setTransform(const fvec3& pos, const fquat& rot, float uscale); // set this (Entity) matrix
  void setRotAxisAngle(fvec4 axisaa);                                  // set this (Entity) rotation
  void setRotation(fquat rot);                                         // set this (Entity) rotation
  void setPos(fvec3 pos);                                              // set this (Entity) position
  void setScale(float scale);                                              // set this (Entity) position

  spawndata_constptr_t data() const {
    return _entdata;
  }
  void addDrawableToDefaultLayer(lev2::drawable_ptr_t pdrw);
  void addDrawableToLayer(lev2::drawable_ptr_t pdrw, const std::string& layername);

  ////////////////////////////////////////////////////////////////

  xfnode_ptr_t transformNode() {
    return _dagnode->_xfnode;
  }
  decompxf_ptr_t transform() {
    return _dagnode->_xfnode->_transform;
  }
  xfnode_const_ptr_t transformNode() const {
    return _dagnode->_xfnode;
  }
  decompxf_const_ptr_t transform() const {
    return _dagnode->_xfnode->_transform;
  }

  ////////////////////////////////////////////////////////////////

  Entity(spawndata_constptr_t edata, Simulation* inst, int entref);
  ~Entity();

  ////////////////////////////////////////////////////////////////

  void _deleteComponents();
  
  ////////////////////////////////////////////////////////////////

  Simulation* simulation() const {
    return mSimulation;
  }

  PoolString name() const;

  _rendermtxprovider_t _renderMtxProvider = nullptr;
  decompxf_ptr_t _override_initial_xf = nullptr;

  int _entref = -1;

private:
  //void notify(const ComponentEvent& e);

  spawndata_constptr_t _entdata;
  dagnodedata_ptr_t _dagnode;
  Simulation* mSimulation = nullptr;

  mutable bool mComposed = false;

  ComponentTable::LutType _components;
  ComponentTable mComponentTable;

  PoolString _name;

};

///////////////////////////////////////////////////////////////////////////////

struct IEntController {
public:
  virtual void ComposeEntData(SpawnData* pentdata) const         = 0;
  virtual void DeComposeEntData(SpawnData* pentdata) const       = 0;
  virtual Entity* ComposeEntity(const SpawnData* pentdata) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs {
