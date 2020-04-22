#include <ork/lev2/aud/singularity/fmosc.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/wavetable.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////

FmOsc::FmOsc()
    : _baseFrequency(0.0f)
    , _modIndex(0)
    , _pbIndex(0)
    , _pbIndexNext(0)
    , _pbIncrBase(0)
    , _waveform(nullptr) {
  setWave(0);
}
FmOsc::~FmOsc() {
}

void FmOsc::setWave(int iwave) {
  assert(iwave >= 0 and iwave < 8);
  static const Wavetable* fmwave[8];
  static bool ginit = true;
  if (ginit) {
    ginit     = false;
    fmwave[0] = builtinWaveTable("sine");
    fmwave[1] = builtinWaveTable("tx81z.2");
    fmwave[2] = builtinWaveTable("tx81z.3");
    fmwave[3] = builtinWaveTable("tx81z.4");
    fmwave[4] = builtinWaveTable("tx81z.5");
    fmwave[5] = builtinWaveTable("tx81z.6");
    fmwave[6] = builtinWaveTable("tx81z.7");
    fmwave[7] = builtinWaveTable("tx81z.8");
  }
  _waveform = fmwave[iwave];
}

void FmOsc::keyOn(const DspKeyOnInfo& koi, const Fm4OpData& opd) {
  _pbIndex     = 0;
  _pbIndexNext = 0;
  _prevOutput  = 0.0f;
  _pbIncrBase  = 0;
  setWave(opd._waveform);
}
void FmOsc::keyOff() {
}
float FmOsc::compute(float frq, float mi) {
  const float* sblk = _waveform->_wavedata.data();
  int wtsize        = _waveform->_wavedata.size();

  float pi = float(wtsize) * frq / 48000.0f;

  _pbIncrBase   = int64_t(pi * 65536.0);
  _pbIndexNext  = _pbIndex + _pbIncrBase;
  int64_t incdp = int64_t(mi * 65536.0);

  int64_t this_phase = _pbIndex + incdp;
  float fract        = float(this_phase & 0xffff) * kinv64k;
  float invfr        = 1.0f - fract;

  int64_t iiA = (this_phase >> 16) % wtsize;
  int64_t iiB = (iiA + 1) % wtsize;

  float sampA = float(sblk[iiA]);
  float sampB = float(sblk[iiB]);
  _prevOutput = (sampB * fract + sampA * invfr);

  _pbIndex = _pbIndexNext;

  return softsat(_prevOutput, 1.1f);
}

const float _katktab[32] = {
    // delta/per sec
    0.0001, 0.0001, 0.0001, 0.0001, // 0
    0.0002, 0.0001, 0.0004, 0.0005, // 4
    0.0010, 0.0015, 0.0020, 0.0025, // 8
    0.0100, 0.0300, 0.0035, 0.0335, // 12

    0.0500, 0.0600, 0.0700, 1.5000, // 16
    0.5000, 3.9500, 3.9500, 3.9500, // 20
    3.9500, 3.9500, 3.9500, 3.9500, // 24
    3.9500, 3.9500, 3.9500, 3.9500, // 28
};

const float _kdectab[32] = {
    // delta/per sec
    0.0000, 0.00001, 0.0001, 0.0001, // 0
    0.0002, 0.0001,  0.0004, 0.0005, // 4
    0.0010, 0.0020,  0.0021, 0.0022, // 8
    0.0023, 0.024,   0.0025, 0.0026, // 12

    0.0027, 0.0028,  0.0060, 0.0320, // 16
    0.0015, 0.0055,  0.0055, 0.0055, // 20
    0.0075, 0.0075,  3.9500, 3.9500, // 24
    3.9500, 3.9500,  8.9500, 3.9500, // 28
};
const float _kd1levtab[16] = {
    0.0,
    0.1,
    0.2,
    0.3,
    0.4,
    0.5,
    0.55,
    0.6,
    0.6f,
    0.7,
    0.75,
    0.8,
    0.85,
    0.9,
    0.95,
    1.0,
};
const float _kreltab[16] = {
    0.001,
    0.0011,
    0.0013,
    0.0015,
    0.003,
    0.004,
    0.005,
    0.006,
    0.07,
    0.08,
    0.09,
    0.1,
    0.2,
    0.3,
    0.4,
    .5,
};

} // namespace ork::audio::singularity
