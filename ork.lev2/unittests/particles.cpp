////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <utpp/UnitTest++.h>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_emitters.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>

////////////////////////////////////////////////////////////////

namespace ork::lev2::particle {

TEST(particles_a) {

  using namespace ork::dataflow;

  ////////////////////////////////////////////////////
  // create particlesystem dataflow graph
  ////////////////////////////////////////////////////

  auto graphdata = std::make_shared<GraphData>();

  auto ptcl_pool    = ParticlePoolData::createShared();
  auto ptcl_globals = GlobalModuleData::createShared();
  auto ptcl_emitter = NozzleEmitterData::createShared();
  auto ptcl_gravity = GravityModuleData::createShared();
  GraphData::addModule(graphdata, "G", ptcl_globals);
  GraphData::addModule(graphdata, "P", ptcl_pool);
  GraphData::addModule(graphdata, "E", ptcl_emitter);
  GraphData::addModule(graphdata, "GRAV", ptcl_gravity);

  ////////////////////////////////////////////////////

  ptcl_pool->_poolSize = 1024; // set num particles in pool

  ////////////////////////////////////////////////////
  // connect and init plugs
  ////////////////////////////////////////////////////

  auto E_rate     = ptcl_emitter->typedInputNamed<FloatXfPlugTraits>("EmissionRate");
  auto GR_G       = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("G");
  auto GR_Mass    = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("Mass");
  auto GR_OthMass = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("OthMass");
  auto GR_MinDist = ptcl_gravity->typedInputNamed<FloatXfPlugTraits>("MinDistance");
  auto GR_Center  = ptcl_gravity->typedInputNamed<Vec3XfPlugTraits>("Center");

  auto P_out  = ptcl_pool->outputNamed("ParticleBuffer");
  auto E_inp  = ptcl_emitter->inputNamed("ParticleBuffer");
  auto E_out  = ptcl_emitter->outputNamed("ParticleBuffer");
  auto GR_inp = ptcl_gravity->inputNamed("ParticleBuffer");

  graphdata->safeConnect(E_inp, P_out);
  graphdata->safeConnect(GR_inp, E_out);

  E_rate->setValue(100.0f);
  GR_G->setValue(1);
  GR_Mass->setValue(1.0f);
  GR_OthMass->setValue(1.0f);
  GR_MinDist->setValue(1);
  GR_Center->setValue(fvec3(1, 2, 3));

  ////////////////////////////////////////////////////

  auto dg_context = std::make_shared<dgcontext>();
  dg_context->createRegisters<float>("ptc_float", 16);
  dg_context->createRegisters<fvec3>("ptc_vec3f", 16);
  dg_context->createRegisters<ParticleBufferData>("ptc_buffer", 4);

  ////////////////////////////////////////////////////
  // create topology
  ////////////////////////////////////////////////////

  auto dg_sorter                       = std::make_shared<DgSorter>(graphdata.get(), dg_context);
  dg_sorter->_logchannel->_enabled     = true;
  dg_sorter->_logchannel_reg->_enabled = true;

  auto topo         = dg_sorter->generateTopology();
  auto graphinst    = std::make_shared<GraphInst>(graphdata);
  auto ptcl_context = graphinst->_impl.makeShared<Context>();
  graphinst->updateTopology(topo);

  ////////////////////////////////////////////////////
  // execute...
  ////////////////////////////////////////////////////

  auto updata      = std::make_shared<ui::UpdateData>();
  updata->_dt      = 0.1f;
  updata->_abstime = 0.0f;

  for (int i = 0; i < 100; i++) {
    updata->_abstime += updata->_dt;
    graphinst->compute(updata);
  }
}

} // namespace ork::lev2::particle
