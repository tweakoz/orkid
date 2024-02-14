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
#include <dspstretch/signalsmith-stretch.h>

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

using stretcher_t = signalsmith::stretch::SignalsmithStretch<float>;
using stretcher_ptr_t = std::shared_ptr<stretcher_t>;

struct PitchImpl{
  PitchImpl(){
    _shifter = std::make_shared<stretcher_t>();
    _tempbufferINP.resize(16384);
    _tempbufferOUT.resize(16384);
  }
  void clear(){
    _shifter->reset();
    _hipassfilter.Clear();
    _lopassAfilter.Clear();
    _lopassBfilter.Clear();
    _lopassCfilter.Clear();
    _lopassDfilter.Clear();
    _lopassEfilter.Clear();
    _lopassFfilter.Clear();
    _lopassGfilter.Clear();
    _lopassHfilter.Clear();    
  }
  stretcher_ptr_t _shifter;
  std::vector<float> _tempbufferINP;
  std::vector<float> _tempbufferOUT;
  BiQuad _hipassfilter;
  BiQuad _lopassAfilter;
  BiQuad _lopassBfilter;
  BiQuad _lopassCfilter;
  BiQuad _lopassDfilter;
  BiQuad _lopassEfilter;
  BiQuad _lopassFfilter;
  BiQuad _lopassGfilter;
  BiQuad _lopassHfilter;
};

///////////////////////////////////////////////////////////////////////////////

constexpr size_t knumstages = 4;
PitchShifter::PitchShifter(const PitchShifterData* dbd)
    : DspBlock(dbd) {
  auto syn = synth::instance();

  auto impl = _impl[0].makeShared<PitchImpl>();
  impl->_shifter->presetDefault(1, syn->sampleRate());
  int block_size = impl->_shifter->blockSamples();
  int intrv_size = impl->_shifter->intervalSamples();
  printf( "block_size<%d> intrv_size<%d>\n", block_size, intrv_size );
  // samples: 5760, 1440
  // mSec:    120,  30
  //impl->_shifter->configure(1, 4096, 64);

}

PitchShifter::~PitchShifter(){
  auto syn = synth::instance();
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

  auto impl = _impl[0].getShared<PitchImpl>();

  impl->_hipassfilter.SetHpf(60.0f);
  float basef        = 10000.0f;
  impl->_lopassAfilter.SetLpf(basef / ratio);
  impl->_lopassBfilter.SetLpf(basef / ratio);
  impl->_lopassCfilter.SetLpf(basef / ratio);
  impl->_lopassDfilter.SetLpf(basef / ratio);
  impl->_lopassEfilter.SetLpf(basef / ratio);
  impl->_lopassFfilter.SetLpf(basef / ratio);
  impl->_lopassGfilter.SetLpf(basef / ratio);
  impl->_lopassHfilter.SetLpf(basef / ratio);

  impl->_shifter->setTransposeSemitones(shift/100.0f); 

  for( int i=0; i<inumframes; i++ ){
    float oinpL = ibufL[i];
    float oinpR = ibufR[i];
    float inp  = oinpL+oinpR;
    inp        = impl->_hipassfilter.compute(inp);
    inp        = impl->_lopassAfilter.compute(inp);
    inp        = impl->_lopassBfilter.compute(inp);
    inp        = impl->_lopassCfilter.compute(inp);
    inp        = impl->_lopassDfilter.compute(inp);
    inp        = impl->_lopassEfilter.compute(inp);
    inp        = impl->_lopassFfilter.compute(inp);
    inp        = impl->_lopassGfilter.compute(inp);
    inp        = impl->_lopassHfilter.compute(inp);
    impl->_tempbufferINP[i] = inp;
  }

  const float *inputBuffers[1] = { impl->_tempbufferINP.data() };
  float *outputBuffers[1] = { impl->_tempbufferOUT.data() };

  impl->_shifter->process( inputBuffers, //
                           inumframes, //
                           outputBuffers, //
                           inumframes );

  for( int i=0; i<inumframes; i++ ){
    float oinpL = ibufL[i];
    float oinpR = ibufR[i];
    float shifted  = impl->_tempbufferOUT[i];
    obufL[i] = lerp(oinpL,oinpL + shifted, mix);
    obufR[i] = lerp(oinpR,oinpR + shifted, mix);
  }

}

///////////////////////////////////////////////////////////////////////////////

void PitchShifter::doKeyOn(const KeyOnInfo& koi) // final
{
  auto impl = _impl[0].getShared<PitchImpl>();
  impl->clear();
}
} // namespace ork::audio::singularity
