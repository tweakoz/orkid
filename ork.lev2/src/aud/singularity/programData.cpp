////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::ProgramData, "SynProgramData");

namespace ork::audio::singularity {

//////////////////////////////////////////////////////////////////////////////

void ProgramData::describeX(class_t* clazz) {
  clazz->directProperty("Name", &ProgramData::_name);
  clazz->directProperty("Tags", &ProgramData::_tags);
  clazz->directVectorProperty("Layers", &ProgramData::_layerdatas);
}

///////////////////////////////////////////////////////////////////////////////

lyrdata_ptr_t ProgramData::newLayer() {
  auto l = std::make_shared<LayerData>(this);
  _layerdatas.push_back(l);
  return l;
}

} // namespace ork::audio::singularity
