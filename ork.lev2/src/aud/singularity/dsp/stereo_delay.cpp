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

ImplementReflectionX(ork::audio::singularity::StereoDelayData, "DspStereoDelay");

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

void StereoDelayData::describeX(class_t* clazz) {}

///////////////////////////////////////////////////////////

StereoDelayData::StereoDelayData(std::string name)
  : DspBlockData(name) {
  _blocktype     = "Fdn4Reverb";
  auto mix_param = addParam();
  mix_param->useDefaultEvaluator();
}

dspblk_ptr_t StereoDelayData::createInstance() const { // override
  return std::make_shared<StereoDelay>(this);
}

///////////////////////////////////////////////////////////

StereoDelay::StereoDelay(const StereoDelayData* data)
  : DspBlock(data) {
    auto syni = synth::instance();
    _delayL = syni->allocDelayLine();
    _delayR = syni->allocDelayLine();
}

///////////////////////////////////////////////////////////

StereoDelay::~StereoDelay(){
  auto syni = synth::instance();
  syni->freeDelayLine(_delayL);
  syni->freeDelayLine(_delayR);
}

///////////////////////////////////////////////////////////

void StereoDelay::compute(DspBuffer& dspbuf) {
  int inumframes = _layer->_dspwritecount;
  float invfr = 1.0f / inumframes;
 int ibase      = _layer->_dspwritebase;
  float mix      = _param[0].eval();

  auto ilbuf = getInpBuf(dspbuf, 0) + ibase;
  auto irbuf = getInpBuf(dspbuf, 1) + ibase;
  auto olbuf = getOutBuf(dspbuf, 0) + ibase;
  auto orbuf = getOutBuf(dspbuf, 1) + ibase;

  for (int i = 0; i < inumframes; i++) {
    float fi = float(i) * invfr;
    float delout_L = _delayL->out(fi);
    float delout_R = _delayR->out(fi);
    float inl  = ilbuf[i];
    float inr  = irbuf[i];
    inl += delout_L*0.5;
    inr += delout_R*0.5;
    _delayL->inp(inl);
    _delayR->inp(inr);
    olbuf[i] = inl;
    orbuf[i] = inr;
  }
}

///////////////////////////////////////////////////////////

void StereoDelay::doKeyOn(const KeyOnInfo& koi) {
  _delayL->setStaticDelayTime(.125);
  _delayR->setStaticDelayTime(.25);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity {
