////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/modulation.h>

ImplementReflectionX(ork::audio::singularity::PITCH_DATA, "DspPitch");
ImplementReflectionX(ork::audio::singularity::SWPLUSSHP_DATA, "DspOscilSawAndShaper");
ImplementReflectionX(ork::audio::singularity::SAWPLUS_DATA, "DspOscilSawPlus");
ImplementReflectionX(ork::audio::singularity::SINE_DATA, "DspOscilSine");
ImplementReflectionX(ork::audio::singularity::SAW_DATA, "DspOscilSaw");
ImplementReflectionX(ork::audio::singularity::SQUARE_DATA, "DspOscilSquare");
ImplementReflectionX(ork::audio::singularity::SINEPLUS_DATA, "DspOscilSinePlus");
ImplementReflectionX(ork::audio::singularity::SHAPEMODOSC_DATA, "DspOscilShapeMod");
ImplementReflectionX(ork::audio::singularity::PLUSSHAPEMODOSC_DATA, "DspOscilShapeModPlus");
ImplementReflectionX(ork::audio::singularity::SYNCM_DATA, "DspOscilSyncModulator");
ImplementReflectionX(ork::audio::singularity::SYNCS_DATA, "DspOscilSyncCarrier");
ImplementReflectionX(ork::audio::singularity::PWM_DATA, "DspOscilPWM");
ImplementReflectionX(ork::audio::singularity::NOISE_DATA, "DspOscilNoise");

namespace ork::audio::singularity {

float shaper(float inp, float adj);
float wrap(float inp, float adj);

///////////////////////////////////////////////////////////////////////////////

void PITCH_DATA::describeX(class_t* clazz){}

PITCH_DATA::PITCH_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "PITCH";
  auto P = addParam("pitch");
  P->usePitchEvaluator();
  //P->_debug = true;
  //addParam("pch2")->usePitchEvaluator();
}

dspblkdata_ptr_t PITCH_DATA::clone() const{
  auto rval = std::make_shared<PITCH_DATA>(_name);
  size_t numparams = _paramd.size();
  rval->_paramd.clear();
  rval->_paramd.resize(numparams);
  for (size_t i=0; i<numparams; i++) {
    auto p = _paramd[i];
    auto p2 = p->clone();
    rval->_paramd[i] = p2;
    p->dump();
    p2->dump();
  }
  rval->_blocktype = _blocktype;
  rval->_numParams = _numParams;
  rval->_inputPad = _inputPad;

  rval->_blockIndex = _blockIndex;
  rval->_vars = _vars;
  rval->_bypass = _bypass;

  return rval;
}

dspblk_ptr_t PITCH_DATA::createInstance() const { // override
  return std::make_shared<PITCH>(this);
}

PITCH::PITCH(const DspBlockData* dbd)
    : DspBlock(dbd) {

}

void PITCH::compute(DspBuffer& dspbuf) // final
{
  auto LD = _layer->_layerdata;  
  auto PD = LD->_programdata;
  float portorate = PD->_portamento_rate;

  float value = _param[0].eval();
  _target = _layer->_layerBasePitch + value;
  if(portorate==0.0f){
    _current = _target;
  }
  else {
    float crate = controlRate();
    float elapsed = 1.0f / crate;
    // portorate in cents per second
    // get in ticks..
    float prate_this_tick = portorate*elapsed;
    float delta = (_target-_current);
    float delta_mag = abs(delta);
    float delta_sgn = (delta>=0.0f) ? 1.0f : -1.0f;
    float ir = 1.0f-portorate;
    if(delta_mag>prate_this_tick){
      delta_mag = prate_this_tick;
    }
    _current = _current+delta_mag*delta_sgn;
  }

  _layer->_curPitchOffsetInCents = value;
  _layer->_curPitchInCents = _current;
  //printf( "pch<%g>\n", value );
  
}
void PITCH::doKeyOn(const KeyOnInfo& koi) { // final

}

///////////////////////////////////////////////////////////////////////////////

void SINE_DATA::describeX(class_t* clazz){}

SINE_DATA::SINE_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "SINE";
  addParam("pitch")->usePitchEvaluator();
}

dspblk_ptr_t SINE_DATA::createInstance() const { // override
  return std::make_shared<SINE>(this);
}

SINE::SINE(const DspBlockData* dbd)
    : DspBlock(dbd)
    , _pblep(getSampleRate(), PolyBLEP::SINE) {
}

void SINE::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = _layer->_dspwritecount;
  //    float* ubuf = dspbuf.channel(0);
  float lyrcents = _layer->_layerBasePitch;
  float note      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(note);
  // printf("lc<%f> coff<%f> note<%f> frq<%f>\n", lyrcents, centoff, note, frq );
  float SR = synth::instance()->_sampleRate;
  _pblep.setFrequency(frq);

  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      outputchan[i] = _pblep.getAndInc();
    }
  }
}
void SINE::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void SAW_DATA::describeX(class_t* clazz){}

SAW_DATA::SAW_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "SAW";
  addParam("pitch")->usePitchEvaluator();
}

dspblk_ptr_t SAW_DATA::createInstance() const { // override
  return std::make_shared<SAW>(this);
}

SAW::SAW(const DspBlockData* dbd)
    : DspBlock(dbd)
    , _pblep(getSampleRate(), PolyBLEP::RAMP) {
}

void SAW::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = _layer->_dspwritecount;
  // float* ubuf = dspbuf.channel(0);
  float lyrcents = _layer->_layerBasePitch;
  float note      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(note);
  float SR       = synth::instance()->_sampleRate;
  _pblep.setFrequency(frq);

   //printf( "centoff<%g> frq<%f>\n", centoff, frq );
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      outputchan[i] = _pblep.getAndInc();
    }
  }
}
void SAW::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void SQUARE_DATA::describeX(class_t* clazz){}

SQUARE_DATA::SQUARE_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "SQUARE";
  addParam("pitch")->usePitchEvaluator();
}

dspblk_ptr_t SQUARE_DATA::createInstance() const { // override
  return std::make_shared<SQUARE>(this);
}

SQUARE::SQUARE(const DspBlockData* dbd)
    : DspBlock(dbd)
    , _pblep(getSampleRate(), PolyBLEP::SQUARE) {
}

void SQUARE::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = _layer->_dspwritecount;
  // float* ubuf = dspbuf.channel(0);
  float lyrcents = _layer->_layerBasePitch;
  float note      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(note);
  float SR       = synth::instance()->_sampleRate;
  _pblep.setFrequency(frq);

  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1) {
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      outputchan[i] = _pblep.getAndInc();
    }
  }
}
void SQUARE::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void SINEPLUS_DATA::describeX(class_t* clazz){}

SINEPLUS_DATA::SINEPLUS_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "SINEPLUS";
  addParam("pitch")->usePitchEvaluator();
}

dspblk_ptr_t SINEPLUS_DATA::createInstance() const { // override
  return std::make_shared<SINEPLUS>(this);
}

SINEPLUS::SINEPLUS(const DspBlockData* dbd)
    : DspBlock(dbd)
    , _pblep(getSampleRate(), PolyBLEP::SINE) {
}

void SINEPLUS::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float lyrcents = _layer->_layerBasePitch;
  float note      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(note);
  float SR       = synth::instance()->_sampleRate;
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
void SINEPLUS::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void SAWPLUS_DATA::describeX(class_t* clazz){}

SAWPLUS_DATA::SAWPLUS_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "SAWPLUS";
  addParam("pitch")->usePitchEvaluator();
}

dspblk_ptr_t SAWPLUS_DATA::createInstance() const { // override
  return std::make_shared<SAWPLUS>(this);
}

SAWPLUS::SAWPLUS(const DspBlockData* dbd)
    : DspBlock(dbd)
    , _pblep(getSampleRate(), PolyBLEP::SAWTOOTH) {
}

void SAWPLUS::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = _layer->_dspwritecount;
  float lyrcents = _layer->_layerBasePitch;
  float note      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(note);
  float SR       = synth::instance()->_sampleRate;
  _pblep.setFrequency(frq);
  float pad = _dbd->_inputPad;

  // printf("lc<%f> coff<%f> note<%f> frq<%f>\n", lyrcents, centoff, note, frq );
  // printf( "saw+ pad<%f>\n", pad );

  // printf( "frq<%f> _phaseInc<%lld>\n", frq, _phaseInc );
  if (1) {
    auto inputchan  = getInpBuf(dspbuf, 0);
    auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      float input   = inputchan[i] * pad;
      float saw     = _pblep.getAndInc();
      float swplus  = input + (saw);
      outputchan[i] = swplus;
    }
  }
}
void SAWPLUS::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void SWPLUSSHP_DATA::describeX(class_t* clazz){}

SWPLUSSHP_DATA::SWPLUSSHP_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "SWPLUSSHP";
  addParam("pitch")->usePitchEvaluator();
}

dspblk_ptr_t SWPLUSSHP_DATA::createInstance() const { // override
  return std::make_shared<SWPLUSSHP>(this);
}

SWPLUSSHP::SWPLUSSHP(const DspBlockData* dbd)
    : DspBlock(dbd)
    , _pblep(getSampleRate(), PolyBLEP::RAMP) {
  _pblep.setAmplitude(1.0f);
}

void SWPLUSSHP::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float lyrcents = _layer->_layerBasePitch;
  float note      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(note);
  float SR       = synth::instance()->_sampleRate;
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
void SWPLUSSHP::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void SHAPEMODOSC_DATA::describeX(class_t* clazz){}

SHAPEMODOSC_DATA::SHAPEMODOSC_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "SHAPEMODOSC";
  addParam("pitch")->usePitchEvaluator();
  addParam("depth")->useDefaultEvaluator();
}

dspblk_ptr_t SHAPEMODOSC_DATA::createInstance() const { // override
  return std::make_shared<SHAPEMODOSC>(this);
}

SHAPEMODOSC::SHAPEMODOSC(const DspBlockData* dbd)
    : DspBlock(dbd)
    , _pblep(getSampleRate(), PolyBLEP::SINE) {
  _pblep.setAmplitude(1.0f);
}

void SHAPEMODOSC::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  float depth   = _param[1].eval(); //,0.01f,100.0f);

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;
  float lyrcents = _layer->_layerBasePitch;
  float note      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(note);
  float SR       = synth::instance()->_sampleRate;
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
void SHAPEMODOSC::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void PLUSSHAPEMODOSC_DATA::describeX(class_t* clazz){}

PLUSSHAPEMODOSC_DATA::PLUSSHAPEMODOSC_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "PLUSSHAPEMODOSC";
  addParam("pitch")->usePitchEvaluator();
  addParam("depth")->useDefaultEvaluator();
}

dspblk_ptr_t PLUSSHAPEMODOSC_DATA::createInstance() const { // override
  return std::make_shared<PLUSSHAPEMODOSC>(this);
}

PLUSSHAPEMODOSC::PLUSSHAPEMODOSC(const DspBlockData* dbd)
    : DspBlock(dbd)
    , _pblep(getSampleRate(), PolyBLEP::SINE) {
  _pblep.setAmplitude(0.25f);
}

void PLUSSHAPEMODOSC::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  float depth   = _param[1].eval(); //,0.01f,100.0f);

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;
  float lyrcents = _layer->_layerBasePitch;
  float note      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(note);
  float SR       = synth::instance()->_sampleRate;
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
void PLUSSHAPEMODOSC::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void SYNCM_DATA::describeX(class_t* clazz){}

SYNCM_DATA::SYNCM_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "SYNCM";
  addParam("pitch")->usePitchEvaluator();
}

dspblk_ptr_t SYNCM_DATA::createInstance() const { // override
  return std::make_shared<SYNCM>(this);
}

SYNCM::SYNCM(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void SYNCM::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float lyrcents = _layer->_layerBasePitch;
  float note      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(note);

  float SR  = synth::instance()->_sampleRate;
  _phaseInc = frq / SR;

  // printf( "_dbd->_inputPad<%f>\n", _dbd->_inputPad );

  if (1)
    for (int i = 0; i < inumframes; i++) {
      ubuf[i] = _phase;
      _phase  = fmod(_phase + _phaseInc, 1.0f);
    }
}
void SYNCM::doKeyOn(const KeyOnInfo& koi) // final
{
  _phaseInc = 0.0f;
  _phase    = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void SYNCS_DATA::describeX(class_t* clazz){}

SYNCS_DATA::SYNCS_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "SYNCS";
  addParam("pitch")->usePitchEvaluator();
}

dspblk_ptr_t SYNCS_DATA::createInstance() const { // override
  return std::make_shared<SYNCS>(this);
}

SYNCS::SYNCS(const DspBlockData* dbd)
    : DspBlock(dbd)
    , _pblep(getSampleRate(), PolyBLEP::RAMP) {
  _pblep.setAmplitude(1.0f);
  _prvmaster = 0.0f;
}

void SYNCS::compute(DspBuffer& dspbuf) // final
{
  float centoff = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]      = centoff;

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float lyrcents = _layer->_layerBasePitch;
  float note      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(note);
  float SR       = synth::instance()->_sampleRate;
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
void SYNCS::doKeyOn(const KeyOnInfo& koi) // final
{
  _prvmaster = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void PWM_DATA::describeX(class_t* clazz){}

PWM_DATA::PWM_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "PWM";
  addParam("offset")->useDefaultEvaluator();
}

dspblk_ptr_t PWM_DATA::createInstance() const { // override
  return std::make_shared<PWM>(this);
}

PWM::PWM(const DspBlockData* dbd)
    : DspBlock(dbd)
    , _pblep(getSampleRate(), PolyBLEP::SINE) {
  _pblep.setAmplitude(1.0f);
}

void PWM::compute(DspBuffer& dspbuf) // final
{
  float offset = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]     = offset;

  int inumframes = _layer->_dspwritecount;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  float pad      = _dbd->_inputPad;

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float input = ubuf[i] * pad;
      ubuf[i]     = input + offset * 0.01f;
    }
}
void PWM::doKeyOn(const KeyOnInfo& koi) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void NOISE_DATA::describeX(class_t* clazz){}

NOISE_DATA::NOISE_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "NOISE";
  addParam("pitch")->usePitchEvaluator();
}

dspblk_ptr_t NOISE_DATA::createInstance() const { // override
  return std::make_shared<NOISE>(this);
}

NOISE::NOISE(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
void NOISE::compute(DspBuffer& dspbuf) // final
{
  float centoff  = _param[0].eval();
  _fval[0]       = centoff;
  int inumframes = _layer->_dspwritecount;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  // _layer->_curPitchOffsetInCents = centoff;

  for (int i = 0; i < inumframes; i++) {
    float o = ((rand() & 0xffff) / 32768.0f) - 1.0f;
    ubuf[i] = o;
  }
}

void NOISE::doKeyOn(const KeyOnInfo& koi) // final
{
}
void NOISE::doKeyOff() // final
{
}
} // namespace ork::audio::singularity
