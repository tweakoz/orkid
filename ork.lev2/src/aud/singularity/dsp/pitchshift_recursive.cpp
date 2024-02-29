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
#include <dspstretch/signalsmith-stretch.h>

ImplementReflectionX(ork::audio::singularity::RecursivePitchShifterData, "DspFxPitchShifterRecursive");

namespace ork::audio::singularity {

void RecursivePitchShifterData::describeX(class_t* clazz) {}

using stretcher_t = signalsmith::stretch::SignalsmithStretch<float>;
using stretcher_ptr_t = std::shared_ptr<stretcher_t>;

struct MyPitchImpl{
  MyPitchImpl(){
    auto syni = synth::instance();
    _shifter = std::make_shared<stretcher_t>();
    _tempbufferINP.resize(16384);
    _tempbufferOINP.resize(16384);
    _tempbufferOUT.resize(16384);
    _delayOuter = syni->allocDelayLine();
    _delayOuter->setStaticDelayTime(0.25);
  }
  ~MyPitchImpl(){
    auto syni = synth::instance();
    syni->freeDelayLine(_delayOuter);
  
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
  void setLpfCutoff(float frq){

    _hipassfilter.SetHpf(60.0f);

    _lopassAfilter.SetLpf(frq);
    _lopassBfilter.SetLpf(frq);
    _lopassCfilter.SetLpf(frq);
    _lopassDfilter.SetLpf(frq);
    _lopassEfilter.SetLpf(frq);
    _lopassFfilter.SetLpf(frq);
    _lopassGfilter.SetLpf(frq);
    _lopassHfilter.SetLpf(frq);
  }
  void compute1( RecursivePitchShifter* dspblock, //
                 DspBuffer& dspbuf, //
                 int ibase, //
                 int inumframes){ //

  float mix      = dspblock->_param[0].eval();
  float shift    = dspblock->_param[1].eval(); // cents

  auto ibuf = dspblock->getInpBuf(dspbuf, 0) + ibase;
  auto obuf = dspblock->getOutBuf(dspbuf, 0) + ibase;

  float dbase  = 0.000;
  float dscale = 0.030;
  float ratio  = cents_to_linear_freq_ratio(shift);

  _shifter->setTransposeSemitones(shift/100.0f); 
  float basef        = 10000.0f;
  setLpfCutoff(basef / ratio);

    int base_index = _delayOuter->_index;
    const float *inputBuffers[1] = { _tempbufferINP.data() };
    float *outputBuffers[1] = { _tempbufferOUT.data() };
  
    float invfr = 1.0f / inumframes;

    for( int i=0; i<inumframes; i++ ){
      float fi     = float(i) * invfr;
      _delayOuter->_index = base_index + i;
      float delout = _delayOuter->out(fi);
      float oinp = _hipassfilter.compute(ibuf[i]);
      float inp = oinp + delout;
      inp        = _hipassfilter.compute(inp);
      inp        = _lopassAfilter.compute(inp);
      inp        = _lopassBfilter.compute(inp);
      inp        = _lopassCfilter.compute(inp);
      inp        = _lopassDfilter.compute(inp);
      inp        = _lopassEfilter.compute(inp);
      inp        = _lopassFfilter.compute(inp);
      inp        = _lopassGfilter.compute(inp);
      inp        = _lopassHfilter.compute(inp);
      _tempbufferINP[i] = inp;
      _tempbufferOINP[i] = oinp;
    }
    _shifter->process( inputBuffers, //
                       inumframes, //
                       outputBuffers, //
                       inumframes );

    _delayOuter->_index = base_index;
    float fblevel = dspblock->_mydata->_feedback;
    for( int i=0; i<inumframes; i++ ){
      float inp = _tempbufferINP[i];
      float oinp = _tempbufferOINP[i];
      float shifted = _tempbufferOUT[i];
      float final_out = lerp(inp, inp + shifted, mix);
      _delayOuter->inp(oinp+shifted*fblevel);
      obuf[i] = final_out;
    }
  }
  stretcher_ptr_t _shifter;
  BiQuad _hipassfilter;
  BiQuad _lopassAfilter;
  BiQuad _lopassBfilter;
  BiQuad _lopassCfilter;
  BiQuad _lopassDfilter;
  BiQuad _lopassEfilter;
  BiQuad _lopassFfilter;
  BiQuad _lopassGfilter;
  BiQuad _lopassHfilter;
  delaycontext_ptr_t _delayOuter;
  std::vector<float> _tempbufferINP;
  std::vector<float> _tempbufferOINP;
  std::vector<float> _tempbufferOUT;
};

///////////////////////////////////////////////////////////////////////////////

RecursivePitchShifterData::RecursivePitchShifterData(std::string name, float fb)
    : DspBlockData(name) {
  _feedback      = fb;
  _blocktype       = "RecursivePitchShifter";
  auto mix_param   = addParam();
  auto pitch_param = addParam();

  mix_param->useDefaultEvaluator();
  pitch_param->useDefaultEvaluator();
}
///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t RecursivePitchShifterData::createInstance() const { // override
  return std::make_shared<RecursivePitchShifter>(this);
}

///////////////////////////////////////////////////////////////////////////////

RecursivePitchShifter::RecursivePitchShifter(const RecursivePitchShifterData* dbd)
    : DspBlock(dbd) {
  _mydata = dbd;

  auto syni = synth::instance();

  auto impl = _impl[0].makeShared<MyPitchImpl>();
  impl->_shifter->presetDefault(1, syni->sampleRate());
  int block_size = impl->_shifter->blockSamples();
  int intrv_size = impl->_shifter->intervalSamples();
  printf( "block_size<%d> intrv_size<%d>\n", block_size, intrv_size );


}
RecursivePitchShifter::~RecursivePitchShifter(){

}

///////////////////////////////////////////////////////////////////////////////

void RecursivePitchShifter::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  auto impl = _impl[0].getShared<MyPitchImpl>();
  impl->compute1(this, dspbuf, ibase, inumframes);

}

///////////////////////////////////////////////////////////////////////////////

void RecursivePitchShifter::doKeyOn(const KeyOnInfo& koi) // final
{
  auto impl = _impl[0].getShared<MyPitchImpl>();
  impl->clear();
}
} // namespace ork::audio::singularity
