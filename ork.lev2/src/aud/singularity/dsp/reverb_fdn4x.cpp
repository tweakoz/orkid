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

Fdn4ReverbXData::Fdn4ReverbXData(std::string name)
    : DspBlockData(name){
  _blocktype     = "Fdn4ReverbX";
  auto mix_param = addParam();
  auto dta_param = addParam();
  auto dtb_param = addParam();
  auto dtc_param = addParam();
  auto dtd_param = addParam();

  math::FRANDOMGEN rg(10);

  _axis.x = rg.rangedf(-1, 1);
  _axis.y = rg.rangedf(-1, 1);
  _axis.z = rg.rangedf(-1, 1);
  _axis.normalizeInPlace();
  _speed = rg.rangedf(0.00001, 0.001);

  dta_param->_coarse = _time_scale * rg.rangedf(0.01, 0.15);
  dtb_param->_coarse = _time_scale * rg.rangedf(0.01, 0.15);
  dtc_param->_coarse = _time_scale * rg.rangedf(0.01, 0.15);
  dtd_param->_coarse = _time_scale * rg.rangedf(0.01, 0.15);

  update();
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4ReverbXData::update(){

  _inputGainsL   = fvec4(_input_gain, _input_gain, _input_gain, _input_gain);
  _inputGainsR   = fvec4(_input_gain, _input_gain, _input_gain, _input_gain);
  _outputGainsL  = fvec4(_output_gain, _output_gain, 0, 0);
  _outputGainsR  = fvec4(0, 0, _output_gain, _output_gain);}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t Fdn4ReverbXData::createInstance() const { // override
  return std::make_shared<Fdn4ReverbX>(this);
}

///////////////////////////////////////////////////////////////////////////////

Fdn4ReverbX::Fdn4ReverbX(const Fdn4ReverbXData* dbd)
    : DspBlock(dbd)
    , _mydata(dbd) {

  _inputGainsL  = dbd->_inputGainsL;
  _inputGainsR  = dbd->_inputGainsR;
  _outputGainsL = dbd->_outputGainsL;
  _outputGainsR = dbd->_outputGainsR;
  _axis         = dbd->_axis;
  _angle        = dbd->_angle;
  _speed        = dbd->_speed;
  ///////////////////////////
  // matrixHadamard(0.0);
  matrixHouseholder(dbd->_matrix_gain);

  ///////////////////////////
  _delayA.setStaticDelayTime(_param[1].eval());
  _delayB.setStaticDelayTime(_param[2].eval());
  _delayC.setStaticDelayTime(_param[3].eval());
  _delayD.setStaticDelayTime(_param[4].eval());
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4ReverbX::matrixHadamard(float fblevel) {
  float fbgain = lerp(0.40, 0.49, fblevel);
  _feedbackMatrix.setRow(0, fvec4(+fbgain, +fbgain, +fbgain, +fbgain));
  _feedbackMatrix.setRow(1, fvec4(+fbgain, -fbgain, +fbgain, -fbgain));
  _feedbackMatrix.setRow(2, fvec4(+fbgain, +fbgain, -fbgain, -fbgain));
  _feedbackMatrix.setRow(3, fvec4(+fbgain, -fbgain, -fbgain, +fbgain));
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4ReverbX::matrixHouseholder(float fbgain) {
  _feedbackMatrix.setRow(0, fvec4(+fbgain, -fbgain, -fbgain, -fbgain));
  _feedbackMatrix.setRow(1, fvec4(-fbgain, +fbgain, -fbgain, -fbgain));
  _feedbackMatrix.setRow(2, fvec4(-fbgain, -fbgain, +fbgain, -fbgain));
  _feedbackMatrix.setRow(3, fvec4(-fbgain, -fbgain, -fbgain, +fbgain));
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
    auto rotMatrix = q.toMatrix();
    auto curmatrix = fmtx4::multiply_ltor(_feedbackMatrix,rotMatrix);

    fvec4 grp0 = curmatrix.column(0);
    fvec4 grp1 = curmatrix.column(1);
    fvec4 grp2 = curmatrix.column(2);
    fvec4 grp3 = curmatrix.column(3);

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

    float lout = ainpA * _outputGainsL.x   //
                 + binpA * _outputGainsL.y //
                 + cinpA * _outputGainsL.z //
                 + dinpA * _outputGainsL.w;

    float rout = ainpA * _outputGainsR.x   //
                 + binpA * _outputGainsR.y //
                 + cinpA * _outputGainsR.z //
                 + dinpA * _outputGainsR.w;

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
  _filterA.SetHpf(_mydata->_hipass_cutoff);
  _filterB.SetHpf(_mydata->_hipass_cutoff);
  _filterC.SetHpf(_mydata->_hipass_cutoff);
  _filterD.SetHpf(_mydata->_hipass_cutoff);
  _allpassA.Clear();
  _allpassB.Clear();
  _allpassC.Clear();
  _allpassD.Clear();
  _allpassA.set(_mydata->_allpass_shift_frq);
  _allpassB.set(_mydata->_allpass_shift_frq);
  _allpassC.set(_mydata->_allpass_shift_frq);
  _allpassD.set(_mydata->_allpass_shift_frq);
}
} // namespace ork::audio::singularity
