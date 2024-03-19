////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include "modular_particles2.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

struct SphAttractorModuleData : public ParticleModuleData {
  DeclareConcreteX(SphAttractorModuleData, ParticleModuleData);
public:
  SphAttractorModuleData();
  static std::shared_ptr<SphAttractorModuleData> createShared();
  static rtti::castable_ptr_t sharedFactory();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using sphattractormodule_ptr_t = std::shared_ptr<SphAttractorModuleData>;

struct GravityModuleData : public ParticleModuleData {
  DeclareConcreteX(GravityModuleData, ParticleModuleData);
public:
  GravityModuleData();
  static std::shared_ptr<GravityModuleData> createShared();
  static rtti::castable_ptr_t sharedFactory();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using gravitymodule_ptr_t = std::shared_ptr<GravityModuleData>;

struct TurbulenceModuleData : public ParticleModuleData {
  DeclareConcreteX(TurbulenceModuleData, ParticleModuleData);
public:
  TurbulenceModuleData();
  static std::shared_ptr<TurbulenceModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using turbulencemodule_ptr_t = std::shared_ptr<TurbulenceModuleData>;


struct VortexModuleData : public ParticleModuleData {
  DeclareConcreteX(VortexModuleData, ParticleModuleData);
public:
  VortexModuleData();
  static std::shared_ptr<VortexModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using vortexmodule_ptr_t = std::shared_ptr<VortexModuleData>;


/////////////////////////////////////////
} //namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
