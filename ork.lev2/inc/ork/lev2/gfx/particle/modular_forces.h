////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include "modular_particles2.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

struct GravityModuleData : public ParticleModuleData {
  DeclareConcreteX(GravityModuleData, ParticleModuleData);
public:
  GravityModuleData();
  static std::shared_ptr<GravityModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance() const final;
};

using gravitymodule_ptr_t = std::shared_ptr<GravityModuleData>;

struct TurbulenceModuleData : public ParticleModuleData {
  DeclareConcreteX(TurbulenceModuleData, ParticleModuleData);
public:
  TurbulenceModuleData();
  static std::shared_ptr<TurbulenceModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance() const final;
};

using turbulencemodule_ptr_t = std::shared_ptr<TurbulenceModuleData>;


struct VortexModuleData : public ParticleModuleData {
  DeclareConcreteX(VortexModuleData, ParticleModuleData);
public:
  VortexModuleData();
  static std::shared_ptr<VortexModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance() const final;
};

using vortexmodule_ptr_t = std::shared_ptr<VortexModuleData>;


/////////////////////////////////////////
} //namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
