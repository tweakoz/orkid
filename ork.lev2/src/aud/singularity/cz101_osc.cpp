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

float CzOsc::compute(float frq, float modindex) {

  constexpr double kscale    = double(1 << 24);
  constexpr double kinvscale = 1.0 / kscale;

  int64_t pos = _phase & 0xffffff;
  int64_t x1  = int64_t(modindex * 0xffffff);

  float m1 = .5 / modindex;
  float m2 = .5 / (1.0 - modindex);
  float b2 = 1.0 - m2;

  double dpos = double(pos) * kinvscale;

  double warped = (pos < x1) //
                      ? (m1 * dpos)
                      : (m2 * dpos + b2);

  double phaseinc = kscale * frq / 48000.0f;
  _phase += int64_t(phaseinc);
  return cosf(warped * PI2);
} // namespace ork::audio::singularity

///////////////////////////////////////////////////////////////////////////////

void CzOsc::setWave(int iwA, int iwB) {
  assert(iwA >= 0 and iwA < 8);
  assert(iwB >= 0 and iwB < 8);
}

} // namespace ork::audio::singularity
