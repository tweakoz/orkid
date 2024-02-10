////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>
#include <ork/lev2/aud/singularity/delays.h>

ImplementReflectionX(ork::audio::singularity::PitchShifterData, "DspFxPitchShifter");

namespace ork::audio::singularity {

void PitchShifterData::describeX(class_t* clazz) {}
///////////////////////////////////////////////////////////////////////////////

PitchShifterData::PitchShifterData(std::string name)
    : DspBlockData(name) {
  _blocktype       = "PitchShifter";
  auto mix_param   = addParam();
  auto pitch_param = addParam();

  mix_param->useDefaultEvaluator();
  pitch_param->useDefaultEvaluator();
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t PitchShifterData::createInstance() const { // override
  return std::make_shared<PitchShifter>(this);
}

///////////////////////////////////////////////////////////////////////////////

constexpr size_t knumstages = 4;
PitchShifter::PitchShifter(const PitchShifterData* dbd)
    : DspBlock(dbd) {
  auto syn = synth::instance();
  _delays.resize(knumstages);
  _phases.resize(knumstages);
  for( size_t i=0; i<knumstages; i++ ){
    _delays[i] = syn->allocDelayLine();
    _delays[i]->setStaticDelayTime(0.5);
  }
}

PitchShifter::~PitchShifter(){
  auto syn = synth::instance();
  for( auto delay : _delays ){
    syn->freeDelayLine(delay);
  }
  _delays.clear();  
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
  float ratio  = cents_to_linear_freq_ratio(shift);

  auto ibufL = getInpBuf(dspbuf, 0) + ibase;
  auto ibufR = getInpBuf(dspbuf, 1) + ibase;
  auto obufL = getOutBuf(dspbuf, 0) + ibase;
  auto obufR = getOutBuf(dspbuf, 1) + ibase;

  float invfr = 1.0f / inumframes;

  float dbase  = 0.000;
  float dscale = 0.060;

  float s          = (dbase + dscale) * getSampleRate();
  double frq       = 1.0f * (ratio - 1.0f) * getSampleRate() / s;
  int64_t phaseinc = frq * -double(1L << 48) * getInverseSampleRate();
  //int64_t phaseinc = frq * -double(1L << 47) * getInverseSampleRate();
  // printf("ratio<%g> frq<%g> pi<%lld>\n", ratio, frq, phaseinc);
  //T1 = T0/R; 

  int64_t phas[knumstages];
  float masks[knumstages];
  double ramps[knumstages];
  float times[knumstages];

  for (int i = 0; i < inumframes; i++) {
    float fi     = float(i) * invfr;
    for( size_t j = 0; j < knumstages; j++) {
      phas[j] = (_phases[j] >> 24) & 0xffffff;
      masks[j] = trienv(phas[j]);
      ramps[j] = kinv24m * double(phas[j]);
      times[j] = dbase + dscale * ramps[j];
      _delays[j]->setStaticDelayTime(times[j]);
      _phases[j] += phaseinc;
    }

    /////////////////////////////////////
    // input from dsp channels
    /////////////////////////////////////

    float oinpL = ibufL[i];
    float oinpR = ibufR[i];
    float inp  = oinpL+oinpR;
    
    /////////////////////////////////////
    // do delay mix operation
    /////////////////////////////////////

    float out = 0.0f;
    for( size_t j = 0; j < knumstages; j++) {
      float this_out = _delays[j]->out(fi) * masks[j];
      out += this_out;
      _delays[j]->inp(inp);
    }

    float outgain = 4.0f/(knumstages);
    obufL[i] = lerp(oinpL,oinpL+ out * outgain, mix);
    obufR[i] = lerp(oinpR,oinpR+ out * outgain, mix);
  }
}

///////////////////////////////////////////////////////////////////////////////

void PitchShifter::doKeyOn(const KeyOnInfo& koi) // final
{
  for( size_t i=0; i<knumstages; i++ ){
    //_phases[i] = i<<47;
    _phases[i] = i<<45;
  }
}
} // namespace ork::audio::singularity
