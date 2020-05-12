#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/cz101.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/modulation.h>

namespace ork::audio::singularity {

float shaper(float inp, float adj);
float wrap(float inp, float adj);

///////////////////////////////////////////////////////////////////////////////

SINE::SINE(dspblkdata_constptr_t dbd)
    : DspBlock(dbd)
    , _pblep(48000, PolyBLEP::SINE) {
}

void SINE::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = dspbuf._numframes;
  //    float* ubuf = dspbuf.channel(0);
  float lyrcents = _layer->_layerBasePitch;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);
  // printf("lc<%f> coff<%f> cin<%f> frq<%f>\n", lyrcents, centoff, cin, frq );
  float SR = _layer->_syn._sampleRate;
  _pblep.setFrequency(frq);

  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0);
    for (int i = 0; i < inumframes; i++) {
      outputchan[i] = _pblep.getAndInc();
    }
  }
}
void SINE::doKeyOn(const DspKeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

SAW::SAW(dspblkdata_constptr_t dbd)
    : DspBlock(dbd)
    , _pblep(48000, PolyBLEP::SAWTOOTH) {
}

void SAW::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = dspbuf._numframes;
  // float* ubuf = dspbuf.channel(0);
  float lyrcents = _layer->_layerBasePitch;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);
  float SR       = _layer->_syn._sampleRate;
  _pblep.setFrequency(frq);

  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0);
    for (int i = 0; i < inumframes; i++) {
      outputchan[i] = _pblep.getAndInc();
    }
  }
}
void SAW::doKeyOn(const DspKeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

SQUARE::SQUARE(dspblkdata_constptr_t dbd)
    : DspBlock(dbd)
    , _pblep(48000, PolyBLEP::SQUARE) {
}

void SQUARE::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = dspbuf._numframes;
  // float* ubuf = dspbuf.channel(0);
  float lyrcents = _layer->_layerBasePitch;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);
  float SR       = _layer->_syn._sampleRate;
  _pblep.setFrequency(frq);

  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0);
    for (int i = 0; i < inumframes; i++) {
      outputchan[i] = _pblep.getAndInc();
    }
  }
}
void SQUARE::doKeyOn(const DspKeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

SINEPLUS::SINEPLUS(dspblkdata_constptr_t dbd)
    : DspBlock(dbd)
    , _pblep(48000, PolyBLEP::SINE) {
}

void SINEPLUS::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);
  float lyrcents = _layer->_layerBasePitch;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);
  float SR       = _layer->_syn._sampleRate;
  _pblep.setFrequency(frq);
  float pad = _dbd->_inputPad;

  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1)
    for (int i = 0; i < inumframes; i++) {
      float input  = ubuf[i] * pad;
      float saw    = _pblep.getAndInc();
      float swplus = input + saw;
      ubuf[i]      = swplus;
    }
}
void SINEPLUS::doKeyOn(const DspKeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

SAWPLUS::SAWPLUS(dspblkdata_constptr_t dbd)
    : DspBlock(dbd)
    , _pblep(48000, PolyBLEP::SAWTOOTH) {
}

void SAWPLUS::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = dspbuf._numframes;
  float lyrcents = _layer->_layerBasePitch;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);
  float SR       = _layer->_syn._sampleRate;
  _pblep.setFrequency(frq);
  float pad = _dbd->_inputPad;

  // printf("lc<%f> coff<%f> cin<%f> frq<%f>\n", lyrcents, centoff, cin, frq );
  // printf( "saw+ pad<%f>\n", pad );

  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1) {
    auto inputchan  = getInpBuf(dspbuf, 0);
    auto outputchan = getOutBuf(dspbuf, 0);
    for (int i = 0; i < inumframes; i++) {
      float input   = inputchan[i] * pad;
      float saw     = _pblep.getAndInc();
      float swplus  = input + (saw);
      outputchan[i] = swplus;
    }
  }
}
void SAWPLUS::doKeyOn(const DspKeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

SWPLUSSHP::SWPLUSSHP(dspblkdata_constptr_t dbd)
    : DspBlock(dbd)
    , _pblep(48000, PolyBLEP::RAMP) {
  _pblep.setAmplitude(1.0f);
}

void SWPLUSSHP::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);
  float lyrcents = _layer->_layerBasePitch;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);
  float SR       = _layer->_syn._sampleRate;
  _pblep.setFrequency(frq);
  float pad = _dbd->_inputPad;

  // printf( "_dbd->_inputPad<%f>\n", _dbd->_inputPad );

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float input     = ubuf[i] * pad;
      input           = softsat(input, 1);
      float saw       = _pblep.getAndInc();
      float xxx       = wrap(input + saw, -30.0f);
      float swplusshp = shaper(xxx, .25);
      ubuf[i]         = (swplusshp);
    }
}
void SWPLUSSHP::doKeyOn(const DspKeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

SHAPEMODOSC::SHAPEMODOSC(dspblkdata_constptr_t dbd)
    : DspBlock(dbd)
    , _pblep(48000, PolyBLEP::SINE) {
  _pblep.setAmplitude(1.0f);
}

void SHAPEMODOSC::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  float depth   = _param[1].eval(); //,0.01f,100.0f);

  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);
  float* lbuf    = getOutBuf(dspbuf, 1);
  float lyrcents = _layer->_layerBasePitch;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);
  float SR       = _layer->_syn._sampleRate;
  _pblep.setFrequency(frq);
  float pad = _dbd->_inputPad;

  // printf( "_dbd->_inputPad<%f>\n", _dbd->_inputPad );

  float depg = decibel_to_linear_amp_ratio(depth);

  const float kc1 = 1.0f / 128.0f;
  const float kc2 = 1.0f / 32;

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float inU = ubuf[i] * pad;
      float inL = lbuf[i] * pad;
      float inp = (inU)*depg;

      // First, the SINE value is multiplied by
      // the sample input value, then multiplied
      // by a constant—any samples exceeding full scale
      // will wrap around.

      float sine = _pblep.getAndInc();
      float xxx  = wrap(inp * sine * kc1, 1.0);

      // The result is added to the wrapped product
      // of the SINE value times a constant.

      float yyy = xxx + wrap(sine * kc2, 1.0);

      // The entire resulting waveform is then passed
      // through the SHAPER, whose Adjust value is
      // set by the level of the sample input.

      float swplusshp = shaper(yyy, inp);
      ubuf[i]         = (swplusshp);
    }

  _fval[0] = centoff;
  _fval[1] = depth;
}
void SHAPEMODOSC::doKeyOn(const DspKeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

PLUSSHAPEMODOSC::PLUSSHAPEMODOSC(dspblkdata_constptr_t dbd)
    : DspBlock(dbd)
    , _pblep(48000, PolyBLEP::SINE) {
  _pblep.setAmplitude(0.25f);
}

void PLUSSHAPEMODOSC::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  float depth   = _param[1].eval(); //,0.01f,100.0f);

  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);
  float* lbuf    = getOutBuf(dspbuf, 1);
  float lyrcents = _layer->_layerBasePitch;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);
  float SR       = _layer->_syn._sampleRate;
  _pblep.setFrequency(frq);
  float pad = _dbd->_inputPad;

  // printf( "_dbd->_inputPad<%f>\n", _dbd->_inputPad );

  float depg = decibel_to_linear_amp_ratio(depth);

  const float kc1 = 1.0f / 128.0f;
  const float kc2 = 1.0f / 32;

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float inU = ubuf[i] * pad;
      float inL = lbuf[i] * pad;
      float inp = (inU + inL) * depg;

      // First, the SINE value is multiplied by
      // the sample input value, then multiplied
      // by a constant—any samples exceeding full scale
      // will wrap around.

      float sine = _pblep.getAndInc();
      float xxx  = wrap(inp * sine * kc1, 1.0);

      // The result is added to the wrapped product
      // of the SINE value times a constant.

      float yyy = xxx + wrap(sine * kc2, 1.0);

      // The entire resulting waveform is then passed
      // through the SHAPER, whose Adjust value is
      // set by the level of the sample input.

      float swplusshp = shaper(yyy, inp);
      ubuf[i]         = (swplusshp);

      /*
      x SHAPE MOD OSC
      this function is
       similar to SHAPE MOD OSC, except that it multiplies
       its two input signals and uses that result as its input.

      + SHAPE MOD OSC

       + SHAPE MOD OSC is similar to x SHAPE MOD OSC,
       except that it adds its two input signals and uses
       that sum as its input.
*/
    }

  _fval[0] = centoff;
  _fval[1] = depth;
}
void PLUSSHAPEMODOSC::doKeyOn(const DspKeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

SYNCM::SYNCM(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}

void SYNCM::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);
  float lyrcents = _layer->_layerBasePitch;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);

  float SR  = _layer->_syn._sampleRate;
  _phaseInc = frq / SR;

  // printf( "_dbd->_inputPad<%f>\n", _dbd->_inputPad );

  if (1)
    for (int i = 0; i < inumframes; i++) {
      ubuf[i] = _phase;
      _phase  = fmod(_phase + _phaseInc, 1.0f);
    }
}
void SYNCM::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _phaseInc = 0.0f;
  _phase    = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

SYNCS::SYNCS(dspblkdata_constptr_t dbd)
    : DspBlock(dbd)
    , _pblep(48000, PolyBLEP::RAMP) {
  _pblep.setAmplitude(1.0f);
  _prvmaster = 0.0f;
}

void SYNCS::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);
  float lyrcents = _layer->_layerBasePitch;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);
  float SR       = _layer->_syn._sampleRate;
  _pblep.setFrequency(frq);
  float pad = _dbd->_inputPad;

  // printf( "_dbd->_inputPad<%f>\n", _dbd->_inputPad );

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float input = ubuf[i];

      bool do_sync = (input < _prvmaster);
      _prvmaster   = input;

      if (do_sync)
        _pblep.sync(0.0f);

      float saw = _pblep.getAndInc();
      ubuf[i]   = saw;
    }
}
void SYNCS::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _prvmaster = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

PWM::PWM(dspblkdata_constptr_t dbd)
    : DspBlock(dbd)
    , _pblep(48000, PolyBLEP::SINE) {
  _pblep.setAmplitude(1.0f);
}

void PWM::compute(DspBuffer& dspbuf) // final
{
  float offset = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]     = offset;

  int inumframes = dspbuf._numframes;
  float* ubuf    = getOutBuf(dspbuf, 0);
  float pad      = _dbd->_inputPad;

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float input = ubuf[i] * pad;
      ubuf[i]     = input + offset * 0.01f;
    }
}
void PWM::doKeyOn(const DspKeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

FM4::FM4(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
  _fm4 = new fm4syn;
}
void FM4::compute(DspBuffer& dspbuf) // final
{
  for (int i = 0; i < 4; i++) {
    _fval[i]        = _param[i].eval();
    _fm4->_opAmp[i] = _fval[i];
  }
  int inumframes = dspbuf._numframes;
  float* lbuf    = getOutBuf(dspbuf, 1);
  float* ubuf    = getOutBuf(dspbuf, 0);
  //_layer->_curPitchOffsetInCents = centoff;
  _fm4->compute(dspbuf);
}

void FM4::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _fm4->keyOn(koi);
  //_spOsc.keyOn(koi);
}
void FM4::doKeyOff() // final
{
  _fm4->keyOff();
}
void FM4::initBlock(dspblkdata_ptr_t blockdata) {
}

///////////////////////////////////////////////////////////////////////////////

NOISE::NOISE(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}
void NOISE::compute(DspBuffer& dspbuf) // final
{
  float centoff  = _param[0].eval();
  _fval[0]       = centoff;
  int inumframes = dspbuf._numframes;
  float* lbuf    = getOutBuf(dspbuf, 1);
  float* ubuf    = getOutBuf(dspbuf, 0);
  // _layer->_curPitchOffsetInCents = centoff;

  for (int i = 0; i < inumframes; i++) {
    float o = ((rand() & 0xffff) / 32768.0f) - 1.0f;
    ubuf[i] = o;
  }
}

void NOISE::doKeyOn(const DspKeyOnInfo& koi) // final
{
}
void NOISE::doKeyOff() // final
{
}
} // namespace ork::audio::singularity
