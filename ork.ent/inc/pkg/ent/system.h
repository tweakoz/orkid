////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include "componentfamily.h"
#include "types.h"
#include <ork/event/EventListener.h>
#include <ork/math/cmatrix4.h>
#include <ork/object/Object.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

class SystemDataClass : public object::ObjectClass {
  RttiDeclareExplicit(SystemDataClass, object::ObjectClass, rtti::NamePolicy, object::ObjectCategory);

public:
  SystemDataClass(const rtti::RTTIData&);

  PoolString GetFamily() const { return mFamily; }
  void SetFamily(PoolString family) { mFamily = family; }

private:
  PoolString mFamily;
};

///////////////////////////////////////////////////////////////////////////////

class SystemData : public Object {
  RttiDeclareExplicit(SystemData, Object, rtti::AbstractPolicy, SystemDataClass);

public:
  virtual System* createSystem(ork::ent::Simulation* psi) const = 0;
  PoolString GetFamily() const;

protected:
  SystemData() {}
};

///////////////////////////////////////////////////////////////////////////////

class System {

public:
  virtual systemkey_t systemTypeDynamic() = 0;

  Simulation* simulation() { return mpSimulation; }

protected:
  System(const SystemData* scd, Simulation* pinst) : _systemData(scd), mpSimulation(pinst), mbStarted(false) {}
  Simulation* mpSimulation;
  virtual ~System(){};

private:
  friend class Simulation;

  void Update(Simulation* inst);
  void Start(Simulation* psi);
  void Link(Simulation* psi);
  void UnLink(Simulation* psi);
  void Stop(Simulation* psi);

  virtual void DoUpdate(Simulation* inst) {}
  virtual void DoStart(Simulation* psi) {}
  virtual bool DoLink(Simulation* psi) { return true; }
  virtual void DoUnLink(Simulation* psi) {}
  virtual void DoStop(Simulation* psi) {}
  virtual void enqueueDrawables(DrawableBuffer& buffer) {}

  const SystemData* _systemData;
  bool mbStarted;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
