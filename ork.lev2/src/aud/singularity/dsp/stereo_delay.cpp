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
  _blocktype     = "StereoDelay";
  auto dtL = addParam("delaytimeL","mSec");
  auto dtR = addParam("delaytimeR","mSec");
  auto fbL = addParam("feedbackL","unit");
  auto fbR = addParam("feedbackR","unit");
  auto fbXL = addParam("feedbackXL","unit");
  auto fbXR = addParam("feedbackXR","unit");

  dtL->useDefaultEvaluator();
  dtR->useDefaultEvaluator();
  fbL->useDefaultEvaluator();
  fbR->useDefaultEvaluator();
  fbXL->useDefaultEvaluator();
  fbXR->useDefaultEvaluator();

  dtL->_coarse = 0.251;
  dtR->_coarse = 0.376;
  fbL->_coarse = 0.33;
  fbR->_coarse = 0.33;
  fbXL->_coarse = 0.33;
  fbXR->_coarse = 0.33;
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

  auto ilbuf = getInpBuf(dspbuf, 0) + ibase;
  auto irbuf = getInpBuf(dspbuf, 1) + ibase;
  auto olbuf = getOutBuf(dspbuf, 0) + ibase;
  auto orbuf = getOutBuf(dspbuf, 1) + ibase;

  float dtL   = _param[0].eval();
  float dtR   = _param[1].eval();
  float fbL   = _param[2].eval();
  float fbR   = _param[3].eval();
  float fbXL  = _param[4].eval();
  float fbXR  = _param[5].eval();

  _delayL->setNextDelayTime(dtL);
  _delayR->setNextDelayTime(dtR);

  for (int i = 0; i < inumframes; i++) {
    float fi = float(i) * invfr;
    float doutL = _delayL->out(fi);
    float doutR = _delayR->out(fi);

    float inl  = ilbuf[i];
    float inr  = irbuf[i];
    float dinL = inl+(doutL*fbL)+(doutR*fbXL);
    float dinR = inr+(doutR*fbR)+(doutL*fbXR);

    _delayL->inp(dinL);
    _delayR->inp(dinR);
    olbuf[i] = dinL;
    orbuf[i] = dinR;
  }
}

///////////////////////////////////////////////////////////

void StereoDelay::doKeyOn(const KeyOnInfo& koi) {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity {
