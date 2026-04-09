////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/lev2/aud/singularity/alg_filters.h>
#include <ork/lev2/aud/singularity/alg_eq.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendStereoChorus(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  /////////////////
  // stereo chorus
  /////////////////
  auto chorus                = stage->appendTypedBlock<StereoDynamicEcho>("echo");
  chorus->param(0)->_coarse  = 0.0f; // delay time (L)
  chorus->param(1)->_coarse  = 0.0f; // delay time (R)
  chorus->param(2)->_coarse  = 0.25; // feedback
  chorus->param(3)->_coarse  = 0.5;  // wet/dry mix
  auto delaytime_modL        = chorus->param(0)->_mods;
  auto delaytime_modR        = chorus->param(1)->_mods;
  auto DELAYTIMEMODL         = layer->appendController<CustomControllerData>("DELAYTIMEL");
  auto DELAYTIMEMODR         = layer->appendController<CustomControllerData>("DELAYTIMER");
  delaytime_modL->_src1      = DELAYTIMEMODL;
  delaytime_modL->_src1Scale = 1.0;
  DELAYTIMEMODL->_oncompute  = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( 0.010f + sinf(time * pi2 * .14) * 0.001f);
  };
  delaytime_modR->_src1      = DELAYTIMEMODR;
  delaytime_modR->_src1Scale = 1.0;
  DELAYTIMEMODR->_oncompute  = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( 0.005f + sinf(time * pi2 * 0.09) * 0.0077f);
  };
  /////////////////
  return chorus;
}
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendMildStereoChorus(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  /////////////////
  // stereo chorus
  /////////////////
  auto chorus                = stage->appendTypedBlock<StereoDynamicEcho>("echo");
  chorus->param(0)->_coarse  = 0.0f; // delay time (L)
  chorus->param(1)->_coarse  = 0.0f; // delay time (R)
  chorus->param(2)->_coarse  = 0.15; // feedback
  chorus->param(3)->_coarse  = 0.35;  // wet/dry mix
  auto delaytime_modL        = chorus->param(0)->_mods;
  auto delaytime_modR        = chorus->param(1)->_mods;
  auto DELAYTIMEMODL         = layer->appendController<CustomControllerData>("DELAYTIMEL");
  auto DELAYTIMEMODR         = layer->appendController<CustomControllerData>("DELAYTIMER");
  delaytime_modL->_src1      = DELAYTIMEMODL;
  delaytime_modL->_src1Scale = 1.0;
  DELAYTIMEMODL->_oncompute  = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( 0.010f + sinf(time * pi2 * .14) * 0.001f);
  };
  delaytime_modR->_src1      = DELAYTIMEMODR;
  delaytime_modR->_src1Scale = 1.0;
  DELAYTIMEMODR->_oncompute  = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( 0.005f + sinf(time * pi2 * 0.09) * 0.0077f);
  };
  /////////////////
  return chorus;
}
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendPitchShifter(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  auto shifter               = stage->appendTypedBlock<PitchShifter>("shifter");
  shifter->param(0)->_coarse = 0.5f; // wet/dry mix
  return shifter;
}
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendRecursivePitchShifter(lyrdata_ptr_t layer, dspstagedata_ptr_t stage,float feedback) {
  auto shifter               = stage->appendTypedBlock<RecursivePitchShifter>("shifter-recursive",feedback);
  shifter->param(0)->_coarse = 0.5f; // wet/dry mix
  return shifter;
}
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<Fdn4ReverbData> appendStereoReverb(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  auto fdn4               = stage->appendTypedBlock<Fdn4Reverb>("reverb");
  fdn4->param(0)->_coarse = 0.5f; // wet/dry mix
  fdn4->_input_gain = 0.5;
  fdn4->_output_gain = 1.35;
  fdn4->_time_base = 0.007;
  fdn4->_time_scale = 0.071;
  fdn4->_matrix_gain = 0.498;
  fdn4->_hipass_cutoff = 200.0;
  fdn4->_allpass_shift_frq_bas = 700.0;
  fdn4->_allpass_shift_frq_mul = 1.5;
  fdn4->_allpass_count = 4;
  fdn4->matrixHouseholder(fdn4->_matrix_gain);
  fdn4->update();
  return fdn4;
}
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<Fdn4ReverbData> appendOilTankReverb(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  auto fdn4               = stage->appendTypedBlock<Fdn4Reverb>("reverb");
  fdn4->param(0)->_coarse = 0.5f; // wet/dry mix
  fdn4->_input_gain = 0.5;
  fdn4->_output_gain = 1.6;
  fdn4->_time_base = 0.007;
  fdn4->_time_scale = 0.071;
  fdn4->_matrix_gain = 0.48;
  fdn4->_hipass_cutoff = 200.0;
  fdn4->_allpass_shift_frq_bas = 60.0;
  fdn4->_allpass_shift_frq_mul = 1.1;
  fdn4->_allpass_count = 128;
  fdn4->matrixHouseholder(fdn4->_matrix_gain);
  fdn4->update();
  return fdn4;
}
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<Fdn4ReverbData> appendGuyWireReverb(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  auto fdn4               = stage->appendTypedBlock<Fdn4Reverb>("reverb");
  fdn4->param(0)->_coarse = 0.5f; // wet/dry mix
  fdn4->_input_gain = 0.5;
  fdn4->_output_gain = 1.35;
  fdn4->_time_base = 0.37;
  fdn4->_time_scale = 0.7;
  fdn4->_matrix_gain = 0.498;
  fdn4->_hipass_cutoff = 200.0;
  fdn4->_allpass_shift_frq_bas = 60.0;
  fdn4->_allpass_shift_frq_mul = 1.1;
  fdn4->_allpass_count = 128;
  fdn4->matrixHouseholder(fdn4->_matrix_gain);
  fdn4->update();
  return fdn4;
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoDistortion(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float adj) {
  auto l               = stage->appendTypedBlock<Distortion>("distorion-L");
  auto r               = stage->appendTypedBlock<Distortion>("distorion-R");
  l->param(0)->_coarse = adj;
  r->param(0)->_coarse = adj;
  l->addDspChannel(0);
  r->addDspChannel(1);
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoShaper(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float adj) {
  auto l               = stage->appendTypedBlock<SHAPER>("distorion-L");
  auto r               = stage->appendTypedBlock<SHAPER>("distorion-R");
  l->param(0)->_coarse = adj;
  r->param(0)->_coarse = adj;
  l->addDspChannel(0);
  r->addDspChannel(1);
}
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendStereoStereoDynamicEcho(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float dtL,
    float dtR,
    float feedback,
    float wetness) {
  auto echo               = stage->appendTypedBlock<StereoDynamicEcho>("echo");
  echo->param(0)->_coarse = dtL;
  echo->param(1)->_coarse = dtR;
  echo->param(2)->_coarse = feedback;
  echo->param(3)->_coarse = wetness;
  echo->addDspChannel(0);
  echo->addDspChannel(1);
  return echo;
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoParaEQ(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc,
    float w,
    float gain) {
  auto eql               = stage->appendTypedBlock<ParametricEq>("peq-L");
  auto eqr               = stage->appendTypedBlock<ParametricEq>("peq-R");
  eql->param(0)->_coarse = fc;
  eql->param(1)->_coarse = w;
  eql->param(2)->_coarse = gain;
  eqr->param(0)->_coarse = fc;
  eqr->param(1)->_coarse = w;
  eqr->param(2)->_coarse = gain;
  eql->addDspChannel(0);
  eqr->addDspChannel(1);
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoHighPass(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc) {
  auto eql               = stage->appendTypedBlock<HighPass>("Highpass-L");
  auto eqr               = stage->appendTypedBlock<HighPass>("Highpass-R");
  eql->param(0)->_coarse = fc;
  eqr->param(0)->_coarse = fc;
  eql->addDspChannel(0);
  eqr->addDspChannel(1);
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoHighFreqStimulator(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc,
    float drive,
    float amp) {
  auto eql               = stage->appendTypedBlock<HighFreqStimulator>("Stim-L");
  auto eqr               = stage->appendTypedBlock<HighFreqStimulator>("Stim-R");
  eql->param(0)->_coarse = fc;
  eqr->param(0)->_coarse = fc;
  eql->param(1)->_coarse = drive;
  eqr->param(1)->_coarse = drive;
  eql->param(2)->_coarse = amp;
  eqr->param(2)->_coarse = amp;
  eql->addDspChannel(0);
  eqr->addDspChannel(1);
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoLowPass(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc) {
  auto eql               = stage->appendTypedBlock<TwoPoleLowPass>("Lowpass-L");
  auto eqr               = stage->appendTypedBlock<TwoPoleLowPass>("Lowpass-R");
  eql->param(0)->_coarse = fc;
  eqr->param(0)->_coarse = fc;
  eql->addDspChannel(0);
  eqr->addDspChannel(1);
}
///////////////////////////////////////////////////////////////////////////////
// Professional quality chorus with:
// - Multiple delay voices per channel (3L + 3R)
// - HPF to remove low rumble
// - LPF to smooth wet signal
// - Slow LFO modulation with phase offsets
// - Cross-feedback for richness
///////////////////////////////////////////////////////////////////////////////
void appendProChorus(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float wetness) {
  /////////////////
  // HPF to remove low rumble from wet signal (~80Hz)
  /////////////////
  appendStereoHighPass(layer, stage, 80.0f);

  /////////////////
  // Voice 1: Left channel, slow LFO
  /////////////////
  auto chorus1               = stage->appendTypedBlock<StereoDynamicEcho>("chorus-v1");
  chorus1->param(0)->_coarse = 0.0f;     // base delay L
  chorus1->param(1)->_coarse = 0.0f;     // base delay R
  chorus1->param(2)->_coarse = 0.15f;    // feedback
  chorus1->param(3)->_coarse = wetness * 0.4f;  // wet mix (scaled)

  auto dt1modL        = chorus1->param(0)->_mods;
  auto dt1modR        = chorus1->param(1)->_mods;
  auto DT1MODL        = layer->appendController<CustomControllerData>("PROCHO1L");
  auto DT1MODR        = layer->appendController<CustomControllerData>("PROCHO1R");
  dt1modL->_src1      = DT1MODL;
  dt1modL->_src1Scale = 1.0;
  dt1modR->_src1      = DT1MODR;
  dt1modR->_src1Scale = 1.0;

  // Voice 1: slow, deep modulation (primary chorus character)
  DT1MODL->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Base delay ~7ms, modulation +-3ms, rate 0.5Hz
    cci->setFloatValue(0.007f + sinf(time * pi2 * 0.5f) * 0.003f);
  };
  DT1MODR->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Phase offset of ~120 degrees for stereo spread
    cci->setFloatValue(0.009f + sinf(time * pi2 * 0.5f + 2.094f) * 0.003f);
  };

  /////////////////
  // Voice 2: medium LFO, different rate
  /////////////////
  auto chorus2               = stage->appendTypedBlock<StereoDynamicEcho>("chorus-v2");
  chorus2->param(0)->_coarse = 0.0f;
  chorus2->param(1)->_coarse = 0.0f;
  chorus2->param(2)->_coarse = 0.1f;     // less feedback
  chorus2->param(3)->_coarse = wetness * 0.35f;

  auto dt2modL        = chorus2->param(0)->_mods;
  auto dt2modR        = chorus2->param(1)->_mods;
  auto DT2MODL        = layer->appendController<CustomControllerData>("PROCHO2L");
  auto DT2MODR        = layer->appendController<CustomControllerData>("PROCHO2R");
  dt2modL->_src1      = DT2MODL;
  dt2modL->_src1Scale = 1.0;
  dt2modR->_src1      = DT2MODR;
  dt2modR->_src1Scale = 1.0;

  // Voice 2: slightly faster, different phase
  DT2MODL->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Base delay ~12ms, modulation +-2.5ms, rate 0.7Hz
    cci->setFloatValue(0.012f + sinf(time * pi2 * 0.7f + 1.0f) * 0.0025f);
  };
  DT2MODR->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Phase offset for stereo, different base delay
    cci->setFloatValue(0.015f + sinf(time * pi2 * 0.7f + 3.1f) * 0.0025f);
  };

  /////////////////
  // Voice 3: slowest, widest for depth
  /////////////////
  auto chorus3               = stage->appendTypedBlock<StereoDynamicEcho>("chorus-v3");
  chorus3->param(0)->_coarse = 0.0f;
  chorus3->param(1)->_coarse = 0.0f;
  chorus3->param(2)->_coarse = 0.08f;    // minimal feedback
  chorus3->param(3)->_coarse = wetness * 0.25f;

  auto dt3modL        = chorus3->param(0)->_mods;
  auto dt3modR        = chorus3->param(1)->_mods;
  auto DT3MODL        = layer->appendController<CustomControllerData>("PROCHO3L");
  auto DT3MODR        = layer->appendController<CustomControllerData>("PROCHO3R");
  dt3modL->_src1      = DT3MODL;
  dt3modL->_src1Scale = 1.0;
  dt3modR->_src1      = DT3MODR;
  dt3modR->_src1Scale = 1.0;

  // Voice 3: slowest, deepest for richness
  DT3MODL->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Base delay ~20ms, modulation +-4ms, rate 0.3Hz
    cci->setFloatValue(0.020f + sinf(time * pi2 * 0.3f + 0.5f) * 0.004f);
  };
  DT3MODR->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Opposite phase for maximum stereo width
    cci->setFloatValue(0.018f + sinf(time * pi2 * 0.3f + 3.64f) * 0.004f);
  };

  /////////////////
  // LPF to smooth wet signal (~8kHz) - emulates BBD coloration
  /////////////////
  appendStereoLowPass(layer, stage, 8000.0f);
}
///////////////////////////////////////////////////////////////////////////////
// Professional quality flanger with:
// - Short delay times (1-8ms)
// - High feedback for metallic/jet character
// - Deep modulation for dramatic sweep
// - Multiple voices for thickness
// - Cross-channel feedback for stereo richness
///////////////////////////////////////////////////////////////////////////////
void appendProFlanger(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float wetness,
    float feedback) {
  /////////////////
  // HPF to remove DC offset and low rumble
  /////////////////
  appendStereoHighPass(layer, stage, 40.0f);

  /////////////////
  // Voice 1: Primary flanger - slow deep sweep
  /////////////////
  auto flange1               = stage->appendTypedBlock<StereoDynamicEcho>("flange-v1");
  flange1->param(0)->_coarse = 0.0f;     // base delay L
  flange1->param(1)->_coarse = 0.0f;     // base delay R
  flange1->param(2)->_coarse = feedback; // high feedback for jet sound
  flange1->param(3)->_coarse = wetness * 0.6f;

  auto ft1modL        = flange1->param(0)->_mods;
  auto ft1modR        = flange1->param(1)->_mods;
  auto FT1MODL        = layer->appendController<CustomControllerData>("PROFLG1L");
  auto FT1MODR        = layer->appendController<CustomControllerData>("PROFLG1R");
  ft1modL->_src1      = FT1MODL;
  ft1modL->_src1Scale = 1.0;
  ft1modR->_src1      = FT1MODR;
  ft1modR->_src1Scale = 1.0;

  // Voice 1: slow sweep, short delays (classic flanger)
  FT1MODL->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Base delay ~2ms, modulation sweeps 0.5-4ms, rate 0.15Hz (slow jet sweep)
    float lfo = 0.5f * (1.0f + sinf(time * pi2 * 0.15f));
    cci->setFloatValue(0.0005f + lfo * 0.0035f);
  };
  FT1MODR->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Slight phase offset for stereo movement
    float lfo = 0.5f * (1.0f + sinf(time * pi2 * 0.15f + 0.3f));
    cci->setFloatValue(0.0005f + lfo * 0.0035f);
  };

  /////////////////
  // Voice 2: Slightly faster, adds thickness
  /////////////////
  auto flange2               = stage->appendTypedBlock<StereoDynamicEcho>("flange-v2");
  flange2->param(0)->_coarse = 0.0f;
  flange2->param(1)->_coarse = 0.0f;
  flange2->param(2)->_coarse = feedback * 0.7f;  // slightly less feedback
  flange2->param(3)->_coarse = wetness * 0.35f;

  auto ft2modL        = flange2->param(0)->_mods;
  auto ft2modR        = flange2->param(1)->_mods;
  auto FT2MODL        = layer->appendController<CustomControllerData>("PROFLG2L");
  auto FT2MODR        = layer->appendController<CustomControllerData>("PROFLG2R");
  ft2modL->_src1      = FT2MODL;
  ft2modL->_src1Scale = 1.0;
  ft2modR->_src1      = FT2MODR;
  ft2modR->_src1Scale = 1.0;

  // Voice 2: different rate for complex movement
  FT2MODL->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Slightly different range and rate (0.23Hz)
    float lfo = 0.5f * (1.0f + sinf(time * pi2 * 0.23f + 1.5f));
    cci->setFloatValue(0.001f + lfo * 0.003f);
  };
  FT2MODR->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Counter-phase for stereo width
    float lfo = 0.5f * (1.0f + sinf(time * pi2 * 0.23f + 4.7f));
    cci->setFloatValue(0.001f + lfo * 0.003f);
  };

  /////////////////
  // Voice 3: Cross-channel feedback voice for richness
  /////////////////
  auto flange3               = stage->appendTypedBlock<StereoDynamicEcho>("flange-v3");
  flange3->param(0)->_coarse = 0.0f;
  flange3->param(1)->_coarse = 0.0f;
  flange3->param(2)->_coarse = feedback * 0.5f;
  flange3->param(3)->_coarse = wetness * 0.2f;

  auto ft3modL        = flange3->param(0)->_mods;
  auto ft3modR        = flange3->param(1)->_mods;
  auto FT3MODL        = layer->appendController<CustomControllerData>("PROFLG3L");
  auto FT3MODR        = layer->appendController<CustomControllerData>("PROFLG3R");
  ft3modL->_src1      = FT3MODL;
  ft3modL->_src1Scale = 1.0;
  ft3modR->_src1      = FT3MODR;
  ft3modR->_src1Scale = 1.0;

  // Voice 3: slowest, adds depth
  FT3MODL->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Very slow sweep (0.07Hz) for evolving character
    float lfo = 0.5f * (1.0f + sinf(time * pi2 * 0.07f));
    cci->setFloatValue(0.002f + lfo * 0.005f);
  };
  FT3MODR->_oncompute = [](CustomControllerInst* cci) {
    float time = cci->_layer->_layerTime;
    // Opposite phase for maximum stereo effect
    float lfo = 0.5f * (1.0f + sinf(time * pi2 * 0.07f + 3.14159f));
    cci->setFloatValue(0.002f + lfo * 0.005f);
  };

  /////////////////
  // Gentle LPF to tame harshness from high feedback
  /////////////////
  appendStereoLowPass(layer, stage, 12000.0f);
}
///////////////////////////////////////////////////////////////////////////////
// Professional quality amp simulation with:
// - Multi-stage gain structure (preamp + power amp character)
// - Pre-EQ tone shaping before distortion
// - Post-EQ tone stack (bass/mid/treble)
// - Cabinet simulation (speaker rolloff + resonance)
// - Presence control for attack/definition
///////////////////////////////////////////////////////////////////////////////
void appendProDistortion(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float drive,      // 0-1: clean to heavy distortion
    float bass,       // dB: bass shelf adjustment
    float mid,        // dB: mid parametric adjustment
    float treble,     // dB: treble shelf adjustment
    float presence) { // dB: presence/attack control

  /////////////////
  // Stage 1: Input conditioning
  // HPF to remove DC and subsonic rumble
  /////////////////
  appendStereoHighPass(layer, stage, 60.0f);

  /////////////////
  // Stage 2: Pre-EQ - shape tone BEFORE distortion
  // This is critical for amp character
  /////////////////
  // Mid-presence boost for cut and definition
  auto preEqL               = stage->appendTypedBlock<ParametricEq>("pre-eq-L");
  auto preEqR               = stage->appendTypedBlock<ParametricEq>("pre-eq-R");
  preEqL->param(0)->_coarse = 900.0f;   // frequency: ~900Hz for rock character
  preEqL->param(1)->_coarse = 1.2f;     // width: medium-wide
  preEqL->param(2)->_coarse = 3.0f + drive * 3.0f;  // gain: more boost with more drive
  preEqR->param(0)->_coarse = 900.0f;
  preEqR->param(1)->_coarse = 1.2f;
  preEqR->param(2)->_coarse = 3.0f + drive * 3.0f;
  preEqL->addDspChannel(0);
  preEqR->addDspChannel(1);

  /////////////////
  // Stage 3: First gain stage (preamp character)
  // Lighter saturation, adds odd harmonics
  /////////////////
  float driveDb1 = -30.0f + drive * 36.0f;  // -30dB to +6dB range
  auto dist1L               = stage->appendTypedBlock<Distortion>("dist1-L");
  auto dist1R               = stage->appendTypedBlock<Distortion>("dist1-R");
  dist1L->param(0)->_coarse = driveDb1;
  dist1R->param(0)->_coarse = driveDb1;
  dist1L->addDspChannel(0);
  dist1R->addDspChannel(1);

  /////////////////
  // Stage 4: Inter-stage EQ
  // Tighten low end between stages to prevent mud
  /////////////////
  appendStereoHighPass(layer, stage, 80.0f);

  /////////////////
  // Stage 5: Second gain stage (power amp compression)
  // Heavier saturation, adds even harmonics via shaper
  /////////////////
  float driveDb2 = -36.0f + drive * 30.0f;  // -36dB to -6dB range
  auto dist2L               = stage->appendTypedBlock<SHAPER>("dist2-L");
  auto dist2R               = stage->appendTypedBlock<SHAPER>("dist2-R");
  dist2L->param(0)->_coarse = 0.15f + drive * 0.35f;  // 0.15 to 0.5 shaper amount
  dist2R->param(0)->_coarse = 0.15f + drive * 0.35f;
  dist2L->addDspChannel(0);
  dist2R->addDspChannel(1);

  /////////////////
  // Stage 6: Third gain stage (final saturation/sustain)
  // Soft clip for smooth sustain
  /////////////////
  float driveDb3 = -24.0f + drive * 18.0f;  // subtle additional saturation
  auto dist3L               = stage->appendTypedBlock<Distortion>("dist3-L");
  auto dist3R               = stage->appendTypedBlock<Distortion>("dist3-R");
  dist3L->param(0)->_coarse = driveDb3;
  dist3R->param(0)->_coarse = driveDb3;
  dist3L->addDspChannel(0);
  dist3R->addDspChannel(1);

  /////////////////
  // Stage 7: Post-EQ Tone Stack
  // Bass shelf
  /////////////////
  auto bassL               = stage->appendTypedBlock<PARABASS>("bass-L");
  auto bassR               = stage->appendTypedBlock<PARABASS>("bass-R");
  bassL->param(0)->_coarse = 120.0f;   // frequency
  bassL->param(1)->_coarse = bass;     // gain in dB
  bassR->param(0)->_coarse = 120.0f;
  bassR->param(1)->_coarse = bass;
  bassL->addDspChannel(0);
  bassR->addDspChannel(1);

  // Mid parametric
  auto midL               = stage->appendTypedBlock<ParametricEq>("mid-L");
  auto midR               = stage->appendTypedBlock<ParametricEq>("mid-R");
  midL->param(0)->_coarse = 650.0f;    // frequency: classic mid scoop/boost point
  midL->param(1)->_coarse = 1.0f;      // width
  midL->param(2)->_coarse = mid;       // gain in dB
  midR->param(0)->_coarse = 650.0f;
  midR->param(1)->_coarse = 1.0f;
  midR->param(2)->_coarse = mid;
  midL->addDspChannel(0);
  midR->addDspChannel(1);

  // Treble shelf
  auto trebL               = stage->appendTypedBlock<PARATREBLE>("treble-L");
  auto trebR               = stage->appendTypedBlock<PARATREBLE>("treble-R");
  trebL->param(0)->_coarse = 3000.0f;  // frequency
  trebL->param(1)->_coarse = treble;   // gain in dB
  trebR->param(0)->_coarse = 3000.0f;
  trebR->param(1)->_coarse = treble;
  trebL->addDspChannel(0);
  trebR->addDspChannel(1);

  /////////////////
  // Stage 8: Presence control (attack/definition)
  // High shelf for pick attack and clarity
  /////////////////
  if (presence != 0.0f) {
    auto presL               = stage->appendTypedBlock<PARATREBLE>("presence-L");
    auto presR               = stage->appendTypedBlock<PARATREBLE>("presence-R");
    presL->param(0)->_coarse = 4500.0f;  // frequency: presence range
    presL->param(1)->_coarse = presence; // gain in dB
    presR->param(0)->_coarse = 4500.0f;
    presR->param(1)->_coarse = presence;
    presL->addDspChannel(0);
    presR->addDspChannel(1);
  }

  /////////////////
  // Stage 9: Cabinet Simulation
  // This is CRITICAL for removing digital harshness
  // Real speakers roll off sharply above 5-6kHz
  /////////////////
  // Speaker resonance bump around 100Hz
  auto cabResL               = stage->appendTypedBlock<ParametricEq>("cab-res-L");
  auto cabResR               = stage->appendTypedBlock<ParametricEq>("cab-res-R");
  cabResL->param(0)->_coarse = 100.0f;  // frequency: speaker resonance
  cabResL->param(1)->_coarse = 2.0f;    // width: fairly narrow
  cabResL->param(2)->_coarse = 3.0f;    // gain: subtle boost
  cabResR->param(0)->_coarse = 100.0f;
  cabResR->param(1)->_coarse = 2.0f;
  cabResR->param(2)->_coarse = 3.0f;
  cabResL->addDspChannel(0);
  cabResR->addDspChannel(1);

  // Speaker rolloff - THE most important part of amp sim!
  // Removes the fizzy/harsh highs that plague digital distortion
  appendStereoLowPass(layer, stage, 5500.0f);

  /////////////////
  // Stage 10: Output gain compensation
  // Make up for gain lost in processing
  /////////////////
  auto outGain               = stage->appendTypedBlock<STEREO_GAIN>("output-gain");
  outGain->param(0)->_coarse = 6.0f - drive * 3.0f;  // compensate for drive level
}
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendStereoReverbX(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    int seed,
    float tscale,
    float mint,
    float maxt,
    float minspeed,
    float maxspeed) {
  auto fdn4 = stage->appendTypedBlock<Fdn4ReverbX>("reverb");

  math::FRANDOMGEN rg(10);
  fdn4->_speed            = rg.rangedf(minspeed, maxspeed);
  fdn4->param(0)->_coarse = 0.5f; // wet/dry mix
  fdn4->param(1)->_coarse = tscale * rg.rangedf(mint, maxt);
  fdn4->param(2)->_coarse = tscale * rg.rangedf(mint, maxt);
  fdn4->param(3)->_coarse = tscale * rg.rangedf(mint, maxt);
  fdn4->param(4)->_coarse = tscale * rg.rangedf(mint, maxt);
  fdn4->_input_gain       = 0.65;
  fdn4->_output_gain      = 0.75;
  fdn4->update();
  return fdn4;
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoEnhancer(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  auto stereoenh           = stage->appendTypedBlock<StereoEnhancer>("enhancer");
  auto width_mod           = stereoenh->param(0)->_mods;
  auto WIDTHCONTROL        = layer->appendController<CustomControllerData>("WIDTH");
  width_mod->_src1         = WIDTHCONTROL;
  width_mod->_src1Scale    = 1.0;
  WIDTHCONTROL->_oncompute = [](CustomControllerInst* cci) { //
    cci->setFloatValue( 0.7f);
  };
}
///////////////////////////////////////////////////////////////////////////////
void appendNiceVerb(
    lyrdata_ptr_t fxlayer, //
    dspstagedata_ptr_t fxstage,
    float wetness) {
  auto rv2               = appendStereoReverbX(fxlayer, fxstage, 10, 1.77, 0.01, 0.15, 0.00001, 0.001);
  auto rv1               = appendStereoReverbX(fxlayer, fxstage, 10, 0.47, 0.01, 0.15, 0.00001, 0.001);
  auto rv0               = appendStereoReverbX(fxlayer, fxstage, 10, 0.27, 0.01, 0.15, 0.00001, 0.001);
  rv0->param(0)->_coarse = wetness; // wet/dry mix
  rv1->param(0)->_coarse = wetness; // wet/dry mix
  rv2->param(0)->_coarse = wetness; // wet/dry mix
}
///////////////////////////////////////////////////////////////////////////////
void appendPitchChorus(
    lyrdata_ptr_t fxlayer, //
    dspstagedata_ptr_t fxstage,
    float wetness,
    float cents,
    float feedback) {
  /////////////////
  auto shifterL               = appendRecursivePitchShifter(fxlayer, fxstage,feedback);
  auto shifterR               = appendRecursivePitchShifter(fxlayer, fxstage,feedback);
  shifterL->param(0)->_coarse = wetness; // wet/dry mix
  shifterR->param(0)->_coarse = wetness; // wet/dry mix
  /////////////////
  shifterL->addDspChannel(0); // chorus voice 1 on left
  shifterR->addDspChannel(1); // chorus voice 2 on right
  /////////////////
  auto PITCHMODL        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT1");
  auto PITCHMODR        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT2");
  auto pmodL            = shifterL->param(1)->_mods;
  auto pmodR            = shifterR->param(1)->_mods;
  pmodL->_src1          = PITCHMODL;
  pmodL->_src1Scale     = 1.0;
  pmodR->_src1          = PITCHMODR;
  pmodR->_src1Scale     = 1.0;
  PITCHMODL->_oncompute = [cents](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( cents + sinf(time * pi2 * 0.03f) * 25 );
    return cci->getFloatValue();
  };
  PITCHMODR->_oncompute = [cents](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( cents + sinf(time * pi2 * 0.07f) * 30 );
    return cci->getFloatValue();
  };
}
void appendPitchRec(
    lyrdata_ptr_t fxlayer, //
    dspstagedata_ptr_t fxstage,
    float cents,
    float wetness,
    float feedback){
  /////////////////
  auto shifterL               = appendRecursivePitchShifter(fxlayer, fxstage, feedback);
  auto shifterR               = appendRecursivePitchShifter(fxlayer, fxstage, feedback);
  shifterL->param(0)->_coarse = wetness; // wet/dry mix
  shifterR->param(0)->_coarse = wetness; // wet/dry mix
  /////////////////
  shifterL->addDspChannel(0); // chorus voice 1 on left
  shifterR->addDspChannel(1); // chorus voice 2 on right
  /////////////////
  auto PITCHMODL        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT1");
  auto PITCHMODR        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT2");
  auto pmodL            = shifterL->param(1)->_mods;
  auto pmodR            = shifterR->param(1)->_mods;
  pmodL->_src1          = PITCHMODL;
  pmodL->_src1Scale     = 1.0;
  pmodR->_src1          = PITCHMODR;
  pmodR->_src1Scale     = 1.0;
  PITCHMODL->_oncompute = [cents](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( cents + sinf(time * pi2 * 0.07f) * 15 );
    return cci->getFloatValue();
  };
  PITCHMODR->_oncompute = [cents](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( cents + sinf(time * pi2 * 0.17f) * 20 );
    return cci->getFloatValue();
  };

  appendNiceVerb(fxlayer, fxstage, 0.1);
}
///////////////////////////////////////////////////////////////////////////////
void appendWackiVerb(lyrdata_ptr_t fxlayer, dspstagedata_ptr_t fxstage) {
  auto crverb = [fxlayer, fxstage](
                    int seed, //
                    float mint,
                    float maxt,
                    float mins,
                    float maxs) {
    math::FRANDOMGEN rg(seed);
    auto rv               = appendStereoReverbX(fxlayer, fxstage, seed, 0.0f, 0.00, 0.0, 0.00001, 0.001);
    rv->param(0)->_coarse = 0.25f; // wet/dry mix
    rv->param(1)->_coarse = 0.0f;
    rv->param(2)->_coarse = 0.0f;
    rv->param(3)->_coarse = 0.0f;
    rv->param(4)->_coarse = 0.0f;
    /////////////////
    auto basename = FormatString("crverb-%d-", seed);
    /////////////////
    float midt = (mint + maxt) * 0.5f;
    float rang = (maxt - mint) * 0.5f;
    /////////////////
    auto RV0DTMODA        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTA");
    auto& rvmoda          = rv->param(1)->_mods;
    rvmoda->_src1         = RV0DTMODA;
    rvmoda->_src1Scale    = 1.0;
    float speed           = rg.rangedf(mins, maxs);
    RV0DTMODA->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->setFloatValue( midt + sinf(time * pi2 * speed) * rang);
      return cci->getFloatValue();
    };
    /////////////////
    auto RV0DTMODB        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTB");
    auto rvmodb           = rv->param(2)->_mods;
    rvmodb->_src1         = RV0DTMODB;
    rvmodb->_src1Scale    = 1.0;
    speed                 = rg.rangedf(mins, maxs);
    RV0DTMODB->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->setFloatValue( midt + sinf(time * pi2 * speed) * rang);
      return cci->getFloatValue();
    };
    /////////////////
    auto RV0DTMODC        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTC");
    auto rvmodc           = rv->param(3)->_mods;
    rvmodc->_src1         = RV0DTMODC;
    rvmodc->_src1Scale    = 1.0;
    speed                 = rg.rangedf(mins, maxs);
    RV0DTMODC->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->setFloatValue( midt + sinf(time * pi2 * speed) * rang);
      return cci->getFloatValue();
    };
    /////////////////
    auto RV0DTDMOD        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTD");
    auto rvmodd           = rv->param(4)->_mods;
    rvmodd->_src1         = RV0DTDMOD;
    rvmodd->_src1Scale    = 1.0;
    speed                 = rg.rangedf(mins, maxs);
    RV0DTDMOD->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->setFloatValue( midt + sinf(time * pi2 * speed) * rang);
      return cci->getFloatValue();
    };
  };
  crverb(11, 0.09, 0.13, 0.1, 0.17);
  crverb(112, 0.017, 0.037, 0.1, 0.5);
  crverb(113, 0.007, 0.009, 0.5, 1.5);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
