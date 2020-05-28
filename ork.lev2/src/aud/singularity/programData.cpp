
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/sampler.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

lyrdata_ptr_t ProgramData::newLayer() {
  auto l = std::make_shared<LayerData>();
  _layerdatas.push_back(l);
  return l;
}

} // namespace ork::audio::singularity
