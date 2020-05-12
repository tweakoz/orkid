#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/alg_filters.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

BANDPASS_FILT::BANDPASS_FILT(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void BANDPASS_FILT::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);

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

void BANDPASS_FILT::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter.Clear();
  _biquad.Clear();
}

///////////////////////////////////////////////////////////////////////////////

BAND2::BAND2(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void BAND2::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);

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

void BAND2::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////

NOTCH_FILT::NOTCH_FILT(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void NOTCH_FILT::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);

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

void NOTCH_FILT::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////

NOTCH2::NOTCH2(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void NOTCH2::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);

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

void NOTCH2::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter1.Clear();
}

///////////////////////////////////////////////////////////////////////////////

DOUBLE_NOTCH_W_SEP::DOUBLE_NOTCH_W_SEP(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void DOUBLE_NOTCH_W_SEP::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);

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

void DOUBLE_NOTCH_W_SEP::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter1.Clear();
  _filter2.Clear();
}

///////////////////////////////////////////////////////////////////////////////

TWOPOLE_ALLPASS::TWOPOLE_ALLPASS(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void TWOPOLE_ALLPASS::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);

  float fc  = _param[0].eval();
  float wid = _param[1].eval();
  _fval[0]  = fc;
  _fval[1]  = wid;

  _filterL.Set(fc);
  _filterH.Set(fc);
  // printf( "fc<%f>\n", fc );
  if (1)
    for (int i = 0; i < inumframes; i++) {
      float f1 = _filterL.Tick(ubuf[i] * pad);
      ubuf[i]  = _filterH.Tick(f1);
    }

  // printf( "ff<%f> res<%f>\n", ff, res );
}

void TWOPOLE_ALLPASS::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filterL.Clear();
  _filterH.Clear();
}

///////////////////////////////////////////////////////////////////////////////

TWOPOLE_LOWPASS::TWOPOLE_LOWPASS(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void TWOPOLE_LOWPASS::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  auto inpbuf    = getInpBuf(dspbuf, 0);

  float fc  = _param[0].eval();
  float res = _param[1].eval() * 0.25;
  _fval[0]  = fc;
  _fval[1]  = res;

  // printf( "fc<%f>\n", fc );
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0);
    for (int i = 0; i < inumframes; i++) {
      _smoothFC = (_smoothFC * 0.99f) + fc * .01f;
      _filter.SetWithRes(EM_LPF, fc, res);
      _filter.Tick(inpbuf[i] * pad);
      float output = _filter.output;
      // output = inpbuf[i]*pad;
      outputchan[i] = output;
    }
  }

  // printf( "ff<%f> res<%f>\n", ff, res );
}

void TWOPOLE_LOWPASS::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter.Clear();
  _smoothFC = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////
// LOPAS2 = TWOPOLE_LOWPASS (fixed -6dB res)
///////////////////////////////////////////////////////////////////////////////

LOPAS2::LOPAS2(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void LOPAS2::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  auto inpbuf    = getInpBuf(dspbuf, 0);
  float fc       = _param[0].eval();
  _fval[0]       = fc;
  // float res = decibel_to_linear_amp_ratio(-6);
  _filter.SetWithRes(EM_LPF, fc, -6);
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0);
    for (int i = 0; i < inumframes; i++) {
      _filter.Tick(inpbuf[i] * pad);
      float output  = _filter.output;
      outputchan[i] = output;
    }
  }
}

void LOPAS2::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////
// LOPAS2 = TWOPOLE_LOWPASS (fixed -6dB res)
///////////////////////////////////////////////////////////////////////////////

LP2RES::LP2RES(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void LP2RES::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  auto inpbuf    = getInpBuf(dspbuf, 0);
  float fc       = _param[0].eval();
  _fval[0]       = fc;
  // float res = decibel_to_linear_amp_ratio(12);
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0);
    for (int i = 0; i < inumframes; i++) {
      //_smoothFC = (_smoothFC*0.99f) + fc*.01f;
      _filter.SetWithRes(EM_LPF, fc, 3.0f);
      _filter.Tick(inpbuf[i] * pad);
      float output  = _filter.output;
      outputchan[i] = output;
    }
  }
}

void LP2RES::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////

FOURPOLE_LOPASS_W_SEP::FOURPOLE_LOPASS_W_SEP(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void FOURPOLE_LOPASS_W_SEP::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);

  float fc  = _param[0].eval();
  float res = _param[1].eval();
  float sep = _param[2].eval();

  float ratio = cents_to_linear_freq_ratio(sep);

  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filtFC = 0.99 * _filtFC + 0.01 * fc;

      _filter1.SetWithRes(EM_LPF, _filtFC, res);
      _filter2.SetWithRes(EM_LPF, _filtFC * ratio, res);
      _filter1.Tick(ubuf[i] * pad);
      _filter2.Tick(_filter1.output);
      ubuf[i] = _filter2.output;
    }

  _fval[0] = _filtFC;
  _fval[1] = res;
  _fval[2] = sep;
  // printf( "fc<%f> res<%f> sep<%f>\n", fc, res, sep );
}

void FOURPOLE_LOPASS_W_SEP::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter1.Clear();
  _filter2.Clear();
  _filtFC = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

FOURPOLE_HIPASS_W_SEP::FOURPOLE_HIPASS_W_SEP(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void FOURPOLE_HIPASS_W_SEP::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);

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

void FOURPOLE_HIPASS_W_SEP::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter1.Clear();
  _filter2.Clear();
  _filtFC = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////
// LOPASS : 1 pole! lowpass
///////////////////////////////////////////////////////////////////////////////

LOPASS::LOPASS(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void LOPASS::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float fc       = _param[0].eval();
  if (fc > 16000.0f)
    fc = 16000.0f;
  _fval[0] = fc;
  _lpf.set(fc);

  auto inpbuf = getInpBuf(dspbuf, 0);

  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0);
    for (int i = 0; i < inumframes; i++) {
      float inp     = inpbuf[i] * pad;
      outputchan[i] = _lpf.compute(inp);
    }
  }
}

void LOPASS::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _lpf.init();
}

///////////////////////////////////////////////////////////////////////////////
// LPCLIP : 1 pole! lowpass
///////////////////////////////////////////////////////////////////////////////

LPCLIP::LPCLIP(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void LPCLIP::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float fc       = _param[0].eval();
  _fval[0]       = fc;
  _lpf.set(fc);
  if (1) {
    auto inputchan  = getInpBuf(dspbuf, 0);
    auto outputchan = getOutBuf(dspbuf, 0);
    for (int i = 0; i < inumframes; i++) {
      float inp     = inputchan[i] * pad * 4.0;
      outputchan[i] = softsat(_lpf.compute(inp), 1.0f);
    }
  }
}

void LPCLIP::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _lpf.init();
}

///////////////////////////////////////////////////////////////////////////////

HIPASS::HIPASS(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void HIPASS::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float fc       = _param[0].eval();
  _hpf.set(fc);

  if (1) {
    auto inputchan  = getInpBuf(dspbuf, 0);
    auto outputchan = getOutBuf(dspbuf, 0);
    for (int i = 0; i < inumframes; i++) {
      float inp     = inputchan[i] * pad;
      outputchan[i] = _hpf.compute(inp);
    }
  }
  _fval[0] = fc;
}

void HIPASS::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _hpf.init();
}

///////////////////////////////////////////////////////////////////////////////
// LOPASS : 1 pole! lowpass
///////////////////////////////////////////////////////////////////////////////

LPGATE::LPGATE(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void LPGATE::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);
  float fc       = _param[0].eval();
  _fval[0]       = fc;
  _filter.SetWithQ(EM_LPF, fc, 0.5);
  if (1)
    for (int i = 0; i < inumframes; i++) {
      _filter.Tick(ubuf[i] * pad);
      ubuf[i] = _filter.output;
    }
}

void LPGATE::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter.Clear();
}

///////////////////////////////////////////////////////////////////////////////

HIFREQ_STIMULATOR::HIFREQ_STIMULATOR(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void HIFREQ_STIMULATOR::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);
  float fc       = _param[0].eval();
  float drv      = _param[1].eval();
  float amp      = _param[2].eval();
  float ling     = decibel_to_linear_amp_ratio(amp);
  float drvg     = decibel_to_linear_amp_ratio(drv);
  if (1)
    for (int i = 0; i < inumframes; i++) {
      float input = ubuf[i] * pad;
      _smoothFC   = _smoothFC * .99 + fc * .01;
      _filter1.SetWithRes(EM_HPF, _smoothFC, 0.0f);
      _filter1.Tick(input);
      float postf = _filter1.output * drvg;
      // float cc = clip_float(postf,-.1,.1);
      float saturated = softsat(postf, 0.9f);
      _filter2.SetWithRes(EM_HPF, _smoothFC, 0.0f);
      _filter2.Tick(saturated);
      float stimmed = _filter2.output;

      ubuf[i] = clip_float(input + stimmed * 0.01, -1, 1); //*ling;
    }
  _fval[0] = _smoothFC;
  _fval[1] = drv;
  _fval[2] = amp;
}

void HIFREQ_STIMULATOR::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter1.Clear();
  _filter2.Clear();
  _smoothFC = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////
// 2pole allpass (for phasers, etc..)
///////////////////////////////////////////////////////////////////////////////

ALPASS::ALPASS(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void ALPASS::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd->_inputPad;
  int inumframes = dspbuf._numframes;

  float fc = _param[0].eval();
  _filter.Set(fc);
  _fval[0] = fc;

  if (1) {
    auto inputchan  = getInpBuf(dspbuf, 0);
    auto outputchan = getOutBuf(dspbuf, 0);
    for (int i = 0; i < inumframes; i++) {
      outputchan[i] = _filter.Tick(inputchan[i] * pad);
    }
  }
}

void ALPASS::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _filter.Clear();
}

} // namespace ork::audio::singularity
