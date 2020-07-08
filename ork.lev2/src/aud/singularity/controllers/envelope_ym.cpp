////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::YmEnvData, "SynYmEnv");

namespace ork::audio::singularity {

void YmEnvData::describeX(class_t* clazz) {
  clazz->directProperty("AttackTime", &YmEnvData::_attackTime);
  clazz->directProperty("AttackShape", &YmEnvData::_attackShape);
  clazz->directProperty("Decay1Rate", &YmEnvData::_decay1Rate);
  clazz->directProperty("Decay1Level", &YmEnvData::_decay1Level);
  clazz->directProperty("Decay2Rate", &YmEnvData::_decay2Rate);
  clazz->directProperty("ReleaseRate", &YmEnvData::_releaseRate);
  clazz->directProperty("EgShift", &YmEnvData::_egshift);
  clazz->directProperty("RateScale", &YmEnvData::_rateScale);
}

///////////////////////////////////////////////////////////////////////////////

YmEnvData::YmEnvData() {

  _envadjust = [](const EnvPoint& inp, //
                  int iseg,
                  const KeyOnInfo& KOI) -> EnvPoint { //
    return inp;
  };
}

///////////////////////////////////////////////////////////////////////////////

ControllerInst* YmEnvData::instantiate(Layer* l) const // final
{
  return new YmEnvInst(this, l);
}

///////////////////////////////////////////////////////////////////////////////

YmEnvInst::YmEnvInst(const YmEnvData* data, Layer* l)
    : ControllerInst(l)
    , _data(data)
    , _curseg(-1) {
}
void YmEnvInst::compute() {
  switch (_curseg) {
    case 0: // attack
      _rawout += _atkinc;
      if (_rawout >= 1.0f) {
        _rawout = 1.0f;
        _curseg = 1;
      }
      _prcout = powf(_rawout, _data->_attackShape);
      break;
    case 1: // decay1
      _rawout *= _dec1ratefactor;
      if (_rawout <= _data->_decay1Level) {
        _rawout = _data->_decay1Level;
        _curseg = 2;
      }
      _prcout = _rawout;
      break;
    case 2: // decay2
      _rawout *= _dec2ratefactor;
      _prcout = _rawout;
      break;
    case 3: // released ?
      if (_rawout < 0.0001f) {
        _layer->release();
        _curseg = 4;
      }
    case 4: // release
      _rawout *= _relratefactor;
      _prcout = _rawout;
      break;
  }
  switch (_data->_egshift) {
    case 0:
      _curval = _prcout;
      break;
    case 1: {
      float lo = decibel_to_linear_amp_ratio(-12);
      _curval  = lerp(lo, 1.0, _prcout);
      break;
    }
    case 2: {
      float lo = decibel_to_linear_amp_ratio(-24);
      _curval  = lerp(lo, 1.0, _prcout);
      break;
    }
    case 3: {
      float lo = decibel_to_linear_amp_ratio(-48);
      _curval  = lerp(lo, 1.0, _prcout);
      break;
    }
  }
  validateSample(_curval);
}
void YmEnvInst::keyOn(const KeyOnInfo& KOI) {
  _koi    = KOI;
  _curseg = 0;
  _rawout = 0.0f;
  _layer  = KOI._layer;
  _layer->retain();
  float abas          = controlPeriod();
  _atkinc             = abas / (_data->_attackTime * 10.0f);
  int kb              = KOI._key - 24;
  float unit_keyscale = float(kb) / 67.0f;
  float pow_keyscale  = powf(unit_keyscale, 2.0);
  switch (_data->_rateScale) {
    case 0:
      _dec1ratefactor = _data->_decay1Rate;
      _dec2ratefactor = _data->_decay2Rate;
      _relratefactor  = _data->_releaseRate;
      break;
    case 1:
    case 2:
    case 3: {
      float atkscale = 1.0f;
      if (_data->_rateScale > 0) {
        atkscale += 2.0f * powf(unit_keyscale, 1.0 / float(_data->_rateScale));
      }
      _dec1ratefactor = powf(_data->_decay1Rate, pow_keyscale * (1 << (_data->_rateScale + 1)));
      _dec2ratefactor = powf(_data->_decay2Rate, pow_keyscale * (1 << (_data->_rateScale + 1)));
      _relratefactor  = powf(_data->_releaseRate, pow_keyscale * (1 << (_data->_rateScale + 1)));
      //_dec1ratefactor = _data->_decay1Rate;
      //_dec2ratefactor = _data->_decay2Rate;
      //_relratefactor  = _data->_releaseRate;
      _atkinc *= atkscale;
      if (0)
        printf(
            "kb<%d> uk<%g> rs<%d> atkscale<%g> _decratefactor<%g>\n", //
            kb,
            unit_keyscale,
            _data->_rateScale,
            atkscale,
            _dec1ratefactor);
      break;
    }
  }
  if (_atkinc > 0.3f) {
    _atkinc = 0.3f;
  }
}
void YmEnvInst::keyOff() {
  _curseg = 3;
}

} // namespace ork::audio::singularity
