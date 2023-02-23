#include <ork/lev2/gfx/particle/modular_particles2.h>

////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <utpp/UnitTest++.h>

namespace ork::lev2::particle {

TEST(particles_a) {

  using namespace ork::dataflow;

  auto graphdata = std::make_shared<GraphData>();

  auto ptcl_pool = ParticlePoolData::createShared();
  auto ptcl_globals = GlobalModuleData::createShared();

  GraphData::addModule(graphdata, "G", ptcl_globals);
  GraphData::addModule(graphdata, "P", ptcl_pool);

  auto dg_context = std::make_shared<dgcontext>();
  dg_context->createRegisters<float>("ptc_float", 16);  
  dg_context->createRegisters<fvec3>("ptc_vec3f", 16);  
  dg_context->createRegisters<ParticleBufferData>("ptc_buffer", 4); 

  auto dg_sorter                            = std::make_shared<DgSorter>(graphdata.get(), dg_context);
  dg_sorter->_logchannel->_enabled     = true;
  dg_sorter->_logchannel_reg->_enabled = true;

  auto topo = dg_sorter->generateTopology();
  auto graphinst = std::make_shared<GraphInst>(graphdata);
  graphinst->updateTopology(topo);

  for( int i=0; i<100; i++ ){
    graphinst->compute();
  }

}

} //namespace ork::lev2::particle {
