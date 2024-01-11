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
#include <ork/lev2/aud/singularity/alg_pan.inl>
#include <iostream>
#include <string>
#include <iomanip> // For std::setw and std::setprecision

ImplementReflectionX(ork::audio::singularity::Fdn8ReverbData, "DspFxReverbFDN8");

namespace ork::audio::singularity {

void Fdn8ReverbData::describeX(class_t* clazz) {}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t Fdn8ReverbData::createInstance() const { // override
  return std::make_shared<Fdn8Reverb>(this);
}

///////////////////////////////////////////////////////////////////////////////

void Fdn8ReverbData::update() {
}

///////////////////////////////////////////////////////////////////////////////

Fdn8ReverbData::Fdn8ReverbData(std::string name)
    : DspBlockData(name) {
  _blocktype     = "Fdn8Reverb";
  addParam("inputGain","dB")->useAmplitudeEvaluator();
  addParam("ereflGain","dB")->useAmplitudeEvaluator();
  addParam("outputGain","dB")->useAmplitudeEvaluator();
  addParam("diffuserTimeModRate", "hZ")->useFrequencyEvaluator();
  addParam("diffuserTimeModAmp", "x")->useDefaultEvaluator();
  addParam("diffuserGain", "dB")->useAmplitudeEvaluator();
  addParam("fbTimeModRate", "hZ")->useFrequencyEvaluator();
  addParam("fbTimeModAmp", "x")->useDefaultEvaluator();
  addParam("fbGain", "dB")->useAmplitudeEvaluator();
  addParam("fbLpCutoff", "hZ")->useFrequencyEvaluator();
  addParam("fbHpCutoff", "hZ")->useFrequencyEvaluator();
  addParam("inputHpCutoff", "hz")->useFrequencyEvaluator();
  addParam("baseTime", "sec")->useDefaultEvaluator();
  addParam("ereflTime", "sec")->useDefaultEvaluator();

  ////////////////////////////////////
  // generate matrices
  ////////////////////////////////////
  _hadamard = mtx8f::generateHadamard();
  vec8f unit_x;
  unit_x._elements[0] = 1.0f;
  _householder        = mtx8f::householder(unit_x);
  _permute            = mtx8f::permute(0);
}

///////////////////////////////////////////////////////////////////////////////

Fdn8Reverb::Fdn8Reverb(const Fdn8ReverbData* dbd)
    : DspBlock(dbd)
    , _mydata(dbd) {
  ///////////////////////////
  float tbase    = 0.02; // dbd->_time_base;
  ///////////////////////////
  // matrixHadamard(0.0);
  ///////////////////////////
  ///////////////////////////
  ///////////////////////////
  for( int i=0; i<k_num_diffusers; i++ ){
    float this_time = tbase*powf(1.71f,float(i*0.5));
    _diffuser[i].setTime(this_time);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Fdn8Reverb::doKeyOn(const KeyOnInfo& koi) { // final

  for (int i = 0; i < k_num_diffusers; i++) {
    _diffuser[i]._hadamard    = _mydata->_hadamard;
    _diffuser[i]._permute     = _mydata->_permute;
    _diffuser[i]._nodenorm.broadcast(1e-6);
  }
  _householder = _mydata->_householder;

  _nodenorm.broadcast(1e-6);

  int seed = 0;
  std::mt19937 rng(seed); // Random number generator with seed
  std::uniform_int_distribution<int> dist(0, 0xffff);
    
  float basetime = 0.003;
  for( int j=0; j<8; j++ ){
    int i    = dist(rng) & 0x8000;
    float fi = float(i) / float(0x4000);
    fi = (fi-1.0f)*0.1;
    _fbbasetimes[j] = basetime+fi*basetime;
    _fbmodulations[j] = _fbbasetimes[j];
  }
  for( int j=0; j<8; j++ ){
    _early_refl._delay[j].setStaticDelayTime(0.01+float(j)*0.01f);
  }

  _hipassfilterL.Clear();
  _hipassfilterR.Clear();
  _hipassfilterL.SetHpf(_mydata->_hipass_cutoff);
  _hipassfilterR.SetHpf(_mydata->_hipass_cutoff);
}

///////////////////////////////////////////////////////////////////////////////

void Fdn8Reverb::compute(DspBuffer& dspbuf) // final
{
  float time = synth::instance()->_timeaccum;
  float bilfo = sinf(time);
  float unlfo = 0.5f+(bilfo*0.5);

  float inputGain  = decibel_to_linear_amp_ratio(_param[0].eval());
  float ereflGain = decibel_to_linear_amp_ratio(_param[1].eval());
  float outputGain = decibel_to_linear_amp_ratio(_param[2].eval());
  float diffuserTimeModRate = _param[3].eval();
  float diffuserTimeModAmp = _param[4].eval();
  float diffuserGain = decibel_to_linear_amp_ratio(_param[5].eval());
  float fbtModRate = _param[6].eval();
  float fbtModAmp = _param[7].eval();
  float fbGain = decibel_to_linear_amp_ratio(_param[8].eval());
  float fbLPC = _param[9].eval();
  float fbHPC = _param[10].eval();
  float inpHpCutoff = _param[11].eval();
  float baseTime = _param[12].eval();
  float ereflTime = _param[13].eval();
  //printf( "inputGain<%g> outputGain<%g>\n", inputGain, outputGain);

  _hipassfilterL.SetHpf(inpHpCutoff);
  _hipassfilterR.SetHpf(inpHpCutoff);

  //float frq = (unlfo+0.1f)*14000.0f;

  _fbLP.set(fbLPC);
  _fbHP.set(fbHPC);
  //printf( "frq<%g>\n", frq);


  for( int j=0; j<8; j++ ){

    float fi = float(j) / float(7);
    fi = (fi-1.0f)*0.1;
    _fbbasetimes[j] = baseTime+fi*baseTime;
    _fbmodulations[j] = _fbbasetimes[j];

   // _diffuser[j]._modrate = diffuserTimeModRate;
   // _diffuser[j]._modamp = diffuserTimeModAmp;
    _diffuser[j]._phase = fmod(time*diffuserTimeModRate,1)*PI2;
    _diffuser[j]._modulation = _diffuser[j]._basetimes[j]*diffuserTimeModAmp;
    _diffuser[j]._fbgain = diffuserGain;
  }
  for( int j=0; j<8; j++ ){
    _early_refl._delay[j].setNextDelayTime(ereflTime+float(j)*0.01f);
  }

  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  //float mix      = _param[0].eval();

  auto ilbuf = getInpBuf(dspbuf, 0) + ibase;
  auto irbuf = getInpBuf(dspbuf, 1) + ibase;
  auto olbuf = getOutBuf(dspbuf, 0) + ibase;
  auto orbuf = getOutBuf(dspbuf, 1) + ibase;

  float invfr = 1.0f / inumframes;

  vec8f split;

  for( int i=0; i<k_num_diffusers; i++ ){
    _diffuser[i].tick();
  }
  _fbmodphase = fmod(time*fbtModRate,1)*PI2;
  float val = sinf(_fbmodphase);
  //printf( "time<%g> _fbmodphase<%g> val<%g>\n", time, _fbmodphase, val );
  for( int i=0; i<8; i++ ){
    float fbtime = _fbbasetimes[i];
    fbtime *= (1.0f+(val*fbtModAmp));
    _fbdelay._delay[i].setNextDelayTime(fbtime);
  }

  for (int i = 0; i < inumframes; i++) {
    float fi = float(i) * invfr;

    /////////////////////////////////////
    // input/split from dsp channels
    /////////////////////////////////////

    float inl  = ilbuf[i] * inputGain;
    float inr  = irbuf[i] * inputGain;
    float finl = inl*_hipassfilterL.compute(inl);
    float finr = inr*_hipassfilterR.compute(inr);

    split.broadcast(fvec2(finl,finr));

    /////////////////////////////////////
    // diffusion step
    /////////////////////////////////////

    vec8f d0 = _diffuser[0].process(split, fi);
    vec8f d1 = _diffuser[1].process(d0, fi);
    vec8f d2 = _diffuser[2].process(d1, fi);
    vec8f d3 = _diffuser[3].process(d2, fi);
    vec8f d4 = _diffuser[4].process(d3, fi);
    vec8f d5 = _diffuser[5].process(d4, fi);
    vec8f d6 = _diffuser[6].process(d5, fi);
    vec8f d7 = _diffuser[7].process(d6, fi);

    /////////////////////////////////////
    // early reflections
    /////////////////////////////////////

    float erefl_amt = 0.125f;
    _early_refl.input( //
        (d0*0.25) + //
        (d1*0.2) + //
        (d2*0.15) + //
        (d3*0.1)
    );

    /////////////////////////////////////
    // feedback step
    /////////////////////////////////////

    vec8f fbout = _fbdelay.output(fi)*fbGain;
    //fbout = fbout*0.5f;
    vec8f fbout2 = _fbLP.compute(fbout);
    fbout2 = _fbHP.compute(fbout2);

    auto x = _householder.transform(fbout);
    auto I = (x+d7)*0.99999f;
    _fbdelay.input(I + _nodenorm);

    /////////////////////////////////////
    // mix down
    /////////////////////////////////////

    fvec2 erefl = _stereomix.mixdown(_early_refl.output(fi));

    fvec2 stereo =  _stereomix.mixdown(fbout2); //
                // + (erefl*ereflGain);

    olbuf[i] = ilbuf[i]+stereo.x*outputGain;
    orbuf[i] = irbuf[i]+stereo.y*outputGain;
  }
}

///////////////////////////////////////////////////////////////////////////////

Fdn8Reverb::DiffuserStep::DiffuserStep() {
  // create random axis (fvec3) angle (float)

  _rot.setToIdentity();
  _rotstep.newRot();
}

///////////////////////////////////////////////////////////////////////////////

void Fdn8Reverb::DiffuserStep::setTime(float time) {
  float tstep = time / float(8.0);
  // create c++20 random gen with seed
  std::mt19937 rng(_seed); // Random number generator with seed
  std::uniform_int_distribution<int> dist(0, 0xffff);


  // seed random number generator

  for (int i = 0; i < 8; i++) {
    
    int irand    = dist(rng) - 0x8000;
    float f      = float(irand) / float(0x8000);
    float tdelta = tstep * f;
    _basetimes[i] = (tstep * (float(i) + 0.5) + tdelta);
  }
  _modulation = 0.03f;
}

///////////////////////////////////////////////////////////////////////////////

void Fdn8Reverb::DiffuserStep::tick(){
  float s = sinf(_phase);
  for (int i = 0; i < 8; i++) {
    float t = _basetimes[i] * (1.0f+(s*_modulation));
    t = std::max(0.001f,t);
    //printf( "diff<%d> t<%g>\n", i, t);
    _delays._delay[i].setNextDelayTime(t);
  }
}

///////////////////////////////////////////////////////////////////////////////

vec8f Fdn8Reverb::DiffuserStep::process(const vec8f& input, float fi) {

  vec8f delay_outs = _delays.output(fi);
  vec8f I = input*0.99999f;
  _delays.input(I+delay_outs*_fbgain + _nodenorm);
  vec8f ap_out = delay_outs*(1.0f-(_fbgain*_fbgain))+(input*(_fbgain*-1.0f));
  vec8f permuted = _permute.transform(ap_out); // shuffle and invert (permute)
  vec8f h = _hadamard.transform(permuted);         // hadamard
  _rot = _rot.concat(_rotstep);
  h = _rot.transform(h); 
  return h;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
