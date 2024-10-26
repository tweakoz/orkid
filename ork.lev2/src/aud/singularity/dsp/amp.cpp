////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/alg_pan.inl>
#include <ork/lev2/aud/singularity/modulation.h>

ImplementReflectionX(ork::audio::singularity::AMP_ADAPTIVE_DATA, "DspAmpAdaptive");
ImplementReflectionX(ork::audio::singularity::AMP_MONOIO_DATA, "DspAmpMono");
ImplementReflectionX(ork::audio::singularity::PLUSAMP_DATA, "DspAmpPlus");
ImplementReflectionX(ork::audio::singularity::XAMP_DATA, "DspAmpX");
ImplementReflectionX(ork::audio::singularity::STEREO_GAIN_DATA, "DspAmpStereoGain");
ImplementReflectionX(ork::audio::singularity::GAIN_DATA, "DspAmpMonoGain");
ImplementReflectionX(ork::audio::singularity::BANGAMP_DATA, "DspAmpBang");
ImplementReflectionX(ork::audio::singularity::AMPU_AMPL_DATA, "DspAmpUL");
ImplementReflectionX(ork::audio::singularity::BAL_AMP_DATA, "DspAmpBalance");
ImplementReflectionX(ork::audio::singularity::AMP_MOD_OSC_DATA, "DspAmpModOsc");
ImplementReflectionX(ork::audio::singularity::XGAIN_DATA, "DspAmpXGain");
ImplementReflectionX(ork::audio::singularity::XFADE_DATA, "DspAmpXFade");

namespace ork::audio::singularity {

float shaper(float inp, float adj);
float wrap(float inp, float adj);

///////////////////////////////////////////////////////////////////////////////

void AMP_ADAPTIVE_DATA::describeX(class_t* clazz){

}

AMP_ADAPTIVE_DATA::AMP_ADAPTIVE_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "AMP_ADAPTIVE";
  auto param = addParam("gain","dB");
  param->useAmplitudeEvaluator();
}

dspblk_ptr_t AMP_ADAPTIVE_DATA::createInstance() const { // override
  return std::make_shared<AMP_ADAPTIVE>(this);
}

AMP_ADAPTIVE::AMP_ADAPTIVE(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void AMP_ADAPTIVE::compute(DspBuffer& dspbuf) { // final
  float paramgain = _param[0].eval();         //,0.01f,100.0f);
  int inumframes  = _layer->_dspwritecount;
  const auto& LD  = _layer->_layerdata;
  //////////////////////////////////
  int numinp = _ioconfig->numInputs();
  int numout = _ioconfig->numOutputs();
  //printf( "numinp<%d> numout<%d>\n", numinp, numout );
  //////////////////////////////////
  float laychgain = decibel_to_linear_amp_ratio(LD->_channelGains[0]);
  //printf("paramgain<%g> laychgain<%g> _dbd->_inputPad<%g>\n", paramgain, laychgain, _dbd->_inputPad);
  float baseG = laychgain;
  baseG *= decibel_to_linear_amp_ratio(paramgain)*_dbd->_inputPad;
  //////////////////////////////////
  bool use_natenv = LD->_usenatenv;
  //////////////////////////////////
  switch(numinp){
    case 1:{
      auto bufferL  = getRawBuf(dspbuf, 0) + _layer->_dspwritebase;
      auto bufferR  = getRawBuf(dspbuf, 1) + _layer->_dspwritebase;


      for (int i = 0; i < inumframes; i++) {
        //printf( "_layer->_ampenvgain<%g>\n", _layer->_ampenvgain ); 
        float ampenv = use_natenv //
                     ? 1.0f //
                     : _layer->_ampenvgain;
        float linG = baseG*ampenv;
        float inp     = bufferL[i];


        bufferL[i] = inp * linG;
        bufferR[i] = inp * linG;
      }
      break;
    }
    case 2:{
      auto chanL  = getRawBuf(dspbuf, 0) + _layer->_dspwritebase;
      auto chanR  = getRawBuf(dspbuf, 1) + _layer->_dspwritebase;
      //printf( "outputchanL<%p> outputchanR<%p>\n", outputchanL, outputchanR );
      for (int i = 0; i < inumframes; i++) {
        float ampenv = use_natenv //
                     ? 1.0f //
                     : _layer->_ampenvgain;
        float linG = baseG*ampenv;
        float inpL     = chanL[i];
        float inpR     = chanR[i];
        chanL[i] = inpL * linG;
        chanR[i] = inpR * linG;
      }
      break;
    }
    default:
      OrkAssert(false);
  }
  //////////////////////////////////
  _fval[0] = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void AMP_ADAPTIVE::doKeyOn(const KeyOnInfo& koi) // final
{
  auto LD = koi._layer->_layerdata;
}

///////////////////////////////////////////////////////////////////////////////

void AMP_MONOIO_DATA::describeX(class_t* clazz){

}

AMP_MONOIO_DATA::AMP_MONOIO_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "AMP_MONOIO";
  auto param = addParam("gain","dB");
  param->useAmplitudeEvaluator();
}

dspblk_ptr_t AMP_MONOIO_DATA::createInstance() const { // override
  return std::make_shared<AMP_MONOIO>(this);
}

AMP_MONOIO::AMP_MONOIO(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void AMP_MONOIO::compute(DspBuffer& dspbuf) { // final
  float paramgain = _param[0].eval();         //,0.01f,100.0f);
  int inumframes  = _layer->_dspwritecount;
  const auto& LD  = _layer->_layerdata;
  //////////////////////////////////
  auto inputchan  = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float laychgain = decibel_to_linear_amp_ratio(LD->_channelGains[0]);
  float ampenv = _layer->_ampenvgain;
  //////////////////////////////////
  for (int i = 0; i < inumframes; i++) {
    float linG = paramgain; 
    linG *= laychgain;
    linG *= ampenv;
    float inp     = inputchan[i];
    outputchan[i] = inp * linG * _dbd->_inputPad;
  }
  //////////////////////////////////
  _fval[0] = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void AMP_MONOIO::doKeyOn(const KeyOnInfo& koi) // final
{
  auto LD = koi._layer->_layerdata;
}

///////////////////////////////////////////////////////////////////////////////

void PLUSAMP_DATA::describeX(class_t* clazz){

}

PLUSAMP_DATA::PLUSAMP_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "PLUSAMP";
  addParam("gain")->useDefaultEvaluator(); // position: eval: "POS" 
}
dspblk_ptr_t PLUSAMP_DATA::createInstance() const {
  return std::make_shared<PLUSAMP>(this);
}

void PLUSAMP::initBlock(dspblkdata_ptr_t blockdata) {
  blockdata->_blocktype = "+ AMP";
  auto param            = blockdata->addParam();
  param->useAmplitudeEvaluator();
}

PLUSAMP::PLUSAMP(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void PLUSAMP::compute(DspBuffer& dspbuf) // final
{
  float paramgain = _param[0].eval(); //,0.01f,100.0f);

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;

  auto LD = _layer->_layerdata;

  float laychgain = decibel_to_linear_amp_ratio(LD->_channelGains[0]);
  float baseG = laychgain;
  baseG *= decibel_to_linear_amp_ratio(paramgain)*_dbd->_inputPad;
  bool use_natenv = LD->_usenatenv;
  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1)
    for (int i = 0; i < inumframes; i++) {
      float ampenv = use_natenv ? 1.0f : decibel_to_linear_amp_ratio((1.0-_layer->_ampenvgain)*-96.0f);
      float linG = baseG*ampenv;
      _filt      = 0.99 * _filt + 0.01 * linG;
      //float final_g = decibel_to_linear_amp_ratio(_filt);
      float inU  = ubuf[i] * _filt;
      float inL  = lbuf[i] * _filt;
      //float ae   = _param[1].eval();
      // float ae   = aenv[i];
      float res = (inU + inL) * 0.5 * _filt * 2.0;
      ubuf[i]   = res;
      lbuf[i]   = res;
    }
  _fval[0] = _filt;
}

void PLUSAMP::doKeyOn(const KeyOnInfo& koi) // final
{
  _filt = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void XAMP_DATA::describeX(class_t* clazz){

}

XAMP_DATA::XAMP_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "XAMP";
  addParam("gain")->useDefaultEvaluator(); // position: eval: "POS" 
}
dspblk_ptr_t XAMP_DATA::createInstance() const {
  return std::make_shared<XAMP>(this);
}

void XAMP::initBlock(dspblkdata_ptr_t blockdata) {
  blockdata->_blocktype = "x AMP";
  auto param            = blockdata->addParam();
  param->useAmplitudeEvaluator();
}

XAMP::XAMP(const DspBlockData* dbd)
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

void XAMP::doKeyOn(const KeyOnInfo& koi) // final
{
  _filt = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void GAIN_DATA::describeX(class_t* clazz){

}

GAIN_DATA::GAIN_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "GAIN";
  addParam("gain")->useDefaultEvaluator(); // position: eval: "POS" 
}
dspblk_ptr_t GAIN_DATA::createInstance() const {
  return std::make_shared<GAIN>(this);
}

GAIN::GAIN(const DspBlockData* dbd)
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

void STEREO_GAIN_DATA::describeX(class_t* clazz){

}

STEREO_GAIN_DATA::STEREO_GAIN_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "STEREO_GAIN";
  addParam("gain")->useAmplitudeEvaluator(); 
}
dspblk_ptr_t STEREO_GAIN_DATA::createInstance() const {
  return std::make_shared<STEREO_GAIN>(this);
}

STEREO_GAIN::STEREO_GAIN(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void STEREO_GAIN::compute(DspBuffer& dspbuf) // final
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

void XFADE_DATA::describeX(class_t* clazz){

}

XFADE_DATA::XFADE_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "XFADE";
  addParam("gain")->useDefaultEvaluator(); // position: eval: "POS" 
}
dspblk_ptr_t XFADE_DATA::createInstance() const {
  return std::make_shared<XFADE>(this);
}

XFADE::XFADE(const DspBlockData* dbd)
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

void XFADE::doKeyOn(const KeyOnInfo& koi) // final
{
  _plmix = 0.0f;
  _pumix = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void XGAIN_DATA::describeX(class_t* clazz){

}

XGAIN_DATA::XGAIN_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "XGAIN";
  addParam("gain")->useDefaultEvaluator(); // position: eval: "POS" 
}
dspblk_ptr_t XGAIN_DATA::createInstance() const {
  return std::make_shared<XGAIN>(this);
}

XGAIN::XGAIN(const DspBlockData* dbd)
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
      outputchan[i] = res;
    }
    _fval[0] = _filt;
  }
}
void XGAIN::doKeyOn(const KeyOnInfo& koi) // final
{
  _filt = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void AMPU_AMPL_DATA::describeX(class_t* clazz){

}

AMPU_AMPL_DATA::AMPU_AMPL_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "AMPUAMPL";
  addParam("gainU")->useDefaultEvaluator(); // position: eval: "POS" 
  addParam("gainL")->useDefaultEvaluator(); // position: eval: "POS" 
}
dspblk_ptr_t AMPU_AMPL_DATA::createInstance() const {
  return std::make_shared<AMPU_AMPL>(this);
}

AMPU_AMPL::AMPU_AMPL(const DspBlockData* dbd)
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
  float baseLG = LowerLinG;
  baseLG *= decibel_to_linear_amp_ratio(gainL)*_dbd->_inputPad;
  float baseUG = UpperLinG;
  baseUG *= decibel_to_linear_amp_ratio(gainU)*_dbd->_inputPad;
  bool use_natenv = _layer->_layerdata->_usenatenv;

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filtU      = 0.999 * _filtU + 0.001 * baseUG;
      _filtL      = 0.999 * _filtL + 0.001 * baseLG;
      float inU   = ubuf[i];
      float inL   = lbuf[i];
      float ampenv = use_natenv //
                    ? 1.0f //
                    : _layer->_ampenvgain;
      float resU  = inU * _filtU * ampenv;
      float resL  = inL * _filtL * ampenv;

      ubuf[i] = resU * u_lrmix.lmix + resL * l_lrmix.lmix;
      lbuf[i] = resU * u_lrmix.rmix + resL * l_lrmix.rmix;
    }
  _fval[0] = _filtU;
  _fval[1] = _filtL;
}

void AMPU_AMPL::doKeyOn(const KeyOnInfo& koi) // final
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

void BAL_AMP_DATA::describeX(class_t* clazz){

}

BAL_AMP_DATA::BAL_AMP_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "BALAMP";
  addParam("POS")->useDefaultEvaluator(); // position: eval: "POS" 
  addParam("AMP")->useAmplitudeEvaluator(); // position: eval: "POS" 
}
dspblk_ptr_t BAL_AMP_DATA::createInstance() const {
  return std::make_shared<BAL_AMP>(this);
}

BAL_AMP::BAL_AMP(const DspBlockData* dbd)
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
void BAL_AMP::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void AMP_MOD_OSC_DATA::describeX(class_t* clazz){

}

AMP_MOD_OSC_DATA::AMP_MOD_OSC_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "AMPMODOSC";
  addParam("PITCH")->usePitchEvaluator(); // oscilator pitch 
  addParam("DEP")->useDefaultEvaluator(); // ampmod depth 
}
dspblk_ptr_t AMP_MOD_OSC_DATA::createInstance() const {
  return std::make_shared<AMP_MOD_OSC>(this);
}

AMP_MOD_OSC::AMP_MOD_OSC(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void AMP_MOD_OSC::compute(DspBuffer& dspbuf) // final
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
void AMP_MOD_OSC::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void BANGAMP_DATA::describeX(class_t* clazz){

}

BANGAMP_DATA::BANGAMP_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "!AMP";
  addParam("gain")->useDefaultEvaluator(); // position: eval: "POS" 
}
dspblk_ptr_t BANGAMP_DATA::createInstance() const {
  return std::make_shared<BANGAMP>(this);
}

BANGAMP::BANGAMP(const DspBlockData* dbd)
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

void BANGAMP::doKeyOn(const KeyOnInfo& koi) // final
{
  _smooth = 0.0f;
}


} // namespace ork::audio::singularity
