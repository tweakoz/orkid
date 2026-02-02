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

controllerdata_ptr_t TX81ZEnvData::clone() const {
  auto rval = std::make_shared<TX81ZEnvData>();
  rval->_attackRate  = _attackRate;
  rval->_decay1Rate  = _decay1Rate;
  rval->_decay1Level = _decay1Level;
  rval->_decay2Rate  = _decay2Rate;
  rval->_releaseRate = _releaseRate;
  rval->_rateScale   = _rateScale;
  rval->_levScale    = _levScale;
  rval->_egshift     = _egshift;
  return rval;
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
    , _releaseRate(0)
    , _egshift(0)
    , _rateScale(0)
    , _levScale(0) {
}

///////////////////////////////////////////////////////////////////////////////

ControllerInst* TX81ZEnvData::instantiate(layer_ptr_t layer) const {
  return new TX81ZEnvInst(this, layer);
}

///////////////////////////////////////////////////////////////////////////////
// Convert effective rate (0-63) to time constant
// Higher rate = faster = shorter time
///////////////////////////////////////////////////////////////////////////////

float TX81ZEnvInst::rateToTime(int effectiveRate, bool isRelease) {
  effectiveRate = std::clamp(effectiveRate, 0, 63);

  if (effectiveRate >= 63) {
    return isRelease ? 0.025f : 0.008f;
  }
  if (effectiveRate <= 0) {
    return 12.0f;
  }

  // Each increment of 4 roughly halves the time
  float time = 12.0f * std::pow(2.0f, -effectiveRate / 4.0f);

  if (isRelease && time < 0.025f) {
    time = 0.025f;
  }

  return time;
}

///////////////////////////////////////////////////////////////////////////////

void TX81ZEnvInst::computeScaledRates(int midiKey) {
  int keycode = std::max(0, midiKey - 36);
  int ks = std::clamp(_envdata->_rateScale, 0, 3);
  int shift = 5 - ks;
  int ksr = keycode >> shift;

  int effAttack  = std::min(63, _envdata->_attackRate * 2 + ksr);
  int effDecay1  = std::min(63, _envdata->_decay1Rate * 2 + ksr);
  int effDecay2  = std::min(63, _envdata->_decay2Rate * 2 + ksr);
  int effRelease = std::min(63, _envdata->_releaseRate * 4 + 2 + ksr);

  _attackTime  = rateToTime(effAttack, false);
  _decay1Time  = rateToTime(effDecay1, false);
  _decay2Time  = rateToTime(effDecay2, false);
  _releaseTime = rateToTime(effRelease, true);
}

///////////////////////////////////////////////////////////////////////////////

TX81ZEnvInst::TX81ZEnvInst(const TX81ZEnvData* data, layer_ptr_t l)
    : ControllerInst(l) {
  _envdata = data;
  _layer   = l;

  _decay1Level = _envdata->_decay1Level / 15.0f;

  computeScaledRates(60);
}

///////////////////////////////////////////////////////////////////////////////

void TX81ZEnvInst::keyOn(const KeyOnInfo& KOI) {
  _koi = KOI;

  computeScaledRates(int(KOI._key));

  state            = 1;
  time_since_state = 0;
  _value.x         = 0;
}

///////////////////////////////////////////////////////////////////////////////

void TX81ZEnvInst::keyOff() {
  if (state != 0) {
    _releaseStartLevel = _value.x;
    state              = 4;
    time_since_state   = 0;
  }
}

///////////////////////////////////////////////////////////////////////////////

void TX81ZEnvInst::compute() {
  constexpr double THRESHOLD = 0.000016;

  time_since_state += controlPeriod();

  switch (state) {
    case 0:
      _value.x = 0;
      break;

    case 1: { // Attack
      double t = time_since_state / _attackTime;
      if (t < 5.0) {
        double factor = std::exp(-t);
        _value.x = 1.0 - factor;
      } else {
        _value.x         = 1.0;
        state            = 2;
        time_since_state = 0;
      }
      break;
    }

    case 2: { // Decay 1
      double t = time_since_state / _decay1Time;
      double factor = std::exp(-t);
      double value  = _decay1Level + (1.0 - _decay1Level) * factor;

      if (std::abs(value - _decay1Level) > THRESHOLD) {
        _value.x = value;
      } else {
        _value.x            = _decay1Level;
        _decay2StartLevel   = _decay1Level;
        state               = 3;
        time_since_state    = 0;
      }
      break;
    }

    case 3: { // Decay 2
      double t = time_since_state / _decay2Time;
      double factor = std::exp(-t);
      double value  = _decay2StartLevel * factor;

      if (value > THRESHOLD) {
        _value.x = value;
      } else {
        _value.x         = 0;
        state            = 0;
        time_since_state = 0;
      }
      break;
    }

    case 4: { // Release
      double t = time_since_state / _releaseTime;
      double factor = std::exp(-t);
      double value  = _releaseStartLevel * factor;

      if (value > THRESHOLD) {
        _value.x = value;
      } else {
        _value.x         = 0;
        state            = 0;
        time_since_state = 0;
      }
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
