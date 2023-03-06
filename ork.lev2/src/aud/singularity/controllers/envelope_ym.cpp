////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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

ControllerInst* YmEnvData::instantiate(layer_ptr_t l) const // final
{
  return new YmEnvInst(this, l);
}

///////////////////////////////////////////////////////////////////////////////

YmEnvInst::YmEnvInst(const YmEnvData* data, layer_ptr_t l)
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
        synth::instance()->releaseLayer(_layer);
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

/*
        constexpr float attacktimes[32] = {
          0.0, 6.74821875, 4.712260416666667, 3.3632708333333334, 2.3486770833333335, 1.6713020833333334, 1.1750104166666666, 0.8396041666666667, 0.5878541666666667, 0.41920833333333335, 0.2950833333333333, 0.21142708333333332, 0.14741666666666667, 0.10735416666666667, 0.07538541666666666, 0.054489583333333334, 0.03840625, 0.02709375, 0.018385416666666668, 0.014760416666666666, 0.010416666666666666, 0.0085625, 0.005958333333333334, 0.004583333333333333, 0.004166666666666667, 0.003958333333333334, 0.0032291666666666666, 0.002895833333333333, 0.00171875, 0.00140625, 0.0013541666666666667, 0.0013020833333333333
        };
        constexpr float decaytime[32] = {
          -0.010416666666666666, 32.46463541666667, 22.699, 16.1210625, 11.320114583333334, 8.106, 5.65215625, 4.053010416666667, 2.8355, 4.6875, 1.4370104166666666, 1.020875, 0.71875, 0.5024479166666667, 0.35665625, 0.25545833333333334, 0.375, 0.28125, 0.14436458333333332, 0.09211458333333333, 0.060145833333333336, 0.045697916666666664, 0.03389583333333333, 0.02125, 0.016385416666666666, 0.009947916666666667, 0.009635416666666667, 0.005989583333333334, 0.0049479166666666664, 0.0020833333333333333, 0.0013020833333333333, 1.0416666666666666e-05 
        };
  auto GENDECAYRATE = [](float inrate ) -> float {
    static const float minlev = decibel_to_linear_amp_ratio(-48.0);
    if(inrate<0){
      return 0.9f;
    }
    else{
      return 0.99; //pow(minlev,1.0/(controlRate()*inrate*0.0625));
    }

  };

*/
void YmEnvInst::keyOn(const KeyOnInfo& KOI) {
  _koi    = KOI;
  _curseg = 0;
  _rawout = 0.0f;
  _layer  = KOI._layer;
  _layer->retain();
  float abas          = controlPeriod();
  _atkinc             = abas / (_data->_attackTime);
  OrkAssert(abas>0.0f);
  int kb              = KOI._key - 24;
  float unit_keyscale = ork::clamp<float>(float(kb) / 67.0f,0.01,16.0);
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
      if (1)
        printf(
            "kb<%d> uk<%g> rs<%d> atkscale<%g> _decratefactor<%g>\n", //
            kb,
            unit_keyscale,
            _data->_rateScale,
            atkscale,
            _dec1ratefactor);
      _atkinc *= atkscale;
      break;
    }
  }
  if (_atkinc > 0.3f) {
    _atkinc = 0.3f;
  }
  /*validateSample(_rawout);
  validateSample(_relratefactor);
  validateSample(_dec1ratefactor);
  validateSample(_dec2ratefactor);
  validateSample(_data->_rateScale);
  validateSample(_atkinc);
  validateSample(_curval);*/
}
void YmEnvInst::keyOff() {
  _curseg = 3;
}

} // namespace ork::audio::singularity
