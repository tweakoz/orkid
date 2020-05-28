#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/modulation.h>

namespace ork::audio::singularity {

float shaper(float inp, float adj);
float wrap(float inp, float adj);
const float kmaxclip = 16.0f;  // 18dB
const float kminclip = -16.0f; // 18dB

///////////////////////////////////////////////////////////////////////////////

struct panLR {
  float lmix, rmix;
};

panLR panBlend(float inp) {
  panLR rval;
  rval.lmix = (inp > 0) ? lerp(0.5, 0, inp) : lerp(0.5, 1, -inp);
  rval.rmix = (inp > 0) ? lerp(0.5, 1, inp) : lerp(0.5, 0, -inp);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void AMP_MONOIO::initBlock(dspblkdata_ptr_t blockdata) {
  blockdata->_dspBlock = "AMP_MONOIO";
  blockdata->_paramd[0].useAmplitudeEvaluator();
}

AMP_MONOIO::AMP_MONOIO(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void AMP_MONOIO::compute(DspBuffer& dspbuf) // final
{
  float gain = _param[0].eval(); //,0.01f,100.0f);

  int inumframes = _layer->_dspwritecount;

  const auto& LD = _layer->_layerdata;

  // printf( "amp numinp<%d>\n", numInputs() );
  auto inputchan   = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  auto outputchan  = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float SingleLinG = decibel_to_linear_amp_ratio(LD->_channelGains[0]);

  for (int i = 0; i < inumframes; i++) {
    _filt      = 0.995 * _filt + 0.005 * gain;
    float linG = decibel_to_linear_amp_ratio(_filt);
    linG *= SingleLinG;
    float inp     = inputchan[i];
    float ae      = _param[1].eval();
    outputchan[i] = clip_float(inp * linG * _dbd->_inputPad * ae, kminclip, kmaxclip);
  }
  _fval[0] = _filt;
}

void AMP_MONOIO::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filt   = 0.0f;
  auto LD = koi._layer->_layerdata;
}

///////////////////////////////////////////////////////////////////////////////

void AMP::initBlock(dspblkdata_ptr_t blockdata) {
  blockdata->_dspBlock = "AMP";
  auto& param          = blockdata->addParam();
  param.useAmplitudeEvaluator();
}

AMP::AMP(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void AMP::compute(DspBuffer& dspbuf) // final
{
  float gain     = _param[0].eval();
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  const auto& LD = _layer->_layerdata;
  auto l_lrmix   = panBlend(_lpan);

  if (numInputs() == 1) {
    auto ibuf        = getInpBuf(dspbuf, 0) + ibase;
    auto lbuf        = getOutBuf(dspbuf, 1) + ibase;
    auto ubuf        = getOutBuf(dspbuf, 0) + ibase;
    float SingleLinG = decibel_to_linear_amp_ratio(LD->_channelGains[0]);

    for (int i = 0; i < inumframes; i++) {
      float linG = decibel_to_linear_amp_ratio(gain);
      linG *= SingleLinG;
      float inp  = ibuf[i];
      float mono = clip_float(inp * gain * _dbd->_inputPad, kminclip, kmaxclip);
      ubuf[i]    = mono * l_lrmix.lmix;
      lbuf[i]    = mono * l_lrmix.rmix;
    }
  } else if (numInputs() == 2) {
    auto ilbuf      = getInpBuf(dspbuf, 1 + ibase);
    auto iubuf      = getInpBuf(dspbuf, 0) + ibase;
    auto lbuf       = getOutBuf(dspbuf, 1) + ibase;
    auto ubuf       = getOutBuf(dspbuf, 0) + ibase;
    auto u_lrmix    = panBlend(_upan);
    float UpperLinG = decibel_to_linear_amp_ratio(LD->_channelGains[0]);
    float LowerLinG = decibel_to_linear_amp_ratio(LD->_channelGains[1]);

    for (int i = 0; i < inumframes; i++) {
      // printf("AMPSTER\n");
      float linG = decibel_to_linear_amp_ratio(gain);
      float totG = gain * _dbd->_inputPad;

      float inpU = iubuf[i] * totG;
      float inpL = ilbuf[i] * totG;
      inpU *= UpperLinG;
      inpL *= LowerLinG;

      ubuf[i] = inpU;
      lbuf[i] = inpL;
    }
  }
  _fval[0] = _filt;
}

void AMP::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filt       = 0.0f;
  auto LD     = koi._layer->_layerdata;
  float fpanu = float(LD->_channelPans[0]) / 7.0f;
  float fpanl = float(LD->_channelPans[1]) / 7.0f;
  _upan       = fpanu;
  _lpan       = fpanl;
}

///////////////////////////////////////////////////////////////////////////////

void PLUSAMP::initBlock(dspblkdata_ptr_t blockdata) {
  blockdata->_dspBlock = "+ AMP";
  auto& param          = blockdata->addParam();
  param.useAmplitudeEvaluator();
}

PLUSAMP::PLUSAMP(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void PLUSAMP::compute(DspBuffer& dspbuf) // final
{
  float gain = _param[0].eval(); //,0.01f,100.0f);

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;

  auto LD = _layer->_layerdata;

  float LinG = decibel_to_linear_amp_ratio(LD->_channelGains[0]);

  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filt      = 0.999 * _filt + 0.001 * gain;
      float linG = decibel_to_linear_amp_ratio(_filt);
      float inU  = ubuf[i] * _dbd->_inputPad * LinG;
      float inL  = lbuf[i] * _dbd->_inputPad * LinG;
      float ae   = _param[1].eval();
      // float ae   = aenv[i];
      float res = (inU + inL) * 0.5 * linG * ae * 2.0;
      res       = clip_float(res, -2, 2);
      ubuf[i]   = res;
      lbuf[i]   = res;
    }
  _fval[0] = _filt;
}

void PLUSAMP::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filt = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void XAMP::initBlock(dspblkdata_ptr_t blockdata) {
  blockdata->_dspBlock = "x AMP";
  auto& param          = blockdata->addParam();
  param.useAmplitudeEvaluator();
}

XAMP::XAMP(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void XAMP::compute(DspBuffer& dspbuf) // final
{
  float gain = _param[0].eval(); //,0.01f,100.0f);

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;

  auto LD    = _layer->_layerdata;
  float LinG = decibel_to_linear_amp_ratio(LD->_channelGains[0]);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filt      = 0.999 * _filt + 0.001 * gain;
      float linG = decibel_to_linear_amp_ratio(_filt);
      float inU  = ubuf[i] * _dbd->_inputPad;
      float inL  = lbuf[i] * _dbd->_inputPad;
      // float ae   = aenv[i];
      float ae  = _param[1].eval();
      float res = (inU * inL) * linG * ae * LinG;
      lbuf[i]   = res;
      ubuf[i]   = res;
    }
  _fval[0] = _filt;
}

void XAMP::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filt = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

GAIN::GAIN(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void GAIN::compute(DspBuffer& dspbuf) // final
{
  float gain = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]   = gain;

  if (1) {
    float linG      = decibel_to_linear_amp_ratio(gain);
    int inumframes  = _layer->_dspwritecount;
    auto inpbuf     = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      float inp     = inpbuf[i] * _dbd->_inputPad;
      float outp    = softsat(inp * linG, 1);
      outputchan[i] = outp;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

XFADE::XFADE(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void XFADE::compute(DspBuffer& dspbuf) // final
{
  float index = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]    = index;

  float mix  = index * 0.01f;
  float lmix = (mix > 0) ? lerp(0.5, 0, mix) : lerp(0.5, 1, -mix);
  float umix = (mix > 0) ? lerp(0.5, 1, mix) : lerp(0.5, 0, -mix);

  int inumframes = _layer->_dspwritecount;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      float inputU = ubuf[i] * _dbd->_inputPad;
      float inputL = lbuf[i] * _dbd->_inputPad;
      _plmix       = _plmix * 0.995f + lmix * 0.005f;
      _pumix       = _pumix * 0.995f + umix * 0.005f;

      float mixed   = (inputU * _pumix) + (inputL * _plmix);
      outputchan[i] = mixed;
      // ubuf[i] = inputU+inputL;//(inputU*_pumix)+(inputL*_plmix);
    }
  }
}

void XFADE::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _plmix = 0.0f;
  _pumix = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

XGAIN::XGAIN(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void XGAIN::compute(DspBuffer& dspbuf) // final
{
  float gain = _param[0].eval(); //,0.01f,100.0f);

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;

  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      _filt      = 0.999 * _filt + 0.001 * gain;
      float linG = decibel_to_linear_amp_ratio(_filt);
      float inU  = ubuf[i] * _dbd->_inputPad;
      float inL  = lbuf[i] * _dbd->_inputPad;
      float res  = (inU * inL) * linG;
      res        = clip_float(res, -1, 1);

      outputchan[i] = res;
    }
    _fval[0] = _filt;
  }
}
void XGAIN::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filt = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

AMPU_AMPL::AMPU_AMPL(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void AMPU_AMPL::compute(DspBuffer& dspbuf) // final
{
  float gainU = _param[0].eval();
  float gainL = _param[1].eval();

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;

  auto u_lrmix = panBlend(_upan);
  auto l_lrmix = panBlend(_lpan);

  const auto& layd = _layer->_layerdata;
  float LowerLinG  = decibel_to_linear_amp_ratio(layd->_channelGains[0]);
  float UpperLinG  = decibel_to_linear_amp_ratio(layd->_channelGains[1]);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filtU      = 0.999 * _filtU + 0.001 * gainU;
      _filtL      = 0.999 * _filtL + 0.001 * gainL;
      float linGU = decibel_to_linear_amp_ratio(_filtU);
      float linGL = decibel_to_linear_amp_ratio(_filtL);
      float inU   = ubuf[i] * _dbd->_inputPad;
      float inL   = lbuf[i] * _dbd->_inputPad;
      float ae    = _param[1].eval();
      float resU  = inU * linGU * ae * UpperLinG;
      float resL  = inL * linGL * ae * LowerLinG;

      ubuf[i] = clip_float(resU * u_lrmix.lmix + resL * l_lrmix.lmix, kminclip, kmaxclip);
      lbuf[i] = clip_float(resU * u_lrmix.rmix + resL * l_lrmix.rmix, kminclip, kmaxclip);
    }
  _fval[0] = _filtU;
  _fval[1] = _filtL;
}

void AMPU_AMPL::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filtU      = 0.0f;
  _filtL      = 0.0f;
  auto LD     = koi._layer->_layerdata;
  float fpanu = float(LD->_channelPans[0]) / 7.0f;
  float fpanl = float(LD->_channelPans[1]) / 7.0f;
  _upan       = fpanu;
  _lpan       = fpanl;
}

///////////////////////////////////////////////////////////////////////////////

BAL_AMP::BAL_AMP(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void BAL_AMP::compute(DspBuffer& dspbuf) // final
{
  float gain = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]   = gain;

  float linG = decibel_to_linear_amp_ratio(gain);

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float inp = ubuf[i];
      ubuf[i]   = inp * linG;
    }
}
void BAL_AMP::doKeyOn(const DspKeyOnInfo& koi) // final
{
}
///////////////////////////////////////////////////////////////////////////////

PANNER::PANNER(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}
void PANNER::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;
  float pos      = _param[0].eval();
  float pan      = pos * 0.01f;
  float lmix     = (pan > 0) ? lerp(0.5, 0, pan) : lerp(0.5, 1, -pan);
  float rmix     = (pan > 0) ? lerp(0.5, 1, pan) : lerp(0.5, 0, -pan);

  _fval[0] = pos;

  // printf( "pan<%f> lmix<%f> rmix<%f>\n", pan, lmix, rmix );
  if (1)
    for (int i = 0; i < inumframes; i++) {
      float input = ubuf[i] * _dbd->_inputPad;
      _plmix      = _plmix * 0.995f + lmix * 0.005f;
      _prmix      = _prmix * 0.995f + rmix * 0.005f;

      ubuf[i] = input * _plmix;
      lbuf[i] = input * _prmix;
    }
}
void PANNER::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _plmix = 0.0f;
  _prmix = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

BANGAMP::BANGAMP(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void BANGAMP::compute(DspBuffer& dspbuf) // final
{
  float gain = _param[0].eval(); //,0.01f,100.0f);

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;

  auto LD    = _layer->_layerdata;
  float LinG = decibel_to_linear_amp_ratio(LD->_channelGains[0]);

  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1)
    for (int i = 0; i < inumframes; i++) {
      _smooth    = 0.999 * _smooth + 0.001 * gain;
      float linG = decibel_to_linear_amp_ratio(_smooth);
      float inU  = ubuf[i] * _dbd->_inputPad;
      float inL  = lbuf[i] * _dbd->_inputPad;
      float ae   = _param[1].eval();
      float res  = (inU + inL);
      res        = shaper(res, .25) * linG * ae * LinG;
      lbuf[i]    = res;
      ubuf[i]    = res;
    }
  _fval[0] = _smooth;
}

void BANGAMP::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _smooth = 0.0f;
}

} // namespace ork::audio::singularity
