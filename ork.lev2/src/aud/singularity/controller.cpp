//#include <audiofile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void ControlBlockInst::keyOn(const KeyOnInfo& KOI, controlblockdata_constptr_t CBD) {
  assert(CBD);
  auto l = KOI._layer;

  for (int i = 0; i < kmaxctrlperblock; i++) {
    auto data = CBD->_cdata[i];
    if (data) {
      _cinst[i]                   = data->instantiate(l);
      l->_controld2iMap[data]     = _cinst[i];
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
void ControlBlockInst::compute() {
  for (int i = 0; i < kmaxctrlperblock; i++) {
    if (_cinst[i])
      _cinst[i]->compute();
  }
}

///////////////////////////////////////////////////////////////////////////////

ControllerInst::ControllerInst(Layer* l)
    : _layer(l)
    , _curval(0.0f) {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CustomControllerData::CustomControllerData() {
  _oncompute = [](CustomControllerInst* cci) {};
  _onkeyon   = [](CustomControllerInst* cci, const KeyOnInfo& KOI) { cci->_curval = 0.0f; };
  _onkeyoff  = [](CustomControllerInst* cci) {};
}
///////////////////////////////////////////////////////////////////////////////
ControllerInst* CustomControllerData::instantiate(Layer* layer) const {
  //
  return new CustomControllerInst(this, layer);
}
///////////////////////////////////////////////////////////////////////////////
CustomControllerInst::CustomControllerInst(const CustomControllerData* data, Layer* layer)
    : ControllerInst(layer)
    , _data(data) {
}
///////////////////////////////////////////////////////////////////////////////
void CustomControllerInst::compute() {
  _data->_oncompute(this);
}
///////////////////////////////////////////////////////////////////////////////
void CustomControllerInst::keyOn(const KeyOnInfo& KOI) {
  _data->_onkeyon(this, KOI);
}
///////////////////////////////////////////////////////////////////////////////
void CustomControllerInst::keyOff() {
  _data->_onkeyoff(this);
}
} // namespace ork::audio::singularity
