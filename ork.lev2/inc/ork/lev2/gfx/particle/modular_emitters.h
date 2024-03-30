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

struct RingEmitterData : public ParticleModuleData {
  DeclareConcreteX(RingEmitterData, ParticleModuleData);
public:
  RingEmitterData();
  static std::shared_ptr<RingEmitterData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using ringemittermodule_ptr_t = std::shared_ptr<RingEmitterData>;

///////////////////////////////////////////////////////////////////////////////

struct NozzleEmitterData : public ParticleModuleData {
  DeclareConcreteX(NozzleEmitterData, ParticleModuleData);

public:

  NozzleEmitterData();
  static std::shared_ptr<NozzleEmitterData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using nozzleemittermodule_ptr_t = std::shared_ptr<NozzleEmitterData>;

///////////////////////////////////////////////////////////////////////////////

struct LineEmitterData : public ParticleModuleData {
  DeclareConcreteX(LineEmitterData, ParticleModuleData);

public:

  LineEmitterData();
  static std::shared_ptr<LineEmitterData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using lineemittermodule_ptr_t = std::shared_ptr<LineEmitterData>;

///////////////////////////////////////////////////////////////////////////////

struct EllipticalEmitterData : public ParticleModuleData {
  DeclareConcreteX(EllipticalEmitterData, ParticleModuleData);

public:

  EllipticalEmitterData();
  static std::shared_ptr<EllipticalEmitterData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dataflow::GraphInst* ginst) const final;
};

using ellipticalemittermodule_ptr_t = std::shared_ptr<EllipticalEmitterData>;

/////////////////////////////////////////
} // namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
