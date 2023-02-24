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
  GraphData::addModule(graphdata, "G", ptcl_globals);
  GraphData::addModule(graphdata, "P", ptcl_pool);
  GraphData::addModule(graphdata, "E", ptcl_emitter);

  ////////////////////////////////////////////////////

  ptcl_pool->_poolSize = 1024; // set num particles in pool

  ////////////////////////////////////////////////////
  // connect and init plugs
  ////////////////////////////////////////////////////

  auto P_out    = ptcl_pool->outputNamed("ParticleBuffer");
  auto E_inp    = ptcl_emitter->inputNamed("ParticleBuffer");
  auto E_rate    = ptcl_emitter->typedInputNamed<FloatXfPlugTraits>("EmissionRate");

  graphdata->safeConnect(E_inp,P_out);
  E_rate->setValue(100.0f);

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
