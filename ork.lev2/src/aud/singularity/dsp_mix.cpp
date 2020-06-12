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
#include <ork/lev2/aud/singularity/alg_pan.inl>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
Sum2Data::Sum2Data() {
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

MonoInStereoOutData::MonoInStereoOutData() {
  _blocktype      = "MonoInStereoOut";
  auto& amp_param = addParam();
  amp_param.useAmplitudeEvaluator();
  auto& pan_param = addParam();
  pan_param.useDefaultEvaluator();
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

StereoEnhancerData::StereoEnhancerData() {
  _blocktype        = "StereoEnhancer";
  auto& width_param = addParam();
  width_param.useDefaultEvaluator();
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

StereoDynamicEchoData::StereoDynamicEchoData() {
  _blocktype             = "StereoDynamicEcho";
  auto& delaytime_paramL = addParam();
  auto& delaytime_paramR = addParam();
  auto& feedback_param   = addParam();
  auto& mix_param        = addParam();
  delaytime_paramL.useDefaultEvaluator();
  delaytime_paramR.useDefaultEvaluator();
  feedback_param.useDefaultEvaluator();
  mix_param.useDefaultEvaluator();
}
dspblk_ptr_t StereoDynamicEchoData::createInstance() const { // override
  return std::make_shared<StereoDynamicEcho>(this);
}

StereoDynamicEcho::StereoDynamicEcho(const DspBlockData* dbd)
    : DspBlock(dbd) {
  auto ssedata = (const StereoDynamicEchoData*)dbd;
  _maxdelaylen = ssedata->_maxdelaylen;
  _delaybuffer.resize(_maxdelaylen);
}
static constexpr float kinv64k = 1.0f / 65536.0f;

void StereoDynamicEcho::compute(DspBuffer& dspbuf) // final
{
  float delaytimeL = _param[0].eval();
  float delaytimeR = _param[1].eval();
  float feedback   = _param[2].eval();
  float mix        = _param[3].eval();

  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;

  auto ilbuf = getInpBuf(dspbuf, 0) + ibase;
  auto irbuf = getInpBuf(dspbuf, 1) + ibase;
  auto olbuf = getOutBuf(dspbuf, 0) + ibase;
  auto orbuf = getOutBuf(dspbuf, 1) + ibase;
  auto dlbuf = _delaybuffer.channel(0);
  auto drbuf = _delaybuffer.channel(1);

  float final_delaylenL = delaytimeL * getSampleRate();
  OrkAssert(int(final_delaylenL) < _maxdelaylen);
  float final_delaylenR = delaytimeR * getSampleRate();
  OrkAssert(int(final_delaylenR) < _maxdelaylen);

  float invfr = 1.0f / inumframes;

  int64_t maxx = (_maxdelaylen << 16);

  for (int i = 0; i < inumframes; i++) {
    float fi = float(i) * invfr;

    float inl = ilbuf[i];
    float inr = irbuf[i];

    int64_t index64 = _index << 16;

    /////////////////////////////////////
    // read delayed signal L
    /////////////////////////////////////

    float inner_delaylenL  = lerp(_delaylenL, final_delaylenL, fi);
    int64_t outdelayindexL = index64 - int64_t(inner_delaylenL * 65536.0f);

    while (outdelayindexL < 0)
      outdelayindexL += maxx;
    while (outdelayindexL >= maxx)
      outdelayindexL -= maxx;

    float fract     = float(outdelayindexL & 0xffff) * kinv64k;
    float invfr     = 1.0f - fract;
    int64_t iiA     = (outdelayindexL >> 16) % _maxdelaylen;
    int64_t iiB     = (iiA + 1) % _maxdelaylen;
    float sampA     = dlbuf[iiA];
    float sampB     = dlbuf[iiB];
    float delayoutL = (sampB * fract + sampA * invfr);

    /////////////////////////////////////
    // read delayed signal R
    /////////////////////////////////////

    float inner_delaylenR  = lerp(_delaylenR, final_delaylenR, fi);
    int64_t outdelayindexR = index64 - int64_t(inner_delaylenR * 65536.0f);

    while (outdelayindexR < 0)
      outdelayindexR += maxx;
    while (outdelayindexR >= maxx)
      outdelayindexR -= maxx;

    fract = float(outdelayindexR & 0xffff) * kinv64k;
    invfr = 1.0f - fract;
    iiA   = (outdelayindexR >> 16) % _maxdelaylen;
    iiB   = (iiA + 1) % _maxdelaylen;

    sampA           = drbuf[iiA];
    sampB           = drbuf[iiB];
    float delayoutR = (sampB * fract + sampA * invfr);

    /////////////////////////////////////
    // input to delayline
    /////////////////////////////////////

    int64_t inpdelayindex = _index;
    while (inpdelayindex < 0)
      inpdelayindex += _maxdelaylen;
    while (inpdelayindex >= _maxdelaylen)
      inpdelayindex -= _maxdelaylen;

    dlbuf[inpdelayindex] = inl + delayoutL * feedback;
    drbuf[inpdelayindex] = inr + delayoutR * feedback;

    /////////////////////////////////////
    // output to dsp channels
    /////////////////////////////////////

    olbuf[i] = lerp(inl, delayoutL, mix);
    orbuf[i] = lerp(inr, delayoutR, mix);

    _index++;
  }

  _delaylenL = final_delaylenL;
  _delaylenR = final_delaylenR;
}

void StereoDynamicEcho::doKeyOn(const KeyOnInfo& koi) // final
{
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
