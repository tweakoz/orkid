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
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::MonoInStereoOutData, "SynMonoInStereoOut");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
Sum2Data::Sum2Data(std::string name)
    : DspBlockData(name) {
  _blocktype = "SUM2";
}
dspblk_ptr_t Sum2Data::createInstance() const { // override
  return std::make_shared<SUM2>(this);
}
///////////////////////////////////////////////////////////////////////////////
SUM2::SUM2(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void SUM2::compute(DspBuffer& dspbuf) { // final
  int inumframes       = _layer->_dspwritecount;
  int ibase            = _layer->_dspwritebase;
  const float* inpbufa = getInpBuf(dspbuf, 0) + ibase;
  const float* inpbufb = getInpBuf(dspbuf, 1) + ibase;
  float* outbufa       = getOutBuf(dspbuf, 0) + ibase;
  // float* outbufb       = getOutBuf(dspbuf, 1) + ibase;
  for (int i = 0; i < inumframes; i++) {
    float inA  = inpbufa[i] * _dbd->_inputPad;
    float inB  = inpbufb[i] * _dbd->_inputPad;
    float res  = (inA + inB);
    res        = clip_float(res, -2, 2);
    outbufa[i] = res;
    // outbufb[i] = res;
  }
}
///////////////////////////////////////////////////////////////////////////////

void MonoInStereoOutData::describeX(class_t* clazz) {
}

MonoInStereoOutData::MonoInStereoOutData(std::string name)
    : DspBlockData(name) {
  _blocktype     = "MonoInStereoOut";
  auto amp_param = addParam();
  amp_param->useAmplitudeEvaluator();
  auto pan_param = addParam();
  pan_param->useDefaultEvaluator();
}
dspblk_ptr_t MonoInStereoOutData::createInstance() const { // override
  return std::make_shared<MonoInStereoOut>(this);
}

MonoInStereoOut::MonoInStereoOut(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void MonoInStereoOut::compute(DspBuffer& dspbuf) // final
{
  float dynamicgain = _param[0].eval() * _dbd->_inputPad;
  float dynamicpan  = _panbase + _param[1].eval();
  int inumframes    = _layer->_dspwritecount;
  int ibase         = _layer->_dspwritebase;
  const auto& LD    = _layer->_layerdata;
  auto l_lrmix      = panBlend(dynamicpan);
  auto ibuf         = getInpBuf(dspbuf, 0) + ibase;
  auto lbuf         = getOutBuf(dspbuf, 1) + ibase;
  auto ubuf         = getOutBuf(dspbuf, 0) + ibase;
  float SingleLinG  = decibel_to_linear_amp_ratio(LD->_channelGains[0]);

  for (int i = 0; i < inumframes; i++) {
    // float linG = decibel_to_linear_amp_ratio(dynamicgain);
    // linG *= SingleLinG;
    float inp  = ibuf[i];
    float mono = clip_float(
        inp * dynamicgain, //
        kminclip,
        kmaxclip);
    ubuf[i] = mono * l_lrmix.lmix;
    lbuf[i] = mono * l_lrmix.rmix;
  }
  _fval[0] = _filt;
}

void MonoInStereoOut::doKeyOn(const KeyOnInfo& koi) // final
{
  _filt    = 0.0f;
  auto LD  = koi._layer->_layerdata;
  int chan = _dspchannel[0];
  _panbase = LD->_channelPans[chan];
}
///////////////////////////////////////////////////////////////////////////////

StereoEnhancerData::StereoEnhancerData(std::string name)
    : DspBlockData(name) {
  _blocktype       = "StereoEnhancer";
  auto width_param = addParam();
  width_param->useDefaultEvaluator();
}
dspblk_ptr_t StereoEnhancerData::createInstance() const { // override
  return std::make_shared<StereoEnhancer>(this);
}

StereoEnhancer::StereoEnhancer(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void StereoEnhancer::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  auto ilbuf     = getInpBuf(dspbuf, 0) + ibase;
  auto irbuf     = getInpBuf(dspbuf, 1) + ibase;
  auto olbuf     = getOutBuf(dspbuf, 0) + ibase;
  auto orbuf     = getOutBuf(dspbuf, 1) + ibase;
  float width    = _param[0].eval();

  for (int i = 0; i < inumframes; i++) {
    float inl    = ilbuf[i];
    float inr    = irbuf[i];
    float mono   = (inl + inr) * 0.5f;
    float stereo = (inl - inr) * width;
    olbuf[i]     = mono + stereo;
    orbuf[i]     = mono - stereo;
  }
}

void StereoEnhancer::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
