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

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/util/crc.h>

ImplementReflectionX(ork::audio::singularity::TX81ZEnvData, "SynTx81ZEnv");

namespace ork::audio::singularity {

void TX81ZEnvData::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

PolynomialEval::PolynomialEval() {
}

///////////////////////////////////////////////////////////////////////////////

float PolynomialEval::evaluate(float input) const {
  float output = 0;
  int size     = _polynomial.size();
  for (int i = 0; i < size; i++) {
    output += _polynomial[i] * pow(input, i);
  }
  return output;
}

///////////////////////////////////////////////////////////////////////////////

TX81ZEnvTables::TX81ZEnvTables() {

  _attack._polynomial = {1.418, -15.35, 80.97, -264.7, 566.9, -790.2, 686.1, -335.3, 70.23};
  _decay1._polynomial = {1.418, -15.35, 80.97, -264.7, 566.9, -790.2, 686.1, -335.3, 70.23};
  _decay2._polynomial = {1.418, -15.35, 80.97, -264.7, 566.9, -790.2, 686.1, -335.3, 70.23};
  _release._polynomial = {1.418, -15.35, 80.97, -264.7, 566.9, -790.2, 686.1, -335.3, 70.23};
  _level._polynomial   = {0,1}; // linear
}

///////////////////////////////////////////////////////////////////////////////

const TX81ZEnvTables& getTX81ZEnvTables() {
  static TX81ZEnvTables tables;
  return tables;
}

///////////////////////////////////////////////////////////////////////////////

TX81ZEnvData::TX81ZEnvData()
    : _attackRate(0)
    , _decay1Rate(0)
    , _decay1Level(0)
    , _decay2Rate(0)
    , _releaseRate(0) {
}

///////////////////////////////////////////////////////////////////////////////

ControllerInst* TX81ZEnvData::instantiate(layer_ptr_t layer) const {
  return new TX81ZEnvInst(this, layer);
}

///////////////////////////////////////////////////////////////////////////////

TX81ZEnvInst::TX81ZEnvInst(const TX81ZEnvData* data, layer_ptr_t l)
    : ControllerInst(l) {
  _envdata     = data;
  _layer       = l;
  auto& tables = getTX81ZEnvTables();

  _attackRate  = tables._attack.evaluate(_envdata->_attackRate);
  _decay1Rate  = tables._decay1.evaluate(_envdata->_decay1Rate);
  _decay1Level = tables._decay1.evaluate(_envdata->_decay1Level);
  _decay2Rate  = tables._decay2.evaluate(_envdata->_decay2Rate);
  _releaseRate = tables._release.evaluate(_envdata->_releaseRate);

}

///////////////////////////////////////////////////////////////////////////////

void TX81ZEnvInst::keyOn(const KeyOnInfo& KOI) {
  _koi             = KOI;
  state            = 1;
  time_since_state = 0;
  printf( "_attackRate d<%d> i<%g>\n", _envdata->_attackRate, _attackRate );
  printf( "_decay1Rate d<%d> i<%g>\n", _envdata->_decay1Rate, _decay1Rate );
  printf( "_decay1Level d<%d> i<%g>\n", _envdata->_decay1Level, _decay1Level );
  printf( "_decay2Rate d<%d> i<%g>\n", _envdata->_decay2Rate, _decay2Rate );
  printf( "_releaseRate d<%d> i<%g>\n", _envdata->_releaseRate, _releaseRate );

}

///////////////////////////////////////////////////////////////////////////////

void TX81ZEnvInst::keyOff() {
  state            = 4;
  time_since_state = 0;
}

///////////////////////////////////////////////////////////////////////////////

void TX81ZEnvInst::compute() {
  time_since_state += controlPeriod();

  switch (state) {
    case 1: // Attack
      if (time_since_state < _attackRate) {
        double decay_factor = std::exp(-5.0 * time_since_state / _attackRate);
        _value.x = std::min(1.0 - decay_factor, 1.0);
      } else {
        state            = 2;
        time_since_state = 0;
      }
      break;
    case 2: // Decay 1
      if (_decay1Rate >= 0) {
        double decay_factor = std::exp(-time_since_state / _decay1Rate);
        double value        = decay_factor + _decay1Level * (1 - decay_factor);
        if (value != _decay1Level) {
          _value.x = value;
        } else {
          state            = 3;
          time_since_state = 0;
        }
      }
      break;
    case 3: // Decay 2
      if (_decay2Rate >= 0) {
        double decay_factor = std::exp(-time_since_state / _decay2Rate);
        double value        = _value.x * decay_factor + _decay1Level * (1 - decay_factor);
        if (value != 0) {
          _value.x = value;
        } else {
          state            = 0;
          time_since_state = 0;
        }
      }
      break;
    case 4: // Release
      double decay_factor = std::exp(-time_since_state / _releaseRate);
      double value        = _value.x * decay_factor;
      if (value != 0) {
        _value.x = value;
      } else {
        state            = 0;
        time_since_state = 0;
      }
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
