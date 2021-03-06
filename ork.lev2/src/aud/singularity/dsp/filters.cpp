#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/alg_filters.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

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
      ubuf[i] = _filter.output;
    }

  // printf( "ff<%f> wid<%f>\n", ff, wid );
}

void NOTCH_FILT::doKeyOn(const KeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////

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
LowPassData::LowPassData(std::string name)
    : DspBlockData(name) {
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

  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      float inp     = inpbuf[i] * pad;
      outputchan[i] = _lpf.compute(inp);
    }
  }
}

void LowPass::doKeyOn(const KeyOnInfo& koi) // final
{
  _lpf.init();
}

///////////////////////////////////////////////////////////////////////////////

HighPassData::HighPassData(std::string name)
    : DspBlockData(name) {
  addParam()->useDefaultEvaluator(); // cutoff
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

HighFreqStimulatorData::HighFreqStimulatorData(std::string name)
    : DspBlockData(name) {
  addParam()->useDefaultEvaluator(); // cutoff
  addParam()->useDefaultEvaluator(); // drive
  addParam()->useDefaultEvaluator(); // outgain
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
  float fc          = _param[0].eval();
  float drv         = _param[1].eval();
  float amp         = _param[2].eval();
  float drvg        = decibel_to_linear_amp_ratio(drv);
  float ling        = decibel_to_linear_amp_ratio(amp);
  if (1)
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

AllPassData::AllPassData(std::string name)
    : DspBlockData(name) {
  addParam()->useDefaultEvaluator(); // cutoff
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
  _filter.Set(fc);
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
TwoPoleAllPassData::TwoPoleAllPassData(std::string name)
    : DspBlockData(name) {
  _blocktype = "TwoPoleAllPass";
  addParam()->useDefaultEvaluator(); // center
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

  _filterL.Set(fc);
  _filterH.Set(fc);
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
TwoPoleLowPassData::TwoPoleLowPassData(std::string name)
    : DspBlockData(name) {
  _blocktype     = "TwoPoleLowPassData";
  auto cutoff    = addParam();
  auto resonance = addParam();
  cutoff->useDefaultEvaluator();
  resonance->useDefaultEvaluator();
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

FourPoleLowPassWithSepData::FourPoleLowPassWithSepData(std::string name)
    : DspBlockData(name) {
  _blocktype = "FourPoleLowPassWithSep";
  addParam()->useDefaultEvaluator(); // cutoff
  addParam()->useDefaultEvaluator(); // resonance
  addParam()->useDefaultEvaluator(); // seperation
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
