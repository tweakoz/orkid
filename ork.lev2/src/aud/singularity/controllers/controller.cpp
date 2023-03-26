////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

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
ImplementReflectionX(ork::audio::singularity::ConstantControllerData, "SynConstControllerData");

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

ControllerInst::ControllerInst(layer_ptr_t l)
    : _curval(1.0f)
    , _layer(l) {
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
ControllerInst* CustomControllerData::instantiate(layer_ptr_t layer) const {
  //
  return new CustomControllerInst(this, layer);
}
///////////////////////////////////////////////////////////////////////////////
CustomControllerInst::CustomControllerInst(const CustomControllerData* data, layer_ptr_t layer)
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
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void ConstantControllerData::describeX(class_t* clazz) {
  clazz->directProperty("constvalue", &ConstantControllerData::_constvalue);
}

ControllerInst* ConstantControllerData::instantiate(layer_ptr_t layer) const {
  return new ConstantInst(this, layer);
}
ConstantInst::ConstantInst(const ConstantControllerData* data, layer_ptr_t layer)
    : ControllerInst(layer) {
  _curval = data->_constvalue;
}
void ConstantInst::compute() {
  // printf("_curval<%g>\n", _curval);
}
void ConstantInst::keyOn(const KeyOnInfo& KOI) {
}
void ConstantInst::keyOff() {
}

} // namespace ork::audio::singularity
