////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/reflect/enum_serializer.inl>

#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/kernel/orklut.hpp>

#include <ork/lev2/gfx/particle/modular_particles2.h>
//#include <ork/kernel/fixedlut.hpp>

///////////////////////////////////////////////////////////////////////////////

#include <ork/stream/FileInputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/lev2/lev2_asset.h>

///////////////////////////////////////////////////////////////////////////////

namespace dflow = ::ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////




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

template <> 
std::shared_ptr<ptcl::ParticleBufferData> dflow::inpluginst<ptcl::ParticleBufferData>::value() const{
  return _default;
}

namespace ork::dataflow {
template <> int MaxFanout<ptcl::ParticleBufferData>() {
  return 1;
}
} // namespace dflow

ImplementTemplateReflectionX(ptcl::particlebuf_outplugdata_t, "psys::ptclbufoutplug");
ImplementTemplateReflectionX(ptcl::particlebuf_inplugdata_t, "psys::ptclbufinpplug");
