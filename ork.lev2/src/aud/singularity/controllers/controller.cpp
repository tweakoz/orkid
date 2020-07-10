//#include <audiofile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::ControllerData, "SynControllerData");
ImplementReflectionX(ork::audio::singularity::CustomControllerData, "SynCustomControllerData");

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
void ControllerData::describeX(class_t* clazz) {
  clazz->directProperty("Name", &ControllerData::_name);
}
///////////////////////////////////////////////////////////////////////////////
scopesource_ptr_t ControllerData::createScopeSource() {
  _scopesource = std::make_shared<ScopeSource>();
  return _scopesource;
}
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
    , _curval(1.0f) {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CustomControllerData::describeX(class_t* clazz) {
}

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
