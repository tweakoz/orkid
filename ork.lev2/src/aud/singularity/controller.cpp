//#include <audiofile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/synth.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void ControlBlockInst::keyOn(const KeyOnInfo& KOI, const ControlBlockData* CBD) {
  assert(CBD);
  auto l = KOI._layer;

  for (int i = 0; i < kmaxctrlperblock; i++) {
    auto data = CBD->_cdata[i];
    if (data) {
      _cinst[i]                   = data->instantiate();
      l->_controlMap[data->_name] = _cinst[i];
      _cinst[i]->keyOn(KOI);
    }
  }
}
void ControlBlockInst::keyOff() {
  for (int i = 0; i < kmaxctrlperblock; i++) {
    if (_cinst[i])
      _cinst[i]->keyOff();
  }
}
void ControlBlockInst::compute(int inumfr) {
  for (int i = 0; i < kmaxctrlperblock; i++) {
    if (_cinst[i])
      _cinst[i]->compute(inumfr);
  }
}

///////////////////////////////////////////////////////////////////////////////

ControllerInst::ControllerInst()
    : _curval(0.0f) {
}

} // namespace ork::audio::singularity
