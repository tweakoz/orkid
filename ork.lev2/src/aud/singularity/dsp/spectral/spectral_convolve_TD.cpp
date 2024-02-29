////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <vector>
#include <cmath>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>
#include <ork/lev2/aud/singularity/spectral.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/fft.h>

ImplementReflectionX(ork::audio::singularity::SpectralConvolveTDData, "DspSpectralConvolveTD");

namespace ork::audio::singularity {

void SpectralConvolveTDData::describeX(class_t* clazz) {
}


///////////////////////////////////////////////////////////////////////////////

SpectralConvolveTDData::SpectralConvolveTDData(std::string name, float fb)
    : DspBlockData(name) {
  _blocktype = "SpectralConvolveTD";

  _impulse_dataset = std::make_shared<SpectralImpulseResponseDataSet>();
  for (int i = 0; i < 256; i++) {
    auto imp = std::make_shared<SpectralImpulseResponse>();
    //imp->combFilter(50 + i, 10000);
    _impulse_dataset->_impulses.push_back(imp);
  }

  auto mix_param   = addParam();
  auto gain_param   = addParam();
  mix_param->useDefaultEvaluator();
  gain_param->useDefaultEvaluator();
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t SpectralConvolveTDData::createInstance() const { // override
  return std::make_shared<SpectralConvolveTD>(this);
}

///////////////////////////////////////////////////////////////////////////////

SpectralConvolveTD::SpectralConvolveTD(const SpectralConvolveTDData* dbd)
    : DspBlock(dbd) {
  _mydata             = dbd;
  auto syni           = synth::instance();
}

///////////////////////////////////////////////////////////////////////////////

SpectralConvolveTD::~SpectralConvolveTD() {
}

///////////////////////////////////////////////////////////////////////////////

void SpectralConvolveTD::compute(DspBuffer& dspbuf) {

  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  auto ibufL = getInpBuf(dspbuf, 0) + ibase;
  auto obufL = getOutBuf(dspbuf, 0) + ibase;
  auto ibufR = getInpBuf(dspbuf, 1) + ibase;
  auto obufR = getOutBuf(dspbuf, 1) + ibase;

  float mix = _param[0].eval();
  float lingain = decibel_to_linear_amp_ratio(_param[1].eval());

  for( int i=0; i<inumframes; i++ ){
    float inL = ibufL[i];
    float inR = ibufR[i];
    float inL2 = inL+inR*0.5f;
    float inR2 = inR+inL*0.5f;
    _inpqL[i] = inL2;
    _inpqR[i] = inR2;
  }

  _convolverL.process(
    _inpqL.data(),
    _outqL.data(),
    inumframes);

  _convolverR.process(
    _inpqR.data(),
    _outqR.data(),
    inumframes);
  
  for( int i=0; i<inumframes; i++ ){
    float inL = ibufL[i];
    float inR = ibufR[i];
    float wetL = _outqL[i]*lingain;
    float wetR = _outqR[i]*lingain;
    float mixL = std::lerp(inL,wetL,mix);
    float mixR = std::lerp(inR,wetR,mix);
    obufL[i] = mixL;
    obufR[i] = mixR;
  }

}

///////////////////////////////////////////////////////////////////////////////

void SpectralConvolveTD::doKeyOn(const KeyOnInfo& koi) { // final
  auto dset = _mydata->_impulse_dataset;
  auto imp  = dset->_impulses[0];
  _inpqL.resize(frames_per_controlpass);
  _inpqR.resize(frames_per_controlpass);
  _outqL.resize(frames_per_controlpass);
  _outqR.resize(frames_per_controlpass);
  _convolverL.init(
    frames_per_controlpass, // blocksize
    imp->_impulseL.data(), // impulse
    imp->_impulseL.size()); // impulse length

  _convolverR.init(
    frames_per_controlpass, // blocksize
    imp->_impulseR.data(), // impulse
    imp->_impulseR.size()); // impulse length
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
