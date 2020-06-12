////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/fmosc.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/wavetable.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////
PmOsc::PmOsc()
    : _pbIndex(0)
    , _pbIndexNext(0)
    , _pbIncrBase(0)
    , _waveform(nullptr) {
  setWave(0);
}
///////////////////////////////////////////////////////////
PmOsc::~PmOsc() {
}
///////////////////////////////////////////////////////////
void PmOsc::setWave(int iwave) {
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
///////////////////////////////////////////////////////////
void PmOsc::keyOn(const PmOscData& opd) {
  _pbIndex     = 0;
  _pbIndexNext = 0;
  _prevOutput  = 0.0f;
  _pbIncrBase  = 0;
  setWave(opd._waveform);
  _wtsize  = _waveform->_wavedata.size();
  _wtsXisr = float(_wtsize) * getInverseSampleRate();
}
///////////////////////////////////////////////////////////
void PmOsc::keyOff() {
}
///////////////////////////////////////////////////////////
// PmOsc::compute(float frequency, float phase_offset)
//  frequency: frequency of operator in hertz
//  phase_offset: normalized phase offset
//    -1: -2 PI radians
//    +1: _2 PI radians
///////////////////////////////////////////////////////////
static constexpr float kinv64k = 1.0f / 65536.0f;
static constexpr float kinv32k = 1.0f / 32768.0f;
float PmOsc::compute(float frequency, float phase_offset) {
  const float* sblk = _waveform->_wavedata.data();
  validateSample(frequency);
  validateSample(phase_offset);

  float phaseinc = _wtsXisr * frequency;

  _pbIncrBase           = int64_t(phaseinc * 65536.0f);
  _pbIndexNext          = _pbIndex + _pbIncrBase;
  int64_t scaled_offset = _wtsize * int64_t(phase_offset * 65536.0f);
  int64_t this_phase    = _pbIndex + scaled_offset;

  /*printf(
      "_pbIndex<0x%zx> scaled_offset<0x%zx> this_phase<0x%zx>\n", //
      _pbIndex,
      scaled_offset,
      this_phase);*/

  float fract = float(this_phase & 0xffff) * kinv64k;
  float invfr = 1.0f - fract;

  int64_t iiA = (this_phase >> 16) % _wtsize;
  int64_t iiB = (iiA + 1) % _wtsize;

  while (iiA < 0) {
    iiA += _wtsize;
  }
  while (iiB < 0) {
    iiB += _wtsize;
  }
  /*printf(
      "phase_offset<%g> scaled_offset<%zd> iiA<%zd> iiB<%zd>\n", //
      phase_offset,
      scaled_offset,
      iiA,
      iiB);*/

  float sampA = float(sblk[iiA]);
  float sampB = float(sblk[iiB]);
  _prevOutput = (sampB * fract + sampA * invfr);

  _pbIndex = _pbIndexNext;

  // float output = softsat(_prevOutput, 1.1f);
  validateSample(_prevOutput);
  return _prevOutput;
}
///////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
