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

///////////////////////////////////////////////////////////////////////////////

CzOsc::~CzOsc() {
}

///////////////////////////////////////////////////////////////////////////////

void CzOsc::keyOn(const DspKeyOnInfo& koi, czxdata_constptr_t oscdata) {
  _data  = oscdata;
  _phase = 0;
  setWave(oscdata->_dcoWaveA, oscdata->_dcoWaveB);
}

///////////////////////////////////////////////////////////////////////////////

void CzOsc::keyOff() {
}

///////////////////////////////////////////////////////////////////////////////

float CzOsc::compute(float frq, float mi) {
  constexpr double kscale    = double(1 << 24);
  constexpr double kinvscale = 1.0 / kscale;
  double phaseinc            = kscale * frq / 48000.0f;
  double angle               = double(_phase & 0xffffff) * kinvscale; //
  float output               = -sinf(angle * PI2);
  _phase += int64_t(phaseinc);
  return output;
}

///////////////////////////////////////////////////////////////////////////////

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
