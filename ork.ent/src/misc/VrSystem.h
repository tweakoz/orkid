////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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

class VrSystemData : public ork::ent::SystemData {
  DeclareConcreteX(VrSystemData, ork::ent::SystemData);

public:
  VrSystemData();

  void defaultSetup();

  const PoolString& vrTrackedObject() const {
    return _vrTrackedObject;
  }
  const PoolString& vrCamera() const {
    return _vrCamera;
  }

private:
  ork::ent::System* createSystem(ork::ent::Simulation* pinst) const final;
  PoolString _vrTrackedObject;
  PoolString _vrCamera;
};

///////////////////////////////////////////////////////////////////////////

class VrSystem : public ork::ent::System {
public:
  static constexpr systemkey_t SystemType = "VrSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  VrSystem(const VrSystemData& data, ork::ent::Simulation* pinst);
  ~VrSystem();

  const VrSystemData& vrSystemData() const {
    return _vrSystemData;
  }
  bool DoLink(Simulation* psi) final;

  bool enabled() const;
  void enqueueDrawables(lev2::DrawableBuffer& buffer) final;

private:
  const VrSystemData& _vrSystemData;
  Entity* _trackedObject            = nullptr;
  const lev2::CameraData* _vrCamDat = nullptr; // todo clean this up..
  int _vrstate                      = 0;
  int _prv_vrstate                  = 0;

  void DoUpdate(Simulation* psi) final;
};

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
