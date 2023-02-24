////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/particle/particle.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/dataflow/dataflow.h>
#include <ork/math/gradient.h>
#include <ork/kernel/any.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

struct RingEmitterData : public ParticleModuleData {
  DeclareConcreteX(RingEmitterData, ParticleModuleData);
public:
  RingEmitterData();
  static std::shared_ptr<RingEmitterData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance() const final;
};

///////////////////////////////////////////////////////////////////////////////

struct NozzleEmitterData : public ParticleModuleData {
  DeclareConcreteX(NozzleEmitterData, ParticleModuleData);

public:

  NozzleEmitterData();
  static std::shared_ptr<NozzleEmitterData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance() const final;
};

/////////////////////////////////////////
} // namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
