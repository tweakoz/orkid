////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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

Fdn4ReverbData::Fdn4ReverbData(std::string name, float tscale)
    : DspBlockData(name)
    , _tscale(tscale) {
  _blocktype     = "Fdn4Reverb";
  auto mix_param = addParam();
  mix_param->useDefaultEvaluator();
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t Fdn4ReverbData::createInstance() const { // override
  return std::make_shared<Fdn4Reverb>(this);
}

///////////////////////////////////////////////////////////////////////////////

Fdn4Reverb::Fdn4Reverb(const Fdn4ReverbData* dbd)
    : DspBlock(dbd) {
  ///////////////////////////
  float input_g  = 0.75f;
  float output_g = 0.75f;
  float tscale   = dbd->_tscale;
  ///////////////////////////
  // matrixHadamard(0.0);
  matrixHouseholder();
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
  _feedbackMatrix.setRow(0, fvec4(+fbgain, +fbgain, +fbgain, +fbgain));
  _feedbackMatrix.setRow(1, fvec4(+fbgain, -fbgain, +fbgain, -fbgain));
  _feedbackMatrix.setRow(2, fvec4(+fbgain, +fbgain, -fbgain, -fbgain));
  _feedbackMatrix.setRow(3, fvec4(+fbgain, -fbgain, -fbgain, +fbgain));
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4Reverb::matrixHouseholder() {
  float fbgain = 0.5f;
  _feedbackMatrix.setRow(0, fvec4(+fbgain, -fbgain, -fbgain, -fbgain));
  _feedbackMatrix.setRow(1, fvec4(-fbgain, +fbgain, -fbgain, -fbgain));
  _feedbackMatrix.setRow(2, fvec4(-fbgain, -fbgain, +fbgain, -fbgain));
  _feedbackMatrix.setRow(3, fvec4(-fbgain, -fbgain, -fbgain, +fbgain));
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

  fvec4 grp0 = _feedbackMatrix.column(0);
  fvec4 grp1 = _feedbackMatrix.column(1);
  fvec4 grp2 = _feedbackMatrix.column(2);
  fvec4 grp3 = _feedbackMatrix.column(3);

  for (int i = 0; i < inumframes; i++) {
    float fi = float(i) * invfr;

    /////////////////////////////////////
    // input from dsp channels
    /////////////////////////////////////

    float inl  = ilbuf[i];
    float inr  = irbuf[i];
    float finl = _hipassfilterL.compute(inl);
    float finr = _hipassfilterR.compute(inr);

    /////////////////////////////////////
    // do fdn4 operation
    /////////////////////////////////////

    float aout = _delayA.out(fi);
    float bout = _delayB.out(fi);
    float cout = _delayC.out(fi);
    float dout = _delayD.out(fi);

    auto abcd_out = fvec4(aout, bout, cout, dout);

    float ainp = finl * _inputGainsL.x + finr * _inputGainsR.x;
    float binp = finl * _inputGainsL.y + finr * _inputGainsR.y;
    float cinp = finl * _inputGainsL.z + finr * _inputGainsR.z;
    float dinp = finl * _inputGainsL.w + finr * _inputGainsR.w;

    ainp += grp0.dotWith(abcd_out) + 1e-9;
    binp += grp1.dotWith(abcd_out) + 1e-9;
    cinp += grp2.dotWith(abcd_out) + 1e-9;
    dinp += grp3.dotWith(abcd_out) + 1e-9;

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
  _hipassfilterL.Clear();
  _hipassfilterR.Clear();
  _hipassfilterL.SetHpf(200.0f);
  _hipassfilterR.SetHpf(200.0f);
}
} // namespace ork::audio::singularity
