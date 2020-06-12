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

Fdn4ReverbData::Fdn4ReverbData() {
  _blocktype      = "Fdn4Reverb";
  auto& mix_param = addParam();
  mix_param.useDefaultEvaluator();
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t Fdn4ReverbData::createInstance() const { // override
  return std::make_shared<Fdn4Reverb>(this);
}

///////////////////////////////////////////////////////////////////////////////

DelayContext::DelayContext() {
  _buffer.resize(_maxdelay);
  _index   = 0;
  _bufdata = _buffer.channel(0);
}

///////////////////////////////////////////////////////////////////////////////

float DelayContext::out(float fi) const {
  int64_t index64       = _index << 16;
  float delaylen        = lerp(_basDelayLen, _tgtDelayLen, fi);
  int64_t outdelayindex = index64 - int64_t(delaylen * 65536.0f);

  while (outdelayindex < 0)
    outdelayindex += _maxx;
  while (outdelayindex >= _maxx)
    outdelayindex -= _maxx;

  float fract    = float(outdelayindex & 0xffff) * kinv64k;
  float invfr    = 1.0f - fract;
  int64_t iiA    = (outdelayindex >> 16) % _maxdelay;
  int64_t iiB    = (iiA + 1) % _maxdelay;
  float sampA    = _bufdata[iiA];
  float sampB    = _bufdata[iiB];
  float delayout = (sampB * fract + sampA * invfr);
  return delayout;
}

///////////////////////////////////////////////////////////////////////////////

void DelayContext::inp(float inp) {
  int64_t inpdelayindex = _index++;
  while (inpdelayindex < 0)
    inpdelayindex += _maxdelay;
  while (inpdelayindex >= _maxdelay)
    inpdelayindex -= _maxdelay;
  _bufdata[inpdelayindex] = inp;
}

///////////////////////////////////////////////////////////////////////////////

void DelayContext::setStaticDelayTime(float dt) {
  float delaylen = dt * getSampleRate();
  _basDelayLen   = delaylen;
  _tgtDelayLen   = delaylen;
}

///////////////////////////////////////////////////////////////////////////////

Fdn4Reverb::Fdn4Reverb(const Fdn4ReverbData* dbd)
    : DspBlock(dbd) {
  ///////////////////////////
  float input_g  = 0.75f;
  float output_g = 0.75f;
  float tscale   = 0.47f;
  ///////////////////////////
  matrixHadamard(0.0);
  // matrixHouseholder();
  ///////////////////////////
  _inputGainsL  = fvec4(input_g, input_g, input_g, input_g);
  _inputGainsR  = fvec4(input_g, input_g, input_g, input_g);
  _outputGainsL = fvec4(output_g, output_g, 0, 0);
  _outputGainsR = fvec4(0, 0, output_g, output_g);
  _delayA.setStaticDelayTime(tscale * 0.01f);
  _delayB.setStaticDelayTime(tscale * 0.03f);
  _delayC.setStaticDelayTime(tscale * 0.07f);
  _delayD.setStaticDelayTime(tscale * 0.11f);
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4Reverb::matrixHadamard(float fblevel) {
  float fbgain = lerp(0.40, 0.49, fblevel);
  _feedbackMatrix.SetRow(0, fvec4(+fbgain, +fbgain, +fbgain, +fbgain));
  _feedbackMatrix.SetRow(1, fvec4(+fbgain, -fbgain, +fbgain, -fbgain));
  _feedbackMatrix.SetRow(2, fvec4(+fbgain, +fbgain, -fbgain, -fbgain));
  _feedbackMatrix.SetRow(3, fvec4(+fbgain, -fbgain, -fbgain, +fbgain));
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4Reverb::matrixHouseholder() {
  float fbgain = 0.5f;
  _feedbackMatrix.SetRow(0, fvec4(+fbgain, -fbgain, -fbgain, -fbgain));
  _feedbackMatrix.SetRow(1, fvec4(-fbgain, +fbgain, -fbgain, -fbgain));
  _feedbackMatrix.SetRow(2, fvec4(-fbgain, -fbgain, +fbgain, -fbgain));
  _feedbackMatrix.SetRow(3, fvec4(-fbgain, -fbgain, -fbgain, +fbgain));
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4Reverb::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  float mix      = _param[0].eval();

  auto ilbuf = getInpBuf(dspbuf, 0) + ibase;
  auto irbuf = getInpBuf(dspbuf, 1) + ibase;
  auto olbuf = getOutBuf(dspbuf, 0) + ibase;
  auto orbuf = getOutBuf(dspbuf, 1) + ibase;

  float invfr = 1.0f / inumframes;

  fvec4 grp0 = _feedbackMatrix.GetColumn(0);
  fvec4 grp1 = _feedbackMatrix.GetColumn(1);
  fvec4 grp2 = _feedbackMatrix.GetColumn(2);
  fvec4 grp3 = _feedbackMatrix.GetColumn(3);

  for (int i = 0; i < inumframes; i++) {
    float fi = float(i) * invfr;

    /////////////////////////////////////
    // input from dsp channels
    /////////////////////////////////////

    float inl = ilbuf[i];
    float inr = irbuf[i];

    /////////////////////////////////////
    // do fdn4 operation
    /////////////////////////////////////

    float aout = _delayA.out(fi);
    float bout = _delayB.out(fi);
    float cout = _delayC.out(fi);
    float dout = _delayD.out(fi);

    auto abcd_out = fvec4(aout, bout, cout, dout);

    float ainp = inl * _inputGainsL.x + inr * _inputGainsR.x;
    float binp = inl * _inputGainsL.y + inr * _inputGainsR.y;
    float cinp = inl * _inputGainsL.z + inr * _inputGainsR.z;
    float dinp = inl * _inputGainsL.w + inr * _inputGainsR.w;

    ainp += grp0.Dot(abcd_out) + 1e-9;
    binp += grp1.Dot(abcd_out) + 1e-9;
    cinp += grp2.Dot(abcd_out) + 1e-9;
    dinp += grp3.Dot(abcd_out) + 1e-9;

    _delayA.inp(ainp);
    _delayB.inp(binp);
    _delayC.inp(cinp);
    _delayD.inp(dinp);

    /////////////////////////////////////
    // output to dsp channels
    /////////////////////////////////////

    float lout = ainp * _outputGainsL.x   //
                 + binp * _outputGainsL.y //
                 + cinp * _outputGainsL.z //
                 + dinp * _outputGainsL.w;

    float rout = ainp * _outputGainsR.x   //
                 + binp * _outputGainsR.y //
                 + cinp * _outputGainsR.z //
                 + dinp * _outputGainsR.w;

    olbuf[i] = lerp(inl, lout, mix);
    orbuf[i] = lerp(inr, rout, mix);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4Reverb::doKeyOn(const KeyOnInfo& koi) // final
{
}
} // namespace ork::audio::singularity
