////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <math.h>
#include <assert.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/fmosc.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/reflect/properties/registerX.inl>
//#include <ork/reflect/properties/DirectTyped.hpp>

ImplementReflectionX(ork::audio::singularity::PMXData, "SynPMX");
ImplementReflectionX(ork::audio::singularity::PMXMixData, "SynPMXMixer");

////////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
////////////////////////////////////////////////////////////////////////////////
// the YM3812 avoids multiplications by operating on log-transformed
// x*y = e^(ln(x)+ln(y))
// 256-word ROM stores the exponential function as a
// lookup table, used to convert the logarithmic scale
// signal back to linear scale when required
inline float proc_out(float inp) {
  constexpr float kclamp = 8.0f;
  constexpr float kscale = 0.125f;
  validateSample(inp);
  if (isfinite(inp) and not isnan(inp)) {
    return clip_float(inp, -kclamp, kclamp) * kscale;
  }
  return 0.0f;
}
///////////////////////////////////////////////////////////////////////////////
void PMXData::describeX(class_t* clazz) {

  clazz->directProperty("Feedback", &PMXData::_feedback);
  clazz->directProperty("ModIndex", &PMXData::_modIndex);
  clazz->directProperty("InputChannel", &PMXData::_inpchannel);
  clazz->directVectorProperty("PmInputChannels", &PMXData::_pmInpChannels);

  clazz->lambdaProperty<PMXData, int>(
      "Waveform", //
      [](const PMXData* obj_inp, int& valout) { valout = obj_inp->_pmoscdata._waveform; },
      [](PMXData* obj_out, const int& valinp) { obj_out->_pmoscdata._waveform = valinp; });
}
///////////////////////////////////////////////////////////////////////////////
PMXData::PMXData(std::string name)
    : DspBlockData(name) {

  auto pitch = addParam("pitch");
  pitch->usePitchEvaluator(); // pitch
  auto amp = addParam("amp");
  amp->useDefaultEvaluator(); // amp
  amp->_units = "0-1";
  auto fbl    = addParam("feedback");
  fbl->_units = "0-1";
  fbl->useDefaultEvaluator(); // feedback
}
///////////////////////////////////////////////////////////////////////////////
dspblk_ptr_t PMXData::createInstance() const {
  return std::make_shared<PMX>(this);
}
///////////////////////////////////////////////////////////////////////////////
void PMXData::addPmInput(int dspchannel) {
  _pmInpChannels.push_back(dspchannel);
}
///////////////////////////////////////////////////////////////////////////////
PMX::PMX(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void PMX::compute(DspBuffer& dspbuf) { // final
  int inumframes   = _layer->_dspwritecount;
  float* output    = dspbuf.channel(_dspchannel[0]) + _layer->_dspwritebase;
  float pitch_cents  = _param[0].eval();
  float amp        = _param[1].eval();
  float fbl        = _param[2].eval();
  float note      = (pitch_cents) * 0.01;
  float frq        = midi_note_to_frequency(note);
  
  float clampedamp = std::clamp(amp, 0.0f, 1.0f);
  float clampefbl  = std::clamp(fbl, 0.0f, 1.0f);
  ///////////////////////////////////////////////////////////////
  //printf("frq<%g> amp<%g> fbl<%g>\n", frq, amp, fbl);
  ///////////////////////////////////////////////////////////////
  const float* modinputs[PMXData::kmaxmodulators] = {
      nullptr,
      nullptr,
      nullptr,
      nullptr, //
      nullptr,
      nullptr,
      nullptr,
      nullptr};
  int m = 0;
  for (int inpchi : _pmxdata->_pmInpChannels) {
    modinputs[m] = dspbuf.channel(inpchi) //
                   + _layer->_dspwritebase;
    m++;
  }
  ///////////////////////////////////////////////////////////////
  for (int i = 0; i < inumframes; i++) {
    float famp         = lerp(_amp, amp, float(i) * kfpc);
    float ffrq         = lerp(_frq, frq, float(i) * kfpc);
    float phase_offset = _pmosc._prevOutput * fbl * famp;
    for (int m = 0; m < PMXData::kmaxmodulators; m++) {
      auto modinp = modinputs[m];
      if (modinp != nullptr) {
        phase_offset += modinp[i];
      }
    }
    float osc_out = _pmosc.compute(ffrq, phase_offset * _modIndex);
    output[i]     = proc_out(osc_out * famp);
  }
  ///////////////////////////////////////////////////////////////
  _amp = amp;
  _frq = frq;
}
///////////////////////////////////////////////////////////////////////////////
void PMX::doKeyOn(const KeyOnInfo& koi) { // final
  _pmxdata = (const PMXData*)_dbd;
  _pmosc.keyOn(_pmxdata->_pmoscdata);
  _modIndex  = _pmxdata->_modIndex;
  _modulator = _pmxdata->_modulator;
  if (_pmxdata->_txprogramdata) {
    auto name = _pmxdata->_txprogramdata->_name;
    int alg   = _pmxdata->_txprogramdata->_alg;
     //printf("keyon prog<%s> alg<%d>\n", name.c_str(), alg);
  }
}
///////////////////////////////////////////////////////////////////////////////
void PMX::doKeyOff() { // final
  _pmosc.keyOff();
}
///////////////////////////////////////////////////////////////////////////////
void PMXMixData::describeX(class_t* clazz) {
  clazz->directVectorProperty("InputChannels", &PMXMixData::_pmixInpChannels);
}
///////////////////////////////////////////////////////////////////////////////
PMXMixData::PMXMixData(std::string name)
    : DspBlockData(name) {
}
///////////////////////////////////////////////////////////////////////////////
dspblk_ptr_t PMXMixData::createInstance() const {
  return std::make_shared<PMXMix>(this);
}
void PMXMixData::addInputChannel(int chan) {
  _pmixInpChannels.push_back(chan);
}
///////////////////////////////////////////////////////////////////////////////
PMXMix::PMXMix(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void PMXMix::compute(DspBuffer& dspbuf) { // final
  int inumframes = _layer->_dspwritecount;
  float* output  = dspbuf.channel(_dspchannel[0]) + _layer->_dspwritebase;
  ///////////////////////////////////////////////////////////////
  // mix in each PM carrier oscillator
  ///////////////////////////////////////////////////////////////
  for (int inpchi : _pmixdata->_pmixInpChannels) {
    auto input = dspbuf.channel(inpchi) + _layer->_dspwritebase;
    for (int i = 0; i < inumframes; i++) {
      output[i] += input[i] * _finalamp;
    }
  }
  ///////////////////////////////////////////////////////////////
  for (int i = 0; i < inumframes; i++) {
    output[i] = proc_out(output[i]);
  }
}
///////////////////////////////////////////////////////////////////////////////
void PMXMix::doKeyOn(const KeyOnInfo& koi) { // final
  _pmixdata      = (const PMXMixData*)_dbd;
  int numoutputs = _pmixdata->_pmixInpChannels.size();
  _finalamp      = (numoutputs == 0) //
                  ? 1.0f
                  : 1.0f / float(numoutputs);
  validateSample(_finalamp);
}
///////////////////////////////////////////////////////////////////////////////
void PMXMix::doKeyOff() { // final
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
