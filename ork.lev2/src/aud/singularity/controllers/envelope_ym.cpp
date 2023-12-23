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

ImplementReflectionX(ork::audio::singularity::YmEnvData, "SynYmEnv");

namespace ork::audio::singularity {

void YmEnvData::describeX(class_t* clazz) {
  clazz->directProperty("AttackRate", &YmEnvData::_attackRate);
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
    case "ATTACK"_crcu: // attack
      _rawout += _atkinc;
      if (_rawout >= 1.0f) {
        _rawout = 1.0f;
        _curseg = "DECAY1"_crcu;
      }
      _prcout = powf(_rawout, _data->_attackShape);
      break;
    case "DECAY1"_crcu: // decay1
    if(_disable_dec1){
      if(_disable_dec1){
        _curseg = "SUSTAIN"_crcu;
      }
      else{
        _curseg = "DECAY2"_crcu;
      }
    }
    else{
      _rawout *= _dec1ratefactor;
      if (_rawout <= _sustainLevel) {
        _rawout = _sustainLevel;
        _curseg = "SUSTAIN"_crcu;
      }
      _prcout = _rawout;
    }
      break;
    case "DECAY2"_crcu: // decay2
      if(_disable_dec2){
        _rawout = _sustainLevel;
        _curseg = "SUSTAIN"_crcu;
      }
      else{
        _rawout *= _dec2ratefactor;
        if (_rawout <= _sustainLevel) {
          _rawout = _sustainLevel;
          _curseg = "SUSTAIN"_crcu;
        }
        _prcout = _rawout;
      }
      break;
    case "SUSTAIN"_crcu: // released ?
      _rawout = _sustainLevel;
      break;
    case "???"_crcu: // released ?
    case "RELEASE"_crcu: // release
      _rawout *= _relratefactor;
      _prcout = _rawout;
      static const float kill_thresh = decibel_to_linear_amp_ratio(-84);;
      if (_rawout < kill_thresh) {
        synth::instance()->releaseLayer(_layer);
        _curseg = "RELEASE"_crcu;
      }
      break;
  }
  switch (_data->_egshift) {
    case 0:
      _value.x = _prcout;
      break;
    case 1: {
      float lo = decibel_to_linear_amp_ratio(-12);
      _value.x  = lo + _prcout*(1.0f-lo);
      break;
    }
    case 2: {
      float lo = decibel_to_linear_amp_ratio(-24);
      _value.x  = lo + _prcout*(1.0f-lo);
      break;
    }
    case 3: {
      float lo = decibel_to_linear_amp_ratio(-48);
      _value.x  = lo + _prcout*(1.0f-lo);
      break;
    }
  }
  validateSample(_value.x);
}

static constexpr float attacktimes[32] = {
  0.0, 6.74821875, 4.712260416666667, 3.3632708333333334, 2.3486770833333335, 1.6713020833333334, 1.1750104166666666, 0.8396041666666667, 0.5878541666666667, 0.41920833333333335, 0.2950833333333333, 0.21142708333333332, 0.14741666666666667, 0.10735416666666667, 0.07538541666666666, 0.054489583333333334, 0.03840625, 0.02709375, 0.018385416666666668, 0.014760416666666666, 0.010416666666666666, 0.0085625, 0.005958333333333334, 0.004583333333333333, 0.004166666666666667, 0.003958333333333334, 0.0032291666666666666, 0.002895833333333333, 0.00171875, 0.00140625, 0.0013541666666666667, 0.0013020833333333333
};
static constexpr float decay1times[32] = {
  -0.010416666666666666, 32.46463541666667, 22.699, 16.1210625, 11.320114583333334, 8.106, 5.65215625, 4.053010416666667, 2.8355, 4.6875, 1.4370104166666666, 1.020875, 0.71875, 0.5024479166666667, 0.35665625, 0.25545833333333334, 0.375, 0.28125, 0.14436458333333332, 0.09211458333333333, 0.060145833333333336, 0.045697916666666664, 0.03389583333333333, 0.02125, 0.016385416666666666, 0.009947916666666667, 0.009635416666666667, 0.005989583333333334, 0.0049479166666666664, 0.0020833333333333333, 0.0013020833333333333, 1.0416666666666666e-05 
};
static constexpr float sustainlevels[16] = {
  0, 0.007943, 0.01122, 0.015849, 0.022387,
  0.031623, 0.044668, 0.063096, 0.089125, 0.125893,
  0.177828, 0.251189, 0.358922, 0.506991, 0.716143,
  1
};
static constexpr float decay2times[32] = {
-0.010416666666666666, 32.3053125, 22.591989583333334, 16.165583333333334, 11.297354166666667, 8.036197916666667, 5.640083333333333, 4.034114583333333, 2.8130625, 2.001802083333333, 1.39609375, 1.002625, 0.70359375, 0.49407291666666664, 0.3559791666666667, 0.25478125, 0.18082291666666667, 0.12486458333333333, 0.09140625, 0.0625, 0.0448125, 0.030052083333333333, 0.02225, 0.014739583333333334, 0.010416666666666666, 0.007291666666666667, 0.007052083333333333, 0.0036979166666666666, 0.003447916666666667, 0.0026458333333333334, 1.0416666666666666e-05, 1.0416666666666666e-05
};
static constexpr float relrates[16] = {
0.0, 16.245229166666668, 8.123052083333333, 4.0218125, 2.0078020833333334, 1.0137708333333333, 0.5050104166666667, 0.25042708333333336, 0.123, 0.06289583333333333, 0.030802083333333334, 0.016354166666666666, 0.0089375, 0.0036458333333333334, 0.0012291666666666666, 1.0416666666666666e-05
};
void YmEnvInst::keyOn(const KeyOnInfo& KOI) {
  _koi    = KOI;
  _curseg = "ATTACK"_crcu;
  _rawout = 0.0f;
  _layer  = KOI._layer;
  _layer->retain();
  float abas          = controlPeriod();
  OrkAssert(abas>0.0f);
  int kb              = KOI._key - 24;
  float unit_keyscale = ork::clamp<float>(float(kb) / 67.0f,0.01,16.0);
  float pow_keyscale  = powf(unit_keyscale, 2.0);
  ////////////////////////////////////////////////
  auto GENDECAYRATE = [](float inrate ) -> float {
    static const float minlev = decibel_to_linear_amp_ratio(-48.0);
    if(inrate<0){
      return 0.5f;
    }
    else{
      return pow(minlev,1.0/(controlRate()*inrate));
    }
  };
  ////////////////////////////////////////////////
  float attack_time = attacktimes[_data->_attackRate];
  ////////////////////////////////////////////////
  float decay1_time = decay1times[_data->_decay1Rate];
  _disable_dec1 = (decay1_time<0.0f);
  float decay1_rate_base = GENDECAYRATE(decay1_time);
  ////////////////////////////////////////////////
  _sustainLevel = sustainlevels[_data->_decay1Level];
  ////////////////////////////////////////////////
  float decay2_time = decay2times[_data->_decay2Rate];
  _disable_dec2 = (decay2_time<0.0f);
  float decay2_rate_base = GENDECAYRATE(decay2_time);
  ////////////////////////////////////////////////
  float rel_time = relrates[_data->_releaseRate];
  float rel_rate_base = GENDECAYRATE(rel_time);
  ////////////////////////////////////////////////
  switch (_data->_rateScale) {
    case 0: // key invariant
      _dec1ratefactor = decay1_rate_base;
      _dec2ratefactor = decay2_rate_base;
      _relratefactor  = rel_rate_base;
      break;
    case 1:
    case 2:
    case 3: {
      float atkscale = 1.0f + 2.0f * powf(unit_keyscale, 1.0 / float(_data->_rateScale));
      attack_time *= atkscale;
      float ratescalepower = pow_keyscale * float(1 << (_data->_rateScale + 1));
      _dec1ratefactor = powf(decay1_rate_base, ratescalepower);
      _dec2ratefactor = powf(decay2_rate_base, ratescalepower);
      _relratefactor  = powf(rel_rate_base, ratescalepower);
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
      break;
    }
  }
  _atkinc           = abas / (attack_time);
  if (_atkinc > 0.3f) {
    _atkinc = 0.3f;
  }
  /*validateSample(_rawout);
  validateSample(_relratefactor);
  validateSample(_dec1ratefactor);
  validateSample(_dec2ratefactor);
  validateSample(_data->_rateScale);
  validateSample(_atkinc);
  validateSample(_value.x);*/
}
void YmEnvInst::keyOff() {
  _curseg = "RELEASE"_crcu;
}

} // namespace ork::audio::singularity
