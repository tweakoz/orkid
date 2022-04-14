////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/dataflow/dataflow.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/system.h>

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class CompositingSystemData : public ork::ent::SystemData {
  RttiDeclareConcrete(CompositingSystemData, ork::ent::SystemData);

public:
  lev2::CompositingData _compositingData;
  CompositingSystemData();

  void defaultSetup();

private:
  ork::ent::System* createSystem(ork::ent::Simulation* pinst) const final;
  ork::Object* _accessor() {
    return &_compositingData;
  }
};

///////////////////////////////////////////////////////////////////////////

class CompositingSystem : public ork::ent::System {
public:
  static constexpr systemkey_t SystemType = "CompositingSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  CompositingSystem(const CompositingSystemData& data, ork::ent::Simulation* pinst);
  ~CompositingSystem();

  const CompositingSystemData& compositingSystemData() const {
    return _compositingSystemData;
  }
  lev2::compositorimpl_ptr_t _impl;
  bool DoLink(Simulation* psi) final;

  bool enabled() const;

private:
  const CompositingSystemData& _compositingSystemData;
  void DoUpdate(Simulation* psi) final;
};

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
