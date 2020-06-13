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

Fdn4ReverbXData::Fdn4ReverbXData(float tscale)
    : _tscale(tscale) {
  _blocktype      = "Fdn4ReverbX";
  auto& mix_param = addParam();
  auto& dta_param = addParam();
  auto& dtb_param = addParam();
  auto& dtc_param = addParam();
  auto& dtd_param = addParam();

  mix_param.useDefaultEvaluator();
  dta_param.useDefaultEvaluator();
  dtb_param.useDefaultEvaluator();
  dtc_param.useDefaultEvaluator();
  dtd_param.useDefaultEvaluator();

  math::FRANDOMGEN rg(10);

  dta_param._coarse = tscale * rg.rangedf(0.01, 0.15);
  dtb_param._coarse = tscale * rg.rangedf(0.01, 0.15);
  dtc_param._coarse = tscale * rg.rangedf(0.01, 0.15);
  dtd_param._coarse = tscale * rg.rangedf(0.01, 0.15);

  _axis.x = rg.rangedf(-1, 1);
  _axis.y = rg.rangedf(-1, 1);
  _axis.z = rg.rangedf(-1, 1);
  _axis.Normalize();
  _speed         = rg.rangedf(0.00001, 0.001);
  float input_g  = 0.75f;
  float output_g = 0.75f;
  _inputGainsL   = fvec4(input_g, input_g, input_g, input_g);
  _inputGainsR   = fvec4(input_g, input_g, input_g, input_g);
  _outputGainsL  = fvec4(output_g, output_g, 0, 0);
  _outputGainsR  = fvec4(0, 0, output_g, output_g);
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t Fdn4ReverbXData::createInstance() const { // override
  return std::make_shared<Fdn4ReverbX>(this);
}

///////////////////////////////////////////////////////////////////////////////

Fdn4ReverbX::Fdn4ReverbX(const Fdn4ReverbXData* dbd)
    : DspBlock(dbd) {

  _inputGainsL  = dbd->_inputGainsL;
  _inputGainsR  = dbd->_inputGainsR;
  _outputGainsL = dbd->_outputGainsL;
  _outputGainsR = dbd->_outputGainsR;
  _axis         = dbd->_axis;
  _angle        = dbd->_angle;
  _speed        = dbd->_speed;
  ///////////////////////////
  // matrixHadamard(0.0);
  matrixHouseholder();
  ///////////////////////////
  _delayA.setStaticDelayTime(_param[1].eval());
  _delayB.setStaticDelayTime(_param[2].eval());
  _delayC.setStaticDelayTime(_param[3].eval());
  _delayD.setStaticDelayTime(_param[4].eval());
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4ReverbX::matrixHadamard(float fblevel) {
  float fbgain = lerp(0.40, 0.49, fblevel);
  _feedbackMatrix.SetRow(0, fvec4(+fbgain, +fbgain, +fbgain, +fbgain));
  _feedbackMatrix.SetRow(1, fvec4(+fbgain, -fbgain, +fbgain, -fbgain));
  _feedbackMatrix.SetRow(2, fvec4(+fbgain, +fbgain, -fbgain, -fbgain));
  _feedbackMatrix.SetRow(3, fvec4(+fbgain, -fbgain, -fbgain, +fbgain));
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4ReverbX::matrixHouseholder() {
  float fbgain = 0.5f;
  _feedbackMatrix.SetRow(0, fvec4(+fbgain, -fbgain, -fbgain, -fbgain));
  _feedbackMatrix.SetRow(1, fvec4(-fbgain, +fbgain, -fbgain, -fbgain));
  _feedbackMatrix.SetRow(2, fvec4(-fbgain, -fbgain, +fbgain, -fbgain));
  _feedbackMatrix.SetRow(3, fvec4(-fbgain, -fbgain, -fbgain, +fbgain));
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4ReverbX::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  float mix      = _param[0].eval();

  auto ilbuf = getInpBuf(dspbuf, 0) + ibase;
  auto irbuf = getInpBuf(dspbuf, 1) + ibase;
  auto olbuf = getOutBuf(dspbuf, 0) + ibase;
  auto orbuf = getOutBuf(dspbuf, 1) + ibase;

  float invfr = 1.0f / inumframes;

  _delayA.setNextDelayTime(_param[1].eval());
  _delayB.setNextDelayTime(_param[2].eval());
  _delayC.setNextDelayTime(_param[3].eval());
  _delayD.setNextDelayTime(_param[4].eval());

  for (int i = 0; i < inumframes; i++) {
    float fi   = float(i) * invfr;
    float time = float(_layer->_sampleindex + i) * getInverseSampleRate();

    fquat q;
    q.fromAxisAngle(fvec4(_axis, _speed * time));
    auto rotMatrix = q.ToMatrix();
    auto curmatrix = _feedbackMatrix * rotMatrix;

    fvec4 grp0 = curmatrix.GetColumn(0);
    fvec4 grp1 = curmatrix.GetColumn(1);
    fvec4 grp2 = curmatrix.GetColumn(2);
    fvec4 grp3 = curmatrix.GetColumn(3);

    // todo: renormalize the matrix, it eventually goes unstable...

    /////////////////////////////////////
    // input from dsp channels
    /////////////////////////////////////

    float inl = ilbuf[i];
    float inr = irbuf[i];

    float finl = _filterA.compute(inl);
    float finr = _filterB.compute(inr);
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

void Fdn4ReverbX::doKeyOn(const KeyOnInfo& koi) // final
{
  _filterA.Clear();
  _filterB.Clear();
  _filterC.Clear();
  _filterD.Clear();
  _filterA.Set(60, 8, -12.0);
  _filterB.Set(60, 8, -12.0);
  _filterC.Set(60, 8, 12.0);
  _filterD.Set(60, 8, 12.0);
}
} // namespace ork::audio::singularity
