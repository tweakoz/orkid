////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include "componentfamily.h"
#include <ork/event/Event.h>
#include <ork/math/cmatrix4.h>
#include <ork/object/Object.h>
#include "system.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

class ComponentInst;
class Entity;
class SceneInst;
class ComponentDataTable;
class SceneComposer;

///////////////////////////////////////////////////////////////////////////////

class ComponentDataClass : public object::ObjectClass {
  RttiDeclareExplicit(ComponentDataClass, object::ObjectClass, rtti::NamePolicy,
                      object::ObjectCategory);

public:
  ComponentDataClass(const rtti::RTTIData &);

  PoolString GetFamily() const { return mFamily; }
  void SetFamily(PoolString family) { mFamily = family; }

private:
  PoolString mFamily;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> void RegisterFamily(PoolString family) {
  auto clazz = T::GetClassStatic();
  if( auto as_cdc = dynamic_cast<ComponentDataClass*>(clazz) ){
    as_cdc->SetFamily(family);
  }
  else if( auto as_sdc = dynamic_cast<SystemDataClass*>(clazz) ){
    as_sdc->SetFamily(family);
  }
  else {
    assert(false);
  }
}

///////////////////////////////////////////////////////////////////////////////

class ComponentData : public Object {
  RttiDeclareExplicit(ComponentData, Object, rtti::AbstractPolicy, ComponentDataClass);

public:
  ComponentData();

  virtual ComponentInst *createComponent(Entity *pent) const = 0;

  PoolString GetFamily() const;

  void RegisterWithScene(SceneComposer &sc) { DoRegisterWithScene(sc); }

  virtual const char *GetShortSelector() const { return 0; }

private:
  virtual void DoRegisterWithScene(SceneComposer &sc) {}
};

///////////////////////////////////////////////////////////////////////////////

struct ComponentQuery {
    std::string _eventID;
    svar64_t _eventData;
};

class ComponentInst : public Object {
  RttiDeclareAbstract(ComponentInst, Object);
public:
  void SetEntity(Entity *entity) { mEntity = entity; }
  Entity *GetEntity() { return mEntity; }
  const Entity *GetEntity() const { return mEntity; }
  SceneInst *sceneInst() const;
  // Shortcut to make debugging printfs easier
  const char *GetEntityName() const;

  virtual const char* friendlyName() {
      return GetClass()->Name().c_str();
  }

  PoolString GetFamily() const;

  void Update(SceneInst *inst);
  void Start(SceneInst *psi, const fmtx4 &world);
  void Link(SceneInst *psi);
  void UnLink(SceneInst *psi);
  void Stop(SceneInst *psi); // { DoStop(psi); }
  void activate(SceneInst *psi) { onActivate(psi); }
  void deactivate(SceneInst *psi) { onDeactivate(psi); }

  const char *GetShortSelector() const {
    return (mComponentData != 0) ? mComponentData->GetShortSelector() : 0;
  }

  svar64_t query(const ComponentQuery& q) { return doQuery(q); }

protected:
  ComponentInst(const ComponentData *data, Entity *entity);

  Entity *mEntity;

private:
  bool DoNotify(const ork::event::Event *event) override { return false; }
  virtual svar64_t doQuery(const ComponentQuery& q) { return svar64_t(); }

  virtual void DoUpdate(SceneInst *inst) {}
  virtual bool DoStart(SceneInst *psi, const fmtx4 &world) { return true; }
  virtual void onActivate(SceneInst *psi) {}
  virtual void onDeactivate(SceneInst *psi) {}
  virtual bool DoLink(SceneInst *psi) { return true; }
  virtual void DoUnLink(SceneInst *psi) {}
  virtual void DoStop(SceneInst *psi) {}

  const ComponentData *mComponentData;
  bool mbStarted;
  bool mbValid;
};

///////////////////////////////////////////////////////////////////////////////

class EditorPropMapInst : public ComponentInst {
  RttiDeclareAbstract(EditorPropMapInst, ComponentInst);

public:
  EditorPropMapInst(const ComponentData *cd, Entity *pent)
      : ComponentInst(cd, pent) {}
};

// TODO: This does not belong here. Put it somewhere else.
class EditorPropMapData : public ComponentData {
  RttiDeclareConcrete(EditorPropMapData, ComponentData);

public:
  EditorPropMapData();

  void SetProperty(const ConstString &key, const ConstString &val);
  ConstString GetProperty(const ConstString &key) const;

private:
  ComponentInst *createComponent(Entity *pent) const final {
    return new EditorPropMapInst(this, pent);
  }

  orklut<ConstString, ConstString> mProperties;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ent
