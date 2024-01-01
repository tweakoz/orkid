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

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

Fdn4ReverbData::Fdn4ReverbData(std::string name)
    : DspBlockData(name) {
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
    : DspBlock(dbd)
    , _mydata(dbd) {
  ///////////////////////////
  float input_g  = dbd->_input_gain;
  float output_g = dbd->_output_gain;
  float tbase   = dbd->_time_base;
  float tscale = dbd->_time_scale;
  ///////////////////////////
  // matrixHadamard(0.0);
  matrixHouseholder(dbd->_matrix_gain);
  ///////////////////////////
  _inputGainsL  = fvec4(input_g, input_g, input_g, input_g);
  _inputGainsR  = fvec4(input_g, input_g, input_g, input_g);
  _outputGainsL = fvec4(output_g, output_g, 0, 0);
  _outputGainsR = fvec4(0, 0, output_g, output_g);
  _delayA.setStaticDelayTime(tbase);
  _delayB.setStaticDelayTime(tbase * tscale);
  _delayC.setStaticDelayTime(tbase * tscale*tscale);
  _delayD.setStaticDelayTime(tbase * tscale*tscale*tscale);
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

void Fdn4Reverb::matrixHouseholder(float fbgain) {
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

    float ainpA = ainp + grp0.dotWith(abcd_out) + 1e-9;
    float binpA = binp + grp1.dotWith(abcd_out) + 1e-9;
    float cinpA = cinp + grp2.dotWith(abcd_out) + 1e-9;
    float dinpA = dinp + grp3.dotWith(abcd_out) + 1e-9;

    float ainpB = _allpassA.Tick(ainpA);
    float binpB = _allpassB.Tick(binpA);
    float cinpB = _allpassC.Tick(cinpA);
    float dinpB = _allpassD.Tick(dinpA);

    _delayA.inp(ainpB);
    _delayB.inp(binpB);
    _delayC.inp(cinpB);
    _delayD.inp(dinpB);

    /////////////////////////////////////
    // output to dsp channels
    /////////////////////////////////////

    float lout = ainpA* _outputGainsL.x   //
               + binpA* _outputGainsL.y //
               + cinpA* _outputGainsL.z //
               + dinpA* _outputGainsL.w;

    float rout = ainpA* _outputGainsR.x   //
                 + binpA* _outputGainsR.y //
                 + cinpA* _outputGainsR.z //
                 + dinpA* _outputGainsR.w;

    olbuf[i] = std::lerp( inl,lout, mix);
    orbuf[i] = std::lerp( inr,rout, mix);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4Reverb::doKeyOn(const KeyOnInfo& koi) { // final
  _hipassfilterL.Clear();
  _hipassfilterR.Clear();
  _hipassfilterL.SetHpf(_mydata->_hipass_cutoff);
  _hipassfilterR.SetHpf(_mydata->_hipass_cutoff);
  _allpassA.Clear();
  _allpassB.Clear();
  _allpassC.Clear();
  _allpassD.Clear();
  _allpassA.set(_mydata->_allpass_cutoff);
  _allpassB.set(_mydata->_allpass_cutoff);
  _allpassC.set(_mydata->_allpass_cutoff);
  _allpassD.set(_mydata->_allpass_cutoff);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
