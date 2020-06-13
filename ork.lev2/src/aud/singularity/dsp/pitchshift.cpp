////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

PitchShifterData::PitchShifterData() {
  _blocktype        = "PitchShifter";
  auto& mix_param   = addParam();
  auto& pitch_param = addParam();

  mix_param.useDefaultEvaluator();
  pitch_param.useDefaultEvaluator();
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t PitchShifterData::createInstance() const { // override
  return std::make_shared<PitchShifter>(this);
}

///////////////////////////////////////////////////////////////////////////////

PitchShifter::PitchShifter(const PitchShifterData* dbd)
    : DspBlock(dbd) {
  _delayA.setStaticDelayTime(0.5);
  _delayB.setStaticDelayTime(0.5);
}

///////////////////////////////////////////////////////////////////////////////

inline float trienv(int64_t inphase) {
  double ramp = kinv24m * inphase;
  ramp        = (ramp <= 0.5) //
             ? (ramp * 2.0f)
             : 1.0f - ((ramp - 0.5) * 2.0f);
  return ramp;
}

void PitchShifter::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  float mix      = _param[0].eval();
  float shift    = _param[1].eval(); // cents

  auto ibuf  = getInpBuf(dspbuf, 0) + ibase;
  auto obufL = getOutBuf(dspbuf, 0) + ibase;
  auto obufR = getOutBuf(dspbuf, 1) + ibase;

  float invfr = 1.0f / inumframes;

  float dbase      = 0.000;
  float dscale     = 0.030;
  float ratio      = cents_to_linear_freq_ratio(shift);
  float s          = (dbase + dscale) * getSampleRate();
  double frq       = 1.0f * (ratio - 1.0f) * getSampleRate() / s;
  int64_t phaseinc = frq * -double(1L << 48) * getInverseSampleRate(); // * (getSampleRate() * kinv24m);
  // printf("ratio<%g> frq<%g> pi<%lld>\n", ratio, frq, phaseinc);
  for (int i = 0; i < inumframes; i++) {
    float fi     = float(i) * invfr;
    int64_t pha  = (_phaseA >> 24) & 0xffffff;
    int64_t phb  = (_phaseB >> 24) & 0xffffff;
    int64_t phc  = (_phaseC >> 24) & 0xffffff;
    int64_t phd  = (_phaseD >> 24) & 0xffffff;
    float maska  = trienv(pha) * .5f;
    float maskb  = trienv(phb) * .5f;
    float maskc  = trienv(phc) * .5f;
    float maskd  = trienv(phd) * .5f;
    double rampa = kinv24m * double(pha);
    double rampb = kinv24m * double(phb);
    double rampc = kinv24m * double(phc);
    double rampd = kinv24m * double(phd);
    float atime  = dbase + dscale * rampa;
    float btime  = dbase + dscale * rampb;
    float ctime  = dbase + dscale * rampc;
    float dtime  = dbase + dscale * rampd;
    _delayA.setStaticDelayTime(atime);
    _delayB.setStaticDelayTime(btime);
    _delayC.setStaticDelayTime(ctime);
    _delayD.setStaticDelayTime(dtime);
    _phaseA += phaseinc;
    _phaseB += phaseinc;
    _phaseC += phaseinc;
    _phaseD += phaseinc;
    /////////////////////////////////////
    // input from dsp channels
    /////////////////////////////////////

    float inp = ibuf[i];

    /////////////////////////////////////
    // do fdn4 operation
    /////////////////////////////////////

    float aout = _delayA.out(fi);
    float bout = _delayB.out(fi);
    float cout = _delayC.out(fi);
    float dout = _delayD.out(fi);

    _delayA.inp(inp);
    _delayB.inp(inp);
    _delayC.inp(inp);
    _delayD.inp(inp);

    /////////////////////////////////////
    // output to dsp channels
    /////////////////////////////////////

    float out = aout * maska + //
                bout * maskb + //
                cout * maskc + //
                dout * maskd;

    obufL[i] = lerp(inp, out, mix);
    obufR[i] = lerp(inp, out, mix);
  }
}

///////////////////////////////////////////////////////////////////////////////

void PitchShifter::doKeyOn(const KeyOnInfo& koi) // final
{
  _phaseA = 0;
  _phaseB = (1L << 47);
  _phaseC = (2L << 47);
  _phaseD = (3L << 47);
}
} // namespace ork::audio::singularity
