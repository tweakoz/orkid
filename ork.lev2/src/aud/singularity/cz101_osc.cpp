#include <stdio.h>
#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include <ork/file/file.h>
#include <ork/kernel/string/string.h>
#include <stdint.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/cz101.h>
#include <ork/lev2/aud/singularity/fmosc.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/wavetable.h>

using namespace ork;

namespace ork::audio::singularity {

CzOsc::CzOsc() {
}
CzOsc::~CzOsc() {
}

void CzOsc::keyOn(const DspKeyOnInfo& koi, czxdata_constptr_t oscdata) {
  _data        = oscdata;
  _pbIndex     = 0;
  _pbIndexNext = 0;
  _prevOutput  = 0.0f;
  _pbIncrBase  = 0;
  setWave(oscdata->_dcoWaveA, oscdata->_dcoWaveB);
}
void CzOsc::keyOff() {
}
float CzOsc::compute(float frq, float mi) {
  if (nullptr == _waveformA)
    return 0.0f;

  static const Wavetable* sincw = builtinWaveTable("isincw8pi");
  int wtsize                    = sincw->_wavedata.size();

  float pi  = float(wtsize) * frq / 48000.0f;
  float dpi = 0.0f; // float(wtsize)*(mi)/48000.0f;

  _pbIncrBase  = int64_t(pi * 65536.0f);
  _pbIndexNext = _pbIndex + _pbIncrBase;
  float incdp  = int64_t(dpi * 65536.0f);

  int64_t this_phase = _pbIndex + incdp;
  float fract        = float(this_phase & 0xffff) * kinv64k;
  float invfr        = 1.0f - fract;

  int64_t iiA = (this_phase >> 16) % wtsize;
  int64_t iiB = (iiA + 1) % wtsize;

  float fwph = float(iiA) / float(wtsize);
  fwph += (fract) / float(wtsize);

  float base_angle     = float(iiA) / float(wtsize);
  float inv_base_angle = 1.0f - base_angle;
  float saw_window     = inv_base_angle;
  float sawsq_window   = powf(saw_window, 0.5f);

  float distort_saw = -sinf(base_angle * 0.5f);

  float distort_tri = (base_angle < 0.5) ? (0.25) - base_angle : (0.75) - base_angle;

  float distort_squ = ((base_angle < 0.5) ? -base_angle : 0.5f - base_angle);

  float distort_res = (base_angle * 16) - base_angle;

  float angle  = base_angle; //
  float window = 1.0f;

  switch (_data->_dcoWaveA) {
    case 0: // saw
      window = sincw->sampleLerp(angle);
      angle += distort_saw * mi;
      break;
    case 1: // squ
      angle += distort_squ * mi;
      break;
    case 2: // pulse
      angle += distort_tri * mi;
      break;
    case 3: // pulsine
      angle += distort_squ * mi;
      break;
    case 4: //
      angle += distort_res * mi;
      window = saw_window;
      break;
    case 5: //
      // window = fabs(sinf(base_angle*pi2));
      window = saw_window;
      // window = sincw->sampleLerp(angle);
      angle += distort_res * mi;
      break;
    case 6: //
      angle += distort_res * mi;
      window = saw_window;
      break;
    case 7: //
      angle += distort_res * mi;
      window = saw_window;
      break;
  }

  //////////

  // printf("fa<%f>\n",fa);
  _prevOutput = -sinf(angle * pi2) * window;
  _pbIndex    = _pbIndexNext;
  return _prevOutput;
}

void CzOsc::setWave(int iwA, int iwB) {
  assert(iwA >= 0 and iwA < 8);
  assert(iwB >= 0 and iwB < 8);
  static const Wavetable* czwave[8];
  static bool ginit = true;
  if (ginit) {
    ginit     = false;
    czwave[0] = builtinWaveTable("cz1.1");
    czwave[1] = builtinWaveTable("cz1.2");
    czwave[2] = builtinWaveTable("cz1.3");
    czwave[3] = builtinWaveTable("cz1.4");
    czwave[4] = builtinWaveTable("cz1.5");
    czwave[5] = builtinWaveTable("cz1.6");
    czwave[6] = builtinWaveTable("cz1.7");
    czwave[7] = builtinWaveTable("cz1.8");
  }
  _waveformA = czwave[iwA % 8];
  _waveformB = czwave[iwB % 8];
}

} // namespace ork::audio::singularity
