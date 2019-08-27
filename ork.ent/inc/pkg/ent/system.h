////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include "componentfamily.h"
#include <ork/event/EventListener.h>
#include <ork/math/cmatrix4.h>
#include <ork/object/Object.h>
#include "types.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

class SystemDataClass : public object::ObjectClass {
  RttiDeclareExplicit(SystemDataClass, object::ObjectClass, rtti::NamePolicy,
                      object::ObjectCategory);

public:
  SystemDataClass(const rtti::RTTIData &);

  PoolString GetFamily() const { return mFamily; }
  void SetFamily(PoolString family) { mFamily = family; }

private:
  PoolString mFamily;
};

///////////////////////////////////////////////////////////////////////////////

class SystemData : public Object {
  RttiDeclareExplicit(SystemData, Object, rtti::AbstractPolicy, SystemDataClass);

public:
  virtual System* createSystem(ork::ent::SceneInst* psi) const = 0;
  PoolString GetFamily() const;

protected:
  SystemData() {}
};

///////////////////////////////////////////////////////////////////////////////

class System {

public:
  void Update(SceneInst *inst);
  void Start(SceneInst *psi);
  void Link(SceneInst *psi);
  void UnLink(SceneInst *psi);
  void Stop(SceneInst *psi);
  virtual ~System() {};
  virtual systemkey_t systemTypeDynamic() = 0;

protected:
  System(const SystemData *scd, SceneInst *pinst)
      : _systemData(scd), mpSceneInst(pinst), mbStarted(false) {}

  SceneInst *mpSceneInst;

private:

  virtual void DoUpdate(SceneInst *inst) {}
  virtual void DoStart(SceneInst *psi) {}
  virtual bool DoLink(SceneInst *psi) { return true; }
  virtual void DoUnLink(SceneInst *psi) {}
  virtual void DoStop(SceneInst *psi) {}

  const SystemData *_systemData;
  bool mbStarted;
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////
