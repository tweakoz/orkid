//#include <audiofile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/synth.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

FPARAM::FPARAM()
    : _coarse(0.0f) {
  reset();
}

void FPARAM::reset() {
  _evaluator = [](FPARAM& cec) { return 0.0f; };
  _C1        = []() { return 0.0f; };
  _C2        = []() { return 0.0f; };
}

void FPARAM::keyOn(int ikey, int ivel) {
  _keyOff  = float(ikey - _kstartNote);
  _unitVel = float(ivel) / 127.0f;

  if (false == _kstartBipolar) {
    if (_keyOff < 0)
      _keyOff = 0;

    if (_kstartNote == 0)
      _keyOff = 0;

    // printf( "ikey<%d> ksn<%d> ko<%d>\n", ikey, _kstartNote, int(_keyOff) );
  }

  // printf( "_kstartNote<%d>\n", _kstartNote );
  // printf( "_keyOff<%f>\n", _keyOff );
  // printf( "_unitVel<%f>\n", _unitVel );
}

///////////////////////////////////////////////////////////////////////////////

float FPARAM::eval(bool dump) {
  float tot = _evaluator(*this);
  if (dump)
    printf("coarse<%g> c1<%g> c2<%g> tot<%g>\n", _coarse, _C1(), _C2(), tot);

  return tot;
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::useDefaultEvaluator() {
  _mods._evaluator = [this](FPARAM& cec) -> float {
    float kt = _keyTrack * cec._keyOff;
    float vt = -_velTrack * cec._unitVel;
    float rv = cec._coarse + cec._C1() + cec._C2() + kt + vt;
    // printf("kt<%f> vt<%f> rv<%f>\n", kt, vt, rv);
    return rv;
  };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::usePitchEvaluator() {
  _mods._evaluator = [this](FPARAM& cec) -> float {
    float kt       = _keyTrack * cec._keyOff;
    float vt       = _velTrack * cec._unitVel;
    float totcents = (cec._coarse * 100) //
                     + cec._fine         //
                     + cec._C1()         //
                     + cec._C2()         //
                     + kt                //
                     + vt;
    // float ratio = cents_to_linear_freq_ratio(totcents);
    // printf( "rat<%f>\n", ratio);
    /*
    printf( "cec._coarse<%f>\n", cec._coarse);
    printf( "cec._fine<%f>\n", cec._fine);
    printf( "c1<%f>\n", cec._C1());
    printf( "c2<%f>\n", cec._C2());
    printf( "vt<%f>\n", vt);
    printf( "totcents<%f>\n", totcents);
    */
    return totcents;
  };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::useFrequencyEvaluator() {
  _mods._evaluator = [this](FPARAM& cec) -> float {
    float ktcents  = _keyTrack * cec._keyOff;
    cec._vval      = _velTrack * cec._unitVel;
    float vtcents  = cec._vval;
    float totcents = cec._C1() + cec._C2() + ktcents + vtcents;
    float ratio    = cents_to_linear_freq_ratio(totcents);
    // printf( "vtcents<%f> ratio<%f>\n", vtcents, ratio );
    // printf( "ratio<%f>\n", ratio);
    return cec._coarse * ratio;
  };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::useKrzPosEvaluator() {
  _mods._evaluator = [this](FPARAM& cec) -> float {
    cec._kval  = _keyTrack * cec._keyOff;
    cec._vval  = _velTrack * cec._unitVel;
    cec._s1val = cec._C1();
    cec._s2val = cec._C2();
    float x    = (cec._coarse) + cec._s1val + cec._s2val + cec._kval + cec._vval;
    return clip_float(x, -100, 100);
  };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::useAmplitudeEvaluator() {
  _mods._evaluator = [this](FPARAM& cec) -> float {
    cec._kval  = _keyTrack * cec._keyOff;
    cec._vval  = lerp(-_velTrack, 0.0f, cec._unitVel);
    cec._s1val = cec._C1();
    cec._s2val = cec._C2();
    float x    = (cec._coarse) + cec._s1val + cec._s2val + cec._kval + cec._vval;
    // printf( "vt<%f> kt<%f> x<%f>\n", vt, kt, x );
    return x;
  };
}

///////////////////////////////////////////////////////////////////////////////

void DspParamData::useKrzEvnOddEvaluator() {
  _mods._evaluator = [this](FPARAM& cec) -> float {
    float kt = _keyTrack * cec._keyOff;
    float vt = lerp(-_velTrack, 0.0f, cec._unitVel);
    float x  = (cec._coarse) + cec._C1() + cec._C2() + kt + vt;
    // printf( "vt<%f> kt<%f> x<%f>\n", vt, kt, x );
    return clip_float(x, -10, 10);
  };
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
