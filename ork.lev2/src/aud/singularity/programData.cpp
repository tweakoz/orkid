
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::ProgramData, "SynProgramData");

namespace ork::audio::singularity {

//////////////////////////////////////////////////////////////////////////////

void ProgramData::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

lyrdata_ptr_t ProgramData::newLayer() {
  auto l = std::make_shared<LayerData>(this);
  _layerdatas.push_back(l);
  return l;
}

} // namespace ork::audio::singularity
