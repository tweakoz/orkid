////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synth.h>
#include <ork/reflect/properties/registerX.inl>
ImplementReflectionX(ork::audio::singularity::GradientData, "SynGradient");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
void GradientData::describeX(class_t* clazz) {
  clazz->directProperty("initial", &GradientData::_initial);
  clazz->directProperty("slope", &GradientData::_slope);
}

///////////////////////////////////////////////////////////////////////////////

GradientData::GradientData()
    : _initial(0.0f)
    , _slope(1.0f){
}
GradientData::~GradientData(){
}

ControllerInst* GradientData::instantiate(layer_ptr_t l) const {
  return new GradientInst(this, l);
}

controllerdata_ptr_t GradientData::clone() const {
  auto rval = std::make_shared<GradientData>();
  rval->_initial = _initial;
  rval->_slope      = _slope;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

GradientInst::GradientInst(const GradientData* data, layer_ptr_t l)
    : ControllerInst(l)
    , _data(data){

}

GradientInst::~GradientInst(){
  printf("XX\n");
}
///////////////////////////////////////////////////////////////////////////////

void GradientInst::reset() {
  _value.x = _data->_initial;
}

///////////////////////////////////////////////////////////////////////////////

void GradientInst::keyOn(const KeyOnInfo& KOI) // final
{
  if (nullptr == _data)
    return;
  reset();
}

///////////////////////////////////////////////////////////////////////////////

void GradientInst::keyOff() // final
{
}

///////////////////////////////////////////////////////////////////////////////

void GradientInst::compute() // final
{
  if (nullptr == _data) {
    _value.x = 0.0f;
  } else {
    float SR = getSampleRate();
    float dt = float(_layer->_dspwritecount) / SR;
    _value.x += _data->_slope*dt;
    // printf( "dt<%g> lforate<%f> PI<%g> _phase<%g> out<%g>\n", dt, _currate, _phaseInc, _phase, _value.x );
  }
}

} // namespace ork::audio::singularity
