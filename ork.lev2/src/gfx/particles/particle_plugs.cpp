////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>

///////////////////////////////////////////////////////////////////////////////

namespace dflow = ::ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<ParticleBufferInst> ParticleBufferPlugTraits::data_to_inst(std::shared_ptr<ParticleBufferData> inp){
  return std::make_shared<ParticleBufferInst>(inp);
}




///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

///////////////////////////////////////////////////////////////////////////////
// plug template instantiations
///////////////////////////////////////////////////////////////////////////////

template <> //
void ptcl::particlebuf_outplugdata_t::describeX(class_t* clazz) {
}
template <> //
void ptcl::particlebuf_inplugdata_t::describeX(class_t* clazz) {
}

template <> //
dflow::inpluginst_ptr_t ptcl::particlebuf_inplugdata_t::createInstance() const {
  return std::make_shared<ptcl::particlebuf_inpluginst_t>(this);
}
template <> //
dflow::outpluginst_ptr_t ptcl::particlebuf_outplugdata_t::createInstance() const {
  return std::make_shared<ptcl::particlebuf_outpluginst_t>(this);
}

ImplementTemplateReflectionX(ptcl::particlebuf_outplugdata_t, "psys::ptclbufoutplug");
ImplementTemplateReflectionX(ptcl::particlebuf_inplugdata_t, "psys::ptclbufinpplug");
