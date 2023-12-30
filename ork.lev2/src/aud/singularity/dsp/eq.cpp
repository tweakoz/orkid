////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/alg_eq.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

PARABASS_DATA::PARABASS_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "PARABASS";
  addParam("fc")->useFrequencyEvaluator();
  addParam("gain")->useAmplitudeEvaluator();
}

dspblk_ptr_t PARABASS_DATA::createInstance() const { // override
  return std::make_shared<PARABASS>(this);
}

PARABASS::PARABASS(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void PARABASS::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  const float* inpbuf    = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* outbuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc   = _param[0].eval();
  float gain = _param[1].eval();
  float pad  = _dbd->_inputPad;

  if (fc > 16000.0f)
    fc = 16000.0f;

  if (gain > 36)
    gain = 36;

  _fval[0] = fc;
  _fval[1] = gain;

  _lsq.set(fc, gain);

  // float ling = decibel_to_linear_amp_ratio(gain);

  //printf( "fc<%f> gain<%f> pad<%f>\n", fc, gain, pad);
  //fc<311.126984> gain<36.000000> pad<0.501187>

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float input = inpbuf[i] * pad;
      float outp  = _lsq.compute(input);
      outbuf[i]     = outp;
    }

  // printf( "ff<%f> wid<%f>\n", ff, wid );
}
void PARABASS::doKeyOn(const KeyOnInfo& koi) // final
{
  _lsq.init();
}

///////////////////////////////////////////////////////////////////////////////

STEEP_RESONANT_BASS_DATA::STEEP_RESONANT_BASS_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "STEEP_RESONANT_BASS";
  addParam("fc")->useFrequencyEvaluator();
  addParam("resonance")->useDefaultEvaluator();
  addParam("gain")->useAmplitudeEvaluator();
}

dspblk_ptr_t STEEP_RESONANT_BASS_DATA::createInstance() const { // override
  return std::make_shared<STEEP_RESONANT_BASS>(this);
}

STEEP_RESONANT_BASS::STEEP_RESONANT_BASS(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void STEEP_RESONANT_BASS::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc   = _param[0].eval() * 1.0f;
  float res  = _param[1].eval();
  float gain = _param[2].eval();

  _fval[1] = res;
  _fval[2] = gain;

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filtFC   = 0.99 * _filtFC + 0.01 * fc;
      float inp = ubuf[i] * pad;

      _svf.SetWithRes(EM_BPF, _filtFC, res);
      _lsq.set(_filtFC, gain);
      _svf.Tick(inp);
      ubuf[i] = _svf.output; //+_lsq.compute(inp);
      ubuf[i] = _svf.output + _lsq.compute(inp);
    }

  _fval[0] = _filtFC;
  // printf( "ff<%f> res<%f>\n", ff, res );
}

void STEEP_RESONANT_BASS::doKeyOn(const KeyOnInfo& koi) // final
{
  _lsq.init();
  _svf.Clear();
  _filtFC = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

PARATREBLE_DATA::PARATREBLE_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "PARATREBLE";
  addParam("fc")->useFrequencyEvaluator();
  addParam("gain")->useAmplitudeEvaluator();
}

dspblk_ptr_t PARATREBLE_DATA::createInstance() const { // override
  return std::make_shared<PARATREBLE>(this);
}

PARATREBLE::PARATREBLE(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void PARATREBLE::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc   = clip_float(_param[0].eval(), 10.0f, 16000.0f);
  float gain = _param[1].eval();
  float pad  = _dbd->_inputPad;

  // if( gain > 42 )
  //    gain = 42;

  _fval[0] = fc;
  _fval[1] = gain;

  _lsq.set(fc, gain);

  // float ling = decibel_to_linear_amp_ratio(gain);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float input = ubuf[i] * pad;
      float outp  = _lsq.compute(input);
      ubuf[i]     = outp;
    }

  // printf( "ff<%f> wid<%f>\n", ff, wid );
}
void PARATREBLE::doKeyOn(const KeyOnInfo& koi) // final
{
  _lsq.init();
}

///////////////////////////////////////////////////////////////////////////////

PARAMID_DATA::PARAMID_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "PARAMID";
  addParam("fc")->useFrequencyEvaluator();
  addParam("gain")->useAmplitudeEvaluator();
}

dspblk_ptr_t PARAMID_DATA::createInstance() const { // override
  return std::make_shared<PARAMID>(this);
}

PARAMID::PARAMID(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void PARAMID::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc   = _param[0].eval();
  float gain = _param[1].eval();
  float pad  = _dbd->_inputPad;
  _fval[0]   = fc;
  _fval[1]   = gain;

  _biquad.SetBpfWithBWoct(fc, 2.2, gain);

  float ling = decibel_to_linear_amp_ratio(gain);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float biq = _biquad.compute(ubuf[i] * pad);
      ubuf[i]   = biq;
    }

  // printf( "ff<%f> wid<%f>\n", ff, wid );
}
void PARAMID::doKeyOn(const KeyOnInfo& koi) // final
{
  _biquad.Clear();
}

///////////////////////////////////////////////////////////////////////////////
ParametricEqData::ParametricEqData(std::string name)
    : DspBlockData(name) {
  auto fc_param    = addParam();
  auto width_param = addParam();
  auto gain_param  = addParam();
  fc_param->useDefaultEvaluator();
  width_param->useDefaultEvaluator();
  gain_param->useDefaultEvaluator();
}
dspblk_ptr_t ParametricEqData::createInstance() const {
  return std::make_shared<ParametricEq>(this);
}

ParametricEq::ParametricEq(const ParametricEqData* dbd)
    : DspBlock(dbd) {
}

void ParametricEq::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  const float* ibuf    = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* obuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc   = _param[0].eval();
  float wid  = clip_float(_param[1].eval(), 0.2, 8);
  float gain = _param[2].eval();
  float pad  = _dbd->_inputPad;

  auto ld = _layer->_layerdata;

  _fval[0] = fc;
  _fval[1] = wid;
  _fval[2] = gain;

  // _biquad.SetParametric2(fc,wid,gain);

  const float kf1 = 0.005f;
  const float kf2 = 1.0f - kf1;

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _smoothFC = fc;   // kf2*_smoothFC + kf1*fc;
      _smoothW  = wid;  // kf2 * _smoothW + kf1 * wid;
      _smoothG  = gain; // kf2*_smoothG + kf1*gain;

      _peq1.set(_smoothFC, _smoothW, _smoothG);

      float inp = ibuf[i] * pad;
      //printf("inp<%g>\n", inp);
      // float outp = _biquad.compute2(inp);
      float outp = _peq1.compute(inp);
      // outp = _peq.proc(1,obuf,fc/48000.0f,wid,gain);
      obuf[i] = inp;
    }
}
void ParametricEq::doKeyOn(const KeyOnInfo& koi) // final
{
  _biquad.Clear();
  _peq.init();
  _peq1.Clear();

  _smoothFC = 0.0f;
  _smoothW  = 0.0f;
  _smoothG  = 0.0f;
}

} // namespace ork::audio::singularity
