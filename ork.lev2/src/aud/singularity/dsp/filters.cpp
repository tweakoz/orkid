////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/alg_filters.h>

ImplementReflectionX(ork::audio::singularity::BANDPASS_FILT_DATA, "DspFilterBandPass");
ImplementReflectionX(ork::audio::singularity::BAND2_DATA, "DspFilterBandpass2");
ImplementReflectionX(ork::audio::singularity::NOTCH_FILT_DATA, "DspFilterNotch");
ImplementReflectionX(ork::audio::singularity::NOTCH2_DATA, "DspFilterNotch2");
ImplementReflectionX(ork::audio::singularity::DOUBLE_NOTCH_W_SEP_DATA, "DspFilterDoubleNotchWithSep");
ImplementReflectionX(ork::audio::singularity::LOPAS2_DATA, "DspFilterLowPass2");
ImplementReflectionX(ork::audio::singularity::LP2RES_DATA, "DspFilterLowPass2Res");
ImplementReflectionX(ork::audio::singularity::LPGATE_DATA, "DspFilterLowPassGate");
ImplementReflectionX(ork::audio::singularity::FOURPOLE_HIPASS_W_SEP_DATA, "DspFilter4PoleWithSep");
ImplementReflectionX(ork::audio::singularity::LPCLIP_DATA, "DspFilterLowPassClip");
ImplementReflectionX(ork::audio::singularity::LowPassData, "DspFilterLowPass");
ImplementReflectionX(ork::audio::singularity::HighPassData, "DspFilterHighPass");
ImplementReflectionX(ork::audio::singularity::AllPassData, "DspFilterAllPass");
ImplementReflectionX(ork::audio::singularity::HighFreqStimulatorData, "DspFilterHighFreqStimulator");
ImplementReflectionX(ork::audio::singularity::TwoPoleLowPassData, "DspFilter2PoleLowPass");
ImplementReflectionX(ork::audio::singularity::TwoPoleAllPassData, "DspFilter2PoleAllPass");
ImplementReflectionX(ork::audio::singularity::FourPoleLowPassWithSepData, "DspFilter4PoleLowPassWithSep");


namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void BANDPASS_FILT_DATA::describeX(class_t* clazz){

}

BANDPASS_FILT_DATA::BANDPASS_FILT_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "BANDPASS_FILT";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
  addParam("width")->useDefaultEvaluator();   // P1 width  eval: "WID"
}
dspblk_ptr_t BANDPASS_FILT_DATA::createInstance() const {
  return std::make_shared<BANDPASS_FILT>(this);
}

BANDPASS_FILT::BANDPASS_FILT(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void BANDPASS_FILT::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc  = _param[0].eval();
  float wid = _param[1].eval();

  // printf( "fc<%f> wid<%f>\n", fc, wid );
  _filter.SetWithBWoct(EM_BPF, fc, wid);
  _biquad.SetBpfWithBWoct(fc, wid, -12.0f);
  //_biquad.SetBpfMeth2(fc);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float input = ubuf[i] * pad;
      _filter.Tick(ubuf[i] * pad);
      // ubuf[i] = _filter.output;
      ubuf[i] = _biquad.compute(input);
    }

  _fval[0] = fc;
  _fval[1] = wid;
  // printf( "ff<%f> wid<%f>\n", ff, wid );
}

void BANDPASS_FILT::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter.Clear();
  _biquad.Clear();
}

///////////////////////////////////////////////////////////////////////////////

void BAND2_DATA::describeX(class_t* clazz){}

BAND2_DATA::BAND2_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "BAND2";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
}
dspblk_ptr_t BAND2_DATA::createInstance() const {
  return std::make_shared<BAND2>(this);
}

BAND2::BAND2(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void BAND2::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc = _param[0].eval();

  _fval[0] = fc;

  _filter.SetWithBWoct(EM_BPF, fc, 2.2f);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filter.Tick(ubuf[i] * pad);
      ubuf[i] = _filter.output;
    }

  // printf( "ff<%f> wid<%f>\n", ff, wid );
}

void BAND2::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////

void NOTCH_FILT_DATA::describeX(class_t* clazz){}

NOTCH_FILT_DATA::NOTCH_FILT_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "NOTCH_FILT";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
  addParam("width")->useDefaultEvaluator();   // P1 width  eval: "WID"
}
dspblk_ptr_t NOTCH_FILT_DATA::createInstance() const {
  return std::make_shared<NOTCH_FILT>(this);
}

NOTCH_FILT::NOTCH_FILT(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void NOTCH_FILT::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc  = _param[0].eval();
  float wid = _param[1].eval();
  _fval[0]  = fc;
  _fval[1]  = wid;

  _filter.SetWithBWoct(EM_NOTCH, fc, wid);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filter.Tick(ubuf[i] * pad);
      ubuf[i] = _filter.output*4.0f;
    }

  // printf( "ff<%f> wid<%f>\n", ff, wid );
}

void NOTCH_FILT::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////

void NOTCH2_DATA::describeX(class_t* clazz){}

NOTCH2_DATA::NOTCH2_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "NOTCH2";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
}
dspblk_ptr_t NOTCH2_DATA::createInstance() const {
  return std::make_shared<NOTCH2>(this);
}

NOTCH2::NOTCH2(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void NOTCH2::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc = _param[0].eval();
  _fval[0] = fc;

  _filter1.SetWithBWoct(EM_NOTCH, fc, 2.2f);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filter1.Tick(ubuf[i] * pad);
      ubuf[i] = _filter1.output;
    }

  // printf( "ff<%f> res<%f>\n", ff, res );
}

void NOTCH2::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter1.Clear();
}

///////////////////////////////////////////////////////////////////////////////

void DOUBLE_NOTCH_W_SEP_DATA::describeX(class_t* clazz){}

DOUBLE_NOTCH_W_SEP_DATA::DOUBLE_NOTCH_W_SEP_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "DOUBLE_NOTCH_W_SEP";
  addParam("cutoff")->useFrequencyEvaluator(); // cutoff eval: "FRQ" 
  addParam("resonance")->useDefaultEvaluator();   // Q
  addParam("separation")->useDefaultEvaluator();   // cents
}
dspblk_ptr_t DOUBLE_NOTCH_W_SEP_DATA::createInstance() const {
  return std::make_shared<DOUBLE_NOTCH_W_SEP>(this);
}

DOUBLE_NOTCH_W_SEP::DOUBLE_NOTCH_W_SEP(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void DOUBLE_NOTCH_W_SEP::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc    = _param[0].eval();
  float res   = _param[1].eval();
  float sep   = _param[2].eval();
  _fval[0]    = fc;
  _fval[1]    = res;
  _fval[2]    = sep;
  float ratio = cents_to_linear_freq_ratio(sep);

  _filter1.SetWithRes(EM_NOTCH, fc, res);
  _filter2.SetWithRes(EM_NOTCH, fc * ratio, res);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filter1.Tick(ubuf[i] * pad);
      _filter2.Tick(_filter1.output);
      ubuf[i] = _filter2.output;
    }

  // printf( "ff<%f> res<%f>\n", ff, res );
}

void DOUBLE_NOTCH_W_SEP::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter1.Clear();
  _filter2.Clear();
}

///////////////////////////////////////////////////////////////////////////////
// LOPAS2 = TWOPOLE_LOWPASS (fixed -6dB res)
///////////////////////////////////////////////////////////////////////////////

void LOPAS2_DATA::describeX(class_t* clazz){}

LOPAS2_DATA::LOPAS2_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "LOPAS2";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
}
dspblk_ptr_t LOPAS2_DATA::createInstance() const {
  return std::make_shared<LOPAS2>(this);
}

LOPAS2::LOPAS2(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void LOPAS2::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  auto inpbuf    = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  float fc       = _param[0].eval();
  _fval[0]       = fc;
  // float res = decibel_to_linear_amp_ratio(-6);
  _filter.SetWithRes(EM_LPF, fc, -6);
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      _filter.Tick(inpbuf[i] * pad);
      float output  = _filter.output;
      outputchan[i] = output;
    }
  }
}

void LOPAS2::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////
// LOPAS2 = TWOPOLE_LOWPASS (fixed -6dB res)
///////////////////////////////////////////////////////////////////////////////

void LP2RES_DATA::describeX(class_t* clazz){}

LP2RES_DATA::LP2RES_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "LP2RES";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
}
dspblk_ptr_t LP2RES_DATA::createInstance() const {
  return std::make_shared<LP2RES>(this);
}

LP2RES::LP2RES(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void LP2RES::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  auto inpbuf    = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  float fc       = _param[0].eval();
  _fval[0]       = fc;
  // float res = decibel_to_linear_amp_ratio(12);
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      //_smoothFC = (_smoothFC*0.99f) + fc*.01f;
      _filter.SetWithRes(EM_LPF, fc, 3.0f);
      _filter.Tick(inpbuf[i] * pad);
      float output  = _filter.output;
      outputchan[i] = output;
    }
  }
}

void LP2RES::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////

void FOURPOLE_HIPASS_W_SEP_DATA::describeX(class_t* clazz){}

FOURPOLE_HIPASS_W_SEP_DATA::FOURPOLE_HIPASS_W_SEP_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "4POLE_HIPASS_W_SEP";
  addParam("cutoff")->useFrequencyEvaluator();   // P0 cutoff eval: "FRQ" 
  addParam("resonance")->useDefaultEvaluator();  // Q
  addParam("separation")->useDefaultEvaluator(); // cents
}
dspblk_ptr_t FOURPOLE_HIPASS_W_SEP_DATA::createInstance() const {
  return std::make_shared<FOURPOLE_HIPASS_W_SEP>(this);
}

FOURPOLE_HIPASS_W_SEP::FOURPOLE_HIPASS_W_SEP(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void FOURPOLE_HIPASS_W_SEP::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc  = _param[0].eval();
  float res = _param[1].eval();
  float sep = _param[2].eval();

  _filtFC = 0.95 * _filtFC + 0.05 * fc;

  _fval[0] = _filtFC;
  _fval[1] = res; /// 6.0f;
  _fval[2] = sep;

  float ratio = cents_to_linear_freq_ratio(sep);

  _filter1.SetWithRes(EM_HPF, _filtFC, res);
  _filter2.SetWithRes(EM_HPF, _filtFC * ratio, res);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filter1.Tick(ubuf[i] * pad);
      _filter2.Tick(_filter1.output);
      ubuf[i] = _filter2.output;
    }

  // printf( "fc<%f> res<%f> sep<%f>\n", fc, res, sep );
  // printf( "ff<%f> res<%f>\n", ff, res );
}

void FOURPOLE_HIPASS_W_SEP::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter1.Clear();
  _filter2.Clear();
  _filtFC = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////
// LPCLIP : 1 pole! lowpass
///////////////////////////////////////////////////////////////////////////////

void LPCLIP_DATA::describeX(class_t* clazz){}

LPCLIP_DATA::LPCLIP_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "LPCLIP";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
}
dspblk_ptr_t LPCLIP_DATA::createInstance() const {
  return std::make_shared<LPCLIP>(this);
}

LPCLIP::LPCLIP(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void LPCLIP::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  float fc       = _param[0].eval();
  _fval[0]       = fc;
  _lpf.set(fc);
  if (1) {
    auto inputchan  = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      float inp     = inputchan[i] * pad * 4.0;
      outputchan[i] = softsat(_lpf.compute(inp), 1.0f);
    }
  }
}

void LPCLIP::doKeyOn(const KeyOnInfo& koi) // final
{
  _lpf.init();
}

///////////////////////////////////////////////////////////////////////////////
// LOPASS : 1 pole! lowpass
///////////////////////////////////////////////////////////////////////////////

void LPGATE_DATA::describeX(class_t* clazz){}

LPGATE_DATA::LPGATE_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "LPGATE";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
}
dspblk_ptr_t LPGATE_DATA::createInstance() const {
  return std::make_shared<LPGATE>(this);
}

LPGATE::LPGATE(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void LPGATE::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float fc       = _param[0].eval();
  _fval[0]       = fc;
  _filter.SetWithQ(EM_LPF, fc, 0.5);
  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filter.Tick(ubuf[i] * pad);
      ubuf[i] = _filter.output;
    }
}

void LPGATE::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////
// LOPASS : 1 pole! lowpass
///////////////////////////////////////////////////////////////////////////////

void LowPassData::describeX(class_t* clazz){}

LowPassData::LowPassData(std::string name)
    : DspBlockData(name) {
  _blocktype = "LOPASS";
  auto p = addParam("cutoff","Hz");
  p->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
 // p->_debug = true;
}
dspblk_ptr_t LowPassData::createInstance() const {
  return std::make_shared<LowPass>(this);
}

LowPass::LowPass(const LowPassData* dbd)
    : DspBlock(dbd) {
}

void LowPass::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  float fc       = _param[0].eval();
  if (fc > 16000.0f)
    fc = 16000.0f;
  _fval[0] = fc;
  _lpf.set(fc);


  auto inpbuf = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  auto outbuf = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  if (not _dbd->_bypass ) {
    for (int i = 0; i < inumframes; i++) {
      float inp     = inpbuf[i] * pad;
      outbuf[i] = _lpf.compute(inp)*0.5;
    }
  }
}

void LowPass::doKeyOn(const KeyOnInfo& koi) // final
{
  _lpf.init();
}

///////////////////////////////////////////////////////////////////////////////

void HighPassData::describeX(class_t* clazz){}

HighPassData::HighPassData(std::string name)
    : DspBlockData(name) {
  _blocktype = "HIPASS";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
}
dspblk_ptr_t HighPassData::createInstance() const {
  return std::make_shared<HighPass>(this);
}
HighPass::HighPass(const HighPassData* dbd)
    : DspBlock(dbd) {
}

void HighPass::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  float fc       = _param[0].eval();
  _hpf.set(fc);

  if (1) {
    auto inputchan  = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      float inp     = inputchan[i] * pad;
      outputchan[i] = _hpf.compute(inp);
    }
  }
  _fval[0] = fc;
}

void HighPass::doKeyOn(const KeyOnInfo& koi) // final
{
  _hpf.init();
}
///////////////////////////////////////////////////////////////////////////////

void HighFreqStimulatorData::describeX(class_t* clazz){}

HighFreqStimulatorData::HighFreqStimulatorData(std::string name)
    : DspBlockData(name) {
  _blocktype = "HIGH_FREQ_STIMULATOR";
  addParam("cutoff","Hz")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
  addParam("drive","dB")->useDefaultEvaluator(); // drive
  addParam("gain","dB")->useDefaultEvaluator(); // outgain
}
dspblk_ptr_t HighFreqStimulatorData::createInstance() const {
  return std::make_shared<HighFreqStimulator>(this);
}

HighFreqStimulator::HighFreqStimulator(const HighFreqStimulatorData* dbd)
    : DspBlock(dbd) {
}

void HighFreqStimulator::compute(DspBuffer& dspbuf) // final
{
  float pad         = _dbd->_inputPad;
  int inumframes    = _layer->_dspwritecount;
  const float* ibuf = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* obuf       = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float fc          = _param[0].eval()*0.3;
  float drv         = _param[1].eval();
  float amp         = _param[2].eval();
  float drvg        = decibel_to_linear_amp_ratio(drv);
  float ling        = decibel_to_linear_amp_ratio(amp);
  //printf("pad<%f> fc<%f> drv<%f> amp<%f>\n", pad, fc, drv, amp);
  if (not _dbd->_bypass)
    for (int i = 0; i < inumframes; i++) {
      float input = ibuf[i] * pad;
      _filter1.SetWithRes(EM_HPF, fc, 0.0f);
      _filter1.Tick(input);
      float postf     = _filter1.output * drvg;
      float saturated = softsat(postf, 0.9f);
      _filter2.SetWithRes(EM_HPF, fc, 0.0f);
      _filter2.Tick(saturated);
      float stimmed = _filter2.output;

      obuf[i] = (input + stimmed * ling);
    }
}

void HighFreqStimulator::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter1.Clear();
  _filter2.Clear();
}

///////////////////////////////////////////////////////////////////////////////
// 2pole allpass (for phasers, etc..)
///////////////////////////////////////////////////////////////////////////////

void AllPassData::describeX(class_t* clazz){}

AllPassData::AllPassData(std::string name)
    : DspBlockData(name) {
  _blocktype = "ALLPASS";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
}
dspblk_ptr_t AllPassData::createInstance() const {
  return std::make_shared<AllPass>(this);
}

AllPass::AllPass(const AllPassData* dbd)
    : DspBlock(dbd) {
}

void AllPass::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;

  float fc = _param[0].eval();
  _filter.set(fc);
  _fval[0] = fc;

  if (1) {
    auto inputchan  = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      outputchan[i] = _filter.Tick(inputchan[i] * pad);
    }
  }
}

void AllPass::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////

void TwoPoleAllPassData::describeX(class_t* clazz){}

TwoPoleAllPassData::TwoPoleAllPassData(std::string name)
    : DspBlockData(name) {
  _blocktype = "TwoPoleAllPass";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
  addParam()->useDefaultEvaluator(); // width
}
dspblk_ptr_t TwoPoleAllPassData::createInstance() const {
  return std::make_shared<TwoPoleAllPass>(this);
}
TwoPoleAllPass::TwoPoleAllPass(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void TwoPoleAllPass::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  auto inpbuf    = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  auto outbuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc  = _param[0].eval();
  float wid = _param[1].eval();
  _fval[0]  = fc;
  _fval[1]  = wid;

  _filterL.set(fc);
  _filterH.set(fc);
  // printf( "fc<%f>\n", fc );
  if (1)
    for (int i = 0; i < inumframes; i++) {
      float f1  = _filterL.Tick(inpbuf[i] * pad);
      outbuf[i] = _filterH.Tick(f1);
    }

  // printf( "ff<%f> res<%f>\n", ff, res );
}

void TwoPoleAllPass::doKeyOn(const KeyOnInfo& koi) // final
{
  _filterL.Clear();
  _filterH.Clear();
}

///////////////////////////////////////////////////////////////////////////////

void TwoPoleLowPassData::describeX(class_t* clazz){}

TwoPoleLowPassData::TwoPoleLowPassData(std::string name)
    : DspBlockData(name) {
  _blocktype     = "TwoPoleLowPassData";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
  addParam("resonance")->useDefaultEvaluator(); // Q
}
dspblk_ptr_t TwoPoleLowPassData::createInstance() const {
  return std::make_shared<TwoPoleLowPass>(this);
}

TwoPoleLowPass::TwoPoleLowPass(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
void TwoPoleLowPass::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  auto inpbuf    = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  auto outbuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc  = _param[0].eval();
  float res = _param[1].eval() * 0.25;
  _fval[0]  = fc;
  _fval[1]  = res;

  // printf( "fc<%f>\n", fc );
  if (1) {
    for (int i = 0; i < inumframes; i++) {
      _smoothFC = (_smoothFC * 0.99f) + fc * .01f;
      _filter.SetWithRes(EM_LPF, fc, res);
      _filter.Tick(inpbuf[i] * pad);
      outbuf[i] = _filter.output;
    }
  }

  // printf( "ff<%f> res<%f>\n", ff, res );
}

void TwoPoleLowPass::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter.Clear();
  _smoothFC = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void FourPoleLowPassWithSepData::describeX(class_t* clazz){}

FourPoleLowPassWithSepData::FourPoleLowPassWithSepData(std::string name)
    : DspBlockData(name) {
  _blocktype = "FourPoleLowPassWithSep";
  addParam("cutoff")->useFrequencyEvaluator(); // P0 cutoff eval: "FRQ" 
  addParam("resonance")->useDefaultEvaluator(); // resonance Q
  addParam("separation")->useDefaultEvaluator(); // seperation (cents)
}
dspblk_ptr_t FourPoleLowPassWithSepData::createInstance() const { // override
  return std::make_shared<FourPoleLowPassWithSep>(this);
}

FourPoleLowPassWithSep::FourPoleLowPassWithSep(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void FourPoleLowPassWithSep::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = _layer->_dspwritecount;
  auto ibuf      = getInpBuf(dspbuf, 0) + _layer->_dspwritebase;
  auto obuf      = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  float fc  = _param[0].eval();
  float res = _param[1].eval();
  float sep = _param[2].eval();

  float ratio = cents_to_linear_freq_ratio(sep);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filtFC = 0.99 * _filtFC + 0.01 * fc;

      _filter1.SetWithRes(EM_LPF, _filtFC, res);
      _filter2.SetWithRes(EM_LPF, _filtFC * ratio, res);
      _filter1.Tick(ibuf[i] * pad);
      _filter2.Tick(_filter1.output);
      obuf[i] = _filter2.output;
    }

  _fval[0] = _filtFC;
  _fval[1] = res;
  _fval[2] = sep;
  // printf( "fc<%f> res<%f> sep<%f>\n", fc, res, sep );
}

void FourPoleLowPassWithSep::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter1.Clear();
  _filter2.Clear();
  _filtFC = 0.0f;
}

} // namespace ork::audio::singularity
