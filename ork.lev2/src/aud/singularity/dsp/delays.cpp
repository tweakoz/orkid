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

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t StereoDynamicEchoData::createInstance() const { // override
  return std::make_shared<StereoDynamicEcho>(this);
}

///////////////////////////////////////////////////////////////////////////////

StereoDynamicEcho::StereoDynamicEcho(const StereoDynamicEchoData* dbd)
    : DspBlock(dbd) {
  _maxdelaylen = dbd->_maxdelaylen;
  _delaybuffer.resize(_maxdelaylen);
}

///////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////

void StereoDynamicEcho::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
