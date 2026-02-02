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
#include <ork/lev2/aud/singularity/envelope.h>
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
// Convert effective rate (0-63) to time for 48dB of decay
// Higher rate = faster = shorter time
///////////////////////////////////////////////////////////////////////////////

float TX81ZEnvInst::rateToTime(int effectiveRate, bool isRelease) {
  effectiveRate = std::clamp(effectiveRate, 0, 63);

  // Minimum times to prevent clicks
  constexpr float MIN_TIME = 0.005f;
  constexpr float MAX_TIME = 20.0f;

  if (effectiveRate >= 63) {
    return MIN_TIME;
  }
  if (effectiveRate <= 0) {
    return MAX_TIME;
  }

  // Each increment of 4 roughly halves the time
  float time = MAX_TIME * std::pow(2.0f, -effectiveRate / 4.0f);

  return std::max(time, MIN_TIME);
}

///////////////////////////////////////////////////////////////////////////////
// Generate decay factor: multiplier per control period that gives 48dB decay
// in 'time' seconds
///////////////////////////////////////////////////////////////////////////////

static float generateDecayFactor(float time) {
  static const float minlev = decibel_to_linear_amp_ratio(-48.0f);
  if (time <= 0.0f) {
    return 0.5f;  // instant decay
  }
  // Factor that when applied (controlRate * time) times gives minlev attenuation
  return std::pow(minlev, 1.0f / (controlRate() * time));
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

  // Convert rates to times (for 48dB decay)
  float attackTime  = rateToTime(effAttack, false);
  float decay1Time  = rateToTime(effDecay1, false);
  float decay2Time  = rateToTime(effDecay2, false);
  float releaseTime = rateToTime(effRelease, true);

  // Attack: linear rise, increment per control period
  _attackInc = controlPeriod() / attackTime;
  if (_attackInc > 0.3f) _attackInc = 0.3f;  // Cap to prevent clicks

  // Decay factors: multiplicative per control period
  _decay1Factor  = generateDecayFactor(decay1Time);
  _decay2Factor  = generateDecayFactor(decay2Time);
  _releaseFactor = generateDecayFactor(releaseTime);
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

  state     = 1;
  _rawValue = 0;
  _value.x  = 0;

  // Keep layer alive while envelope is running
  _layer->retain();
}

///////////////////////////////////////////////////////////////////////////////

void TX81ZEnvInst::keyOff() {
  if (state != 0) {
    _releaseStartLevel = _rawValue;  // Capture raw value for release
    state = 4;
  }
}

///////////////////////////////////////////////////////////////////////////////

void TX81ZEnvInst::compute() {
  // Threshold for silence (~-96dB)
  constexpr float THRESHOLD = 0.000016f;

  switch (state) {
    case 0:
      _rawValue = 0;
      break;

    case 1: { // Attack - linear rise from 0 to 1.0
      _rawValue += _attackInc;
      if (_rawValue >= 1.0f) {
        _rawValue = 1.0f;
        state = 2;
      }
      break;
    }

    case 2: { // Decay 1 - decay toward sustain level
      _rawValue *= _decay1Factor;
      if (_rawValue <= _decay1Level) {
        _rawValue = _decay1Level;
        state = 3;
      }
      break;
    }

    case 3: { // Decay 2 - decay toward 0
      _rawValue *= _decay2Factor;
      if (_rawValue <= THRESHOLD) {
        _rawValue = 0;
        state = 0;
        // Release layer reference when envelope is done
        synth::instance()->releaseLayer(_layer);
      }
      break;
    }

    case 4: { // Release - decay toward 0
      _rawValue *= _releaseFactor;
      if (_rawValue <= THRESHOLD) {
        _rawValue = 0;
        state = 0;
        // Release layer reference when envelope is done
        synth::instance()->releaseLayer(_layer);
      }
      break;
    }
  }

  // Apply EG shift - remaps [0,1] to [floor,1]
  // egshift: 0=off(-96dB), 1=-48dB, 2=-24dB, 3=-12dB
  switch (_envdata->_egshift) {
    case 0:
      _value.x = _rawValue;
      break;
    case 1: { // -48dB floor
      float floor = 0.004f;
      _value.x = floor + _rawValue * (1.0f - floor);
      break;
    }
    case 2: { // -24dB floor
      float floor = 0.063f;
      _value.x = floor + _rawValue * (1.0f - floor);
      break;
    }
    case 3: { // -12dB floor
      float floor = 0.25f;
      _value.x = floor + _rawValue * (1.0f - floor);
      break;
    }
    default:
      _value.x = _rawValue;
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
