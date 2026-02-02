////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/lev2/aud/singularity/spectral.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_distortionpluschorus(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  appendStereoEnhancer(fxlayer, fxstage);
  /////////////////
  appendStereoDistortion(fxlayer, fxstage, -42);
  appendStereoHighFreqStimulator(
      fxlayer, //
      fxstage,
      1500.0f, // cutoff
      24.0f,   // drive
      -12.0f); // output gain
  // auto chorus = appendStereoChorus(fxlayer, fxstage);
  appendPitchChorus(fxlayer, fxstage, 0.5, 12.50f, 0.25);
  // chorus->param(0)->_coarse  = 0.05f; // delay time (L)
  // chorus->param(1)->_coarse  = 0.03f; // delay time (R)
  // chorus->param(2)->_coarse  = 0.5; // feedback
  // chorus->param(3)->_coarse  = 0.5;  // wet/dry mix
  auto gain               = fxstage->appendTypedBlock<STEREO_GAIN>("final-gain");
  gain->param(0)->_coarse = 12.0f; // delay time (L)
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_distortionplusecho(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  appendStereoEnhancer(fxlayer, fxstage);
  /////////////////
  auto echo = appendStereoStereoDynamicEcho(fxlayer, fxstage, 0.55, 0.28, 0.15, 0.15);
  /////////////////
  appendStereoDistortion(fxlayer, fxstage, 0.0);
  auto chorus               = appendStereoChorus(fxlayer, fxstage);
  appendStereoHighFreqStimulator(
      fxlayer, //
      fxstage,
      500.0f, // cutoff
      18.0f,  // drive
      12.0f);  // output gain
  chorus->param(0)->_coarse = 0.5f;  // delay time (L)
  chorus->param(1)->_coarse = 0.25f; // delay time (R)
  chorus->param(2)->_coarse = 0.25;  // feedback
  chorus->param(3)->_coarse = 0.35;  // wet/dry mix
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_stereochorus(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  appendStereoEnhancer(fxlayer, fxstage);
  auto chorus               = appendStereoChorus(fxlayer, fxstage);
  chorus->param(0)->_coarse = 0.025f;  // delay time (L)
  chorus->param(1)->_coarse = 0.0125f; // delay time (R)
  chorus->param(2)->_coarse = 0.75;    // feedback
  chorus->param(3)->_coarse = 0.45;    // wet/dry mix
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_prochorus(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  // Pro chorus with multiple voices, filtering, and LFO phase offsets
  /////////////////
  appendProChorus(fxlayer, fxstage, 0.5f);
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_proflanger(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  // Pro flanger with multiple voices, high feedback, slow sweep
  /////////////////
  appendProFlanger(fxlayer, fxstage, 0.5f, 0.78f);
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_prodistortion(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  // Medium crunch with balanced EQ
  appendProDistortion(fxlayer, fxstage,
    0.5f,   // drive: medium
    0.0f,   // bass: neutral
    0.0f,   // mid: neutral
    0.0f,   // treble: neutral
    2.0f);  // presence: slight boost for clarity
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_prodistortion_clean(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2);
  /////////////////
  // Clean with warmth - Fender-ish
  appendProDistortion(fxlayer, fxstage,
    0.15f,  // drive: light - just tube warmth
    2.0f,   // bass: slight boost for fullness
    -1.0f,  // mid: slight scoop for clarity
    1.0f,   // treble: slight boost for sparkle
    1.0f);  // presence: subtle
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_prodistortion_crunch(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2);
  /////////////////
  // Classic rock crunch - Marshall-ish
  appendProDistortion(fxlayer, fxstage,
    0.6f,   // drive: medium-high
    1.0f,   // bass: slight boost
    2.0f,   // mid: boosted for rock cut
    1.0f,   // treble: slight boost
    3.0f);  // presence: more attack
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_prodistortion_heavy(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2);
  /////////////////
  // High gain - Mesa/5150-ish
  appendProDistortion(fxlayer, fxstage,
    0.85f,  // drive: high
    -1.0f,  // bass: slight cut for tightness
    -2.0f,  // mid: scooped for modern metal
    2.0f,   // treble: boosted for bite
    4.0f);  // presence: aggressive attack
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
// Pro combo: Distortion → Chorus → Echo (standard pro signal chain order)
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_prodist_chorus_echo(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2);
  /////////////////
  // 1. Distortion first - crunch setting
  appendProDistortion(fxlayer, fxstage,
    0.55f,  // drive: moderate crunch
    0.0f,   // bass: neutral
    1.0f,   // mid: slight boost
    0.0f,   // treble: neutral
    2.0f);  // presence: some bite
  /////////////////
  // 2. Chorus second - adds width to distorted tone
  appendProChorus(fxlayer, fxstage, 0.35f);
  /////////////////
  // 3. Echo last - repeats the processed signal
  appendStereoStereoDynamicEcho(fxlayer, fxstage, 0.4f, 0.4f, 0.5f, 0.355f); // wetness: -9dB
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_fdn4reverb(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  appendStereoEnhancer(fxlayer, fxstage);
  /////////////////
  float matrix_gain = 0.458;
  float out_gain    = 0.4;
  /////////////////
  auto fdn4A                    = fxstage->appendTypedBlock<Fdn4Reverb>("diffusionA");
  fdn4A->param(0)->_coarse      = 0.5f; // wet/dry mix
  fdn4A->_input_gain            = 0.3;
  fdn4A->_output_gain           = out_gain;
  fdn4A->_time_base             = 0.007;
  fdn4A->_time_scale            = 1.071;
  fdn4A->_matrix_gain           = matrix_gain;
  fdn4A->_hipass_cutoff         = 100.0;
  fdn4A->_allpass_shift_frq_bas = 700.0;
  fdn4A->_allpass_shift_frq_mul = 1.3;
  fdn4A->_allpass_count         = 12;
  fdn4A->matrixHouseholder(fdn4A->_matrix_gain);
  fdn4A->update();
  /////////////////
  auto fdn4B                    = fxstage->appendTypedBlock<Fdn4Reverb>("diffusionB");
  fdn4B->param(0)->_coarse      = 0.5f; // wet/dry mix
  fdn4B->_input_gain            = 0.4;
  fdn4B->_output_gain           = out_gain;
  fdn4B->_time_base             = 0.017;
  fdn4B->_time_scale            = 1.061;
  fdn4B->_matrix_gain           = matrix_gain;
  fdn4B->_hipass_cutoff         = 100.0;
  fdn4B->_allpass_shift_frq_bas = 500.0;
  fdn4B->_allpass_shift_frq_mul = 1.4;
  fdn4B->_allpass_count         = 8;
  fdn4B->matrixHouseholder(fdn4B->_matrix_gain);
  fdn4B->update();
  /////////////////
  auto fdn4C                    = fxstage->appendTypedBlock<Fdn4Reverb>("diffusionC");
  fdn4C->param(0)->_coarse      = 0.5f; // wet/dry mix
  fdn4C->_input_gain            = 0.5;
  fdn4C->_output_gain           = out_gain;
  fdn4C->_time_base             = 0.037;
  fdn4C->_time_scale            = 1.061;
  fdn4C->_matrix_gain           = matrix_gain;
  fdn4C->_hipass_cutoff         = 100.0;
  fdn4C->_allpass_shift_frq_bas = 1500.0;
  fdn4C->_allpass_shift_frq_mul = 1.5;
  fdn4C->_allpass_count         = 4;
  fdn4C->matrixHouseholder(fdn4C->_matrix_gain);
  fdn4C->update();
  /////////////////
  auto fdn4D                    = fxstage->appendTypedBlock<Fdn4Reverb>("diffusionD");
  fdn4D->param(0)->_coarse      = 0.5f; // wet/dry mix
  fdn4D->_input_gain            = 0.5;
  fdn4D->_output_gain           = out_gain;
  fdn4D->_time_base             = 0.077;
  fdn4D->_time_scale            = 1.161;
  fdn4D->_matrix_gain           = matrix_gain;
  fdn4D->_hipass_cutoff         = 100.0;
  fdn4D->_allpass_shift_frq_bas = 1500.0;
  fdn4D->_allpass_shift_frq_mul = 1.5;
  fdn4D->_allpass_count         = 4;
  fdn4D->matrixHouseholder(fdn4D->_matrix_gain);
  fdn4D->update();
  /////////////////
  auto stereoenh           = fxstage->appendTypedBlock<StereoDynamicEcho>("echo2");
  auto width_mod           = stereoenh->param(0)->_mods;
  auto WIDTHCONTROL        = fxlayer->appendController<CustomControllerData>("WIDTH2");
  width_mod->_src1         = WIDTHCONTROL;
  width_mod->_src1Scale    = 1.0;
  WIDTHCONTROL->_oncompute = [](CustomControllerInst* cci) { //
    cci->setFloatValue(0.7f);
  };
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_fdnxreverb(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  appendStereoEnhancer(fxlayer, fxstage);
  /////////////////
  auto fdn8               = fxstage->appendTypedBlock<Fdn8Reverb>("diffusionD");
  fdn8->param(0)->_coarse = -12.0f; // input Gain
  fdn8->param(1)->_coarse = 30.0f;  // output Gain
  fdn8->_time_base        = 0.1;
  /////////////////
  float matrix_gain = 0.458;
  float out_gain    = 0.4;
  float fdn4_mix = 0.4;
  /////////////////
  auto fdn4A                    = fxstage->appendTypedBlock<Fdn4Reverb>("diffusionA");
  fdn4A->param(0)->_coarse      = fdn4_mix; // wet/dry mix
  fdn4A->_input_gain            = 0.3;
  fdn4A->_output_gain           = out_gain;
  fdn4A->_time_base             = 0.007;
  fdn4A->_time_scale            = 1.071;
  fdn4A->_matrix_gain           = matrix_gain;
  fdn4A->_hipass_cutoff         = 100.0;
  fdn4A->_allpass_shift_frq_bas = 700.0;
  fdn4A->_allpass_shift_frq_mul = 1.3;
  fdn4A->_allpass_count         = 12;
  fdn4A->matrixHouseholder(fdn4A->_matrix_gain);
  fdn4A->update();
  /////////////////
  auto fdn4B                    = fxstage->appendTypedBlock<Fdn4Reverb>("diffusionB");
  fdn4B->param(0)->_coarse      = fdn4_mix; // wet/dry mix
  fdn4B->_input_gain            = 0.4;
  fdn4B->_output_gain           = out_gain;
  fdn4B->_time_base             = 0.017;
  fdn4B->_time_scale            = 1.061;
  fdn4B->_matrix_gain           = matrix_gain;
  fdn4B->_hipass_cutoff         = 100.0;
  fdn4B->_allpass_shift_frq_bas = 500.0;
  fdn4B->_allpass_shift_frq_mul = 1.4;
  fdn4B->_allpass_count         = 8;
  fdn4B->matrixHouseholder(fdn4B->_matrix_gain);
  fdn4B->update();
  /////////////////
  auto fdn4C                    = fxstage->appendTypedBlock<Fdn4Reverb>("diffusionC");
  fdn4C->param(0)->_coarse      = fdn4_mix; // wet/dry mix
  fdn4C->_input_gain            = 0.5;
  fdn4C->_output_gain           = out_gain;
  fdn4C->_time_base             = 0.037;
  fdn4C->_time_scale            = 1.061;
  fdn4C->_matrix_gain           = matrix_gain;
  fdn4C->_hipass_cutoff         = 100.0;
  fdn4C->_allpass_shift_frq_bas = 1500.0;
  fdn4C->_allpass_shift_frq_mul = 1.5;
  fdn4C->_allpass_count         = 4;
  fdn4C->matrixHouseholder(fdn4C->_matrix_gain);
  fdn4C->update();
  /////////////////
  auto fdn4D                    = fxstage->appendTypedBlock<Fdn4Reverb>("diffusionD");
  fdn4D->param(0)->_coarse      = fdn4_mix; // wet/dry mix
  fdn4D->_input_gain            = 0.5;
  fdn4D->_output_gain           = out_gain;
  fdn4D->_time_base             = 0.127;
  fdn4D->_time_scale            = 1.161;
  fdn4D->_matrix_gain           = matrix_gain;
  fdn4D->_hipass_cutoff         = 100.0;
  fdn4D->_allpass_shift_frq_bas = 1500.0;
  fdn4D->_allpass_shift_frq_mul = 1.5;
  fdn4D->_allpass_count         = 4;
  fdn4D->matrixHouseholder(fdn4D->_matrix_gain);
  fdn4D->update();
  /////////////////
  appendMildStereoChorus(fxlayer, fxstage);
  // fdn4D->update();
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_fdn8reverb(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto fdn8               = fxstage->appendTypedBlock<Fdn8Reverb>("diffusionD");
  fdn8->paramByName("inputGain")->_coarse = -36.0f; // input Gain
  fdn8->paramByName("ereflGain")->_coarse = -36.0f;  // output Gain
  fdn8->paramByName("outputGain")->_coarse = 18.0f;  // output Gain
  fdn8->paramByName("diffuserTimeModRate")->_coarse = 0.01f;  // feedback time modulation rate
  fdn8->paramByName("diffuserTimeModAmp")->_coarse = 0.99f;  // feedback time modulation rate
  fdn8->paramByName("diffuserGain")->_coarse = -3.0f;  // feedback time modulation rate
  fdn8->paramByName("fbTimeModRate")->_coarse = 0.0001f;  // feedback time modulation rate
  fdn8->paramByName("fbTimeModAmp")->_coarse = 0.001f;  // feedback time modulation rate
  fdn8->paramByName("fbGain")->_coarse = -9.9f;  // feedback gain
  fdn8->paramByName("fbLpCutoff")->_coarse = 7000.0f;  // feedback gain
  fdn8->paramByName("fbHpCutoff")->_coarse = 1000.0f;  // feedback gain
  fdn8->paramByName("inputHpCutoff")->_coarse = 300.0f;  // feedback time modulation rate
  fdn8->paramByName("baseTime")->_coarse = 0.1f;  // feedback time modulation rate
  fdn8->paramByName("ereflTime")->_coarse = 0.3f;  // feedback time modulation rate
  fdn8->_time_base        = 0.1;
  /////////////////
  // fdn4D->update();
  //appendStereoEnhancer(fxlayer, fxstage);
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_oiltankreverb(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto reverb = appendOilTankReverb(fxlayer, fxstage);
  appendStereoEnhancer(fxlayer, fxstage);
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_guywireeverb(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto reverb = appendGuyWireReverb(fxlayer, fxstage);
  appendStereoEnhancer(fxlayer, fxstage);
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_niceverb(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto rv2               = appendStereoReverbX(fxlayer, fxstage, 10, 1.77, 0.01, 0.15, 0.00001, 0.001);
  auto rv1               = appendStereoReverbX(fxlayer, fxstage, 10, 0.47, 0.01, 0.15, 0.00001, 0.001);
  auto rv0               = appendStereoReverbX(fxlayer, fxstage, 10, 0.27, 0.01, 0.15, 0.00001, 0.001);
  rv0->param(0)->_coarse = 0.1f; // wet/dry mix
  rv1->param(0)->_coarse = 0.1f; // wet/dry mix
  rv2->param(0)->_coarse = 0.1f; // wet/dry mix
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_testverb(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto rev               = fxstage->appendTypedBlock<TestReverb>("Reverb:Test");
  //rev->param(0)->_coarse = 0.5f; // wet/dry mix
  //rev->_time_base        = 0.1;

  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_echoverb(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto chorus               = appendStereoChorus(fxlayer, fxstage);
  chorus->param(2)->_coarse = 0.5;  // feedback
  chorus->param(3)->_coarse = 0.35; // wet/dry mix
  /////////////////
  auto rv2 = appendStereoReverbX(fxlayer, fxstage, 10, 5.0, 0.01, 0.15, 0.0001, 0.1);
  auto rv1 = appendStereoReverbX(fxlayer, fxstage, 11, 1.47, 0.01, 0.15, 0.00001, 0.01);
  auto rv0 = appendStereoReverbX(fxlayer, fxstage, 12, 0.17, 0.01, 0.15, 0.00001, 0.001);
  /////////////////
  appendStereoParaEQ(fxlayer, fxstage, 10, 8, -36);
  appendStereoParaEQ(fxlayer, fxstage, 20, 8, -12);
  // appendStereoParaEQ(fxlayer, fxstage, 30, 8, -3);
  /////////////////
  rv0->param(0)->_coarse = 0.07f; // wet/dry mix
  rv1->param(0)->_coarse = 0.07f; // wet/dry mix
  rv2->param(0)->_coarse = 0.07f; // wet/dry mix
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_wackiverb(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto chorus               = appendStereoChorus(fxlayer, fxstage);
  chorus->param(2)->_coarse = 0.25; // feedback
  chorus->param(3)->_coarse = 0.35; // wet/dry mix
  /////////////////
  appendWackiVerb(fxlayer, fxstage);
  appendStereoParaEQ(fxlayer, fxstage, 10, 8, -36);
  appendStereoParaEQ(fxlayer, fxstage, 20, 8, -12);
  // appendStereoParaEQ(fxlayer, fxstage, 30, 8, -3);
  /////////////////
  // rv1->param(0)->_coarse = 0.27f; // wet/dry mix
  // rv2->param(0)->_coarse = 0.27f; // wet/dry mix
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_pitchoctup(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto shifter               = appendPitchShifter(fxlayer, fxstage);
  shifter->param(0)->_coarse = 0.5;  // wet/dry mix
  shifter->param(1)->_coarse = 1200; // 1 octave up
  /////////////////
  return fxlayer;
}
lyrdata_ptr_t fxpreset_pitchoctdn(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto shifter               = appendPitchShifter(fxlayer, fxstage);
  shifter->param(0)->_coarse = 0.85;  // wet/dry mix
  shifter->param(1)->_coarse = -1200; // 1 octave up
  /////////////////
  return fxlayer;
}
lyrdata_ptr_t fxpreset_pitchfifthup(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto shifter               = appendPitchShifter(fxlayer, fxstage);
  shifter->param(0)->_coarse = 0.5;  // wet/dry mix
  shifter->param(1)->_coarse = 700; // 1 octave up
  /////////////////
  return fxlayer;
}
lyrdata_ptr_t fxpreset_pitchfifthdn(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto shifter               = appendPitchShifter(fxlayer, fxstage);
  shifter->param(0)->_coarse = 0.85;  // wet/dry mix
  shifter->param(1)->_coarse = -700; // 1 octave up
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_pitchwave(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto shifter               = appendPitchShifter(fxlayer, fxstage);
  shifter->param(0)->_coarse = 0.5; // wet/dry mix
  /////////////////
  auto PITCHMOD        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT");
  auto pmod            = shifter->param(1)->_mods;
  pmod->_src1          = PITCHMOD;
  pmod->_src1Scale     = 1.0;
  PITCHMOD->_oncompute = [](CustomControllerInst* cci) { //
    float time    = cci->_layer->_layerTime;
    cci->_value.x = (1.0f + sinf(time * pi2 * 0.03f)) * 1200.0f;
    return cci->_value.x;
  };
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_pitchchorus(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  appendPitchChorus(fxlayer, fxstage, 0.5, 12.50f, 0.25);
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_pitchrec(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  appendPitchRec(fxlayer, fxstage, 400, 0.5, 0.35);
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_pitchrecdn(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  appendPitchRec(fxlayer, fxstage, -400, 0.5, 0.35);
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_stereodelay(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  auto stdel = fxstage->appendTypedBlock<StereoDelay>("StereoDelay");
  //stdel->param(0)->_coarse = fc;
  //stdel->param(0)->_coarse = fc;
  //eql->addDspChannel(0);
  //eqr->addDspChannel(1);
  //appendStereoHighPass(fxlayer, fxstage, 90.0f);
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_multitest(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  appendStereoHighPass(fxlayer, fxstage, 90.0f);
  appendStereoHighPass(fxlayer, fxstage, 105.0f);
  appendStereoHighPass(fxlayer, fxstage, 120.0f);
  /////////////////
  appendNiceVerb(fxlayer, fxstage, 0.1);
  /////////////////
  appendPitchChorus(
      fxlayer, //
      fxstage,
      0.25,  // wetness
      50.0f, // pitchmod (cents)
      0.5);  // feedback
  /////////////////
  appendStereoHighFreqStimulator(
      fxlayer, //
      fxstage,
      2000.0f, // cutoff
      30.0f,   // drive
      -24.0f); // output gain
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_vowels(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  auto tofd = fxstage->appendTypedBlock<ToFrequencyDomain>("tofd");
  auto vowels = fxstage->appendTypedBlock<SpectralConvolve>("vowels");
  auto dataset = std::make_shared<SpectralImpulseResponseDataSet>();
  dataset->_impulses.resize(256);
  vowels->_impulse_dataset = dataset;
  /////////////////
  float strength = 32.0f;
  float sr = s->sampleRate();
  auto A = std::make_shared<SpectralImpulseResponse>();
  auto E = std::make_shared<SpectralImpulseResponse>();
  auto I = std::make_shared<SpectralImpulseResponse>();
  auto O = std::make_shared<SpectralImpulseResponse>();
  auto U = std::make_shared<SpectralImpulseResponse>();
  A->vowelFormant(sr, 'A', strength);
  E->vowelFormant(sr, 'E', strength);
  I->vowelFormant(sr, 'I', strength);
  O->vowelFormant(sr, 'O', strength);
  U->vowelFormant(sr, 'U', strength);
  /////////////////
  for( int i=0; i<256; i++ ){
    float fi = float(i)/256.0f;
    auto IR = std::make_shared<SpectralImpulseResponse>();
    if( fi < 0.25f ){
      IR->blend(*A,*E,fi/0.25f);
    }
    else if( fi < 0.5f ){
      IR->blend(*E,*I,(fi-0.25f)/0.25f);
    }
    else if( fi < 0.75f ){
      IR->blend(*I,*O,(fi-0.5f)/0.25f);
    }
    else {
      IR->blend(*O,*U,(fi-0.75f)/0.25f);
    }

    dataset->_impulses[i] = IR;
  }
  auto totd = fxstage->appendTypedBlock<ToTimeDomain>("totd");
  /////////////////
  auto lfo = fxlayer->appendController<LfoData>("LFO");
  lfo->_minRate = 0.3;
  lfo->_maxRate = 0.3;

  vowels->param(0)->_coarse = 0.5f;
  vowels->param(0)->_mods->_src1 = lfo;
  vowels->param(0)->_mods->_src1Scale = 0.5;
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_violins(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  auto tofd = fxstage->appendTypedBlock<ToFrequencyDomain>("tofd");
  auto violins = fxstage->appendTypedBlock<SpectralConvolve>("violins");
  auto dataset = std::make_shared<SpectralImpulseResponseDataSet>();
  dataset->_impulses.resize(256);
  violins->_impulse_dataset = dataset;
  for( int i=0; i<256; i++ ){
    float fi = float(i)/256.0f;
    auto IR = std::make_shared<SpectralImpulseResponse>();
    IR->violinFormant(s->sampleRate(), 16.0f);
    dataset->_impulses[i] = IR;
  }

  auto totd = fxstage->appendTypedBlock<ToTimeDomain>("totd");
  /////////////////
  auto lfo = fxlayer->appendController<LfoData>("LFO");
  lfo->_minRate = 0.3;
  lfo->_maxRate = 0.3;

  violins->param(0)->_coarse = 0.5f;
  violins->param(0)->_mods->_src1 = lfo;
  violins->param(0)->_mods->_src1Scale = 0.5;
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_testamp(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  //appendStereoEnhancer(fxlayer, fxstage);
  //appendStereoShaper(fxlayer,fxstage,0.01f);
  //appendStereoHighFreqStimulator(fxlayer,fxstage,2000.0f,30.0f,-30.0f);
  //auto tofd = fxstage->appendTypedBlock<ToFrequencyDomain>("tofd");
  auto cabinet = fxstage->appendTypedBlock<SpectralConvolveTD>("cabinet");
  auto postamp = fxstage->appendTypedBlock<AMP_ADAPTIVE>("postamp");
  appendStereoHighPass(fxlayer, fxstage, 60.0f);
  auto dataset = std::make_shared<SpectralImpulseResponseDataSet>();
  dataset->_impulses.resize(1);
  cabinet->_impulse_dataset = dataset;
  auto IR = std::make_shared<SpectralImpulseResponse>();
  auto base      = ork::audio::singularity::basePath() / "IRs";
  auto ir_path = base/"Fender SuperChamp AT4050.wav";
  IR->loadAudioFileX(ir_path.c_str());
  dataset->_impulses[0] = IR;

  //auto totd = fxstage->appendTypedBlock<ToTimeDomain>("totd");
  /////////////////
  auto lfo = fxlayer->appendController<LfoData>("LFO");
  lfo->_minRate = 0.3;
  lfo->_maxRate = 0.3;

  cabinet->param(0)->_coarse = 0.5f;
  cabinet->param(0)->_mods->_src1 = lfo;
  cabinet->param(0)->_mods->_src1Scale = 0.5;
  postamp->param(0)->_coarse = -6;
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_IR(synth* s, std::string ampname, float mix, float postgain) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // load IR dataset
  /////////////////
  auto dataset = std::make_shared<SpectralImpulseResponseDataSet>();
  dataset->_impulses.resize(1);
  auto IR = std::make_shared<SpectralImpulseResponse>(256);
  auto base1      = ork::file::Path::data_dir() / "src" / "audio" / "ImpulseResponses";
  auto base2      = ork::audio::singularity::basePath() / "IRs";
  ork::file::Path ir_path;
  if((base1/ampname).exists()) {
    ir_path = base1/ampname;
  } else if((base2/ampname).exists()) {
    ir_path = base2/ampname;
  } else {
    OrkAssert(false); // not found
  }
  IR->loadAudioFileX(ir_path.c_str(),false);
  dataset->_impulses[0] = IR;
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  auto convolve = fxstage->appendTypedBlock<SpectralConvolveTD>("convolve");
  convolve->_impulse_dataset = dataset;
  auto postamp = fxstage->appendTypedBlock<AMP_ADAPTIVE>("postamp");
  appendStereoHighPass(fxlayer, fxstage, 60.0f);
  /////////////////
  convolve->param(0)->_coarse = mix;
  convolve->param(1)->_coarse = 1.0f;
  postamp->param(0)->_coarse = postgain;
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_none(synth* s) {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
void loadAllFxPresets(synth* s) {

  auto addpreset = [&](const std::string& named, //
                       lyrdata_ptr_t lyr) {
    s->_fxpresets.push_back(lyr);
    lyr->_name = named;
  };

  addpreset("none", fxpreset_none(s));
  addpreset("Reverb:FDN4", fxpreset_fdn4reverb(s));
  addpreset("Reverb:FDN8", fxpreset_fdn8reverb(s));
  addpreset("Reverb:FDNX", fxpreset_fdnxreverb(s));
  addpreset("Reverb:OilTank", fxpreset_oiltankreverb(s));
  //addpreset("Reverb:TEST", fxpreset_testverb(s));
  addpreset("Reverb:GuyWire", fxpreset_guywireeverb(s));
  addpreset("Reverb:NiceVerb", fxpreset_niceverb(s));
  addpreset("Reverb:EchoVerb", fxpreset_echoverb(s));
  addpreset("Reverb:WackiVerb", fxpreset_wackiverb(s));
  addpreset("Distortion+Chorus", fxpreset_distortionpluschorus(s));
  addpreset("Distortion+Echo", fxpreset_distortionplusecho(s));
  addpreset("StereoChorus", fxpreset_stereochorus(s));
  addpreset("ProChorus", fxpreset_prochorus(s));
  addpreset("ProFlanger", fxpreset_proflanger(s));
  addpreset("ProDistortion", fxpreset_prodistortion(s));
  addpreset("ProDist:Clean", fxpreset_prodistortion_clean(s));
  addpreset("ProDist:Crunch", fxpreset_prodistortion_crunch(s));
  addpreset("ProDist:Heavy", fxpreset_prodistortion_heavy(s));
  addpreset("ProDist+Chorus+Echo", fxpreset_prodist_chorus_echo(s));
  addpreset("ShifterFifthUp", fxpreset_pitchfifthup(s));
  addpreset("ShifterFifthDn", fxpreset_pitchfifthdn(s));
  addpreset("ShifterOctUp", fxpreset_pitchoctup(s));
  addpreset("ShifterOctDn", fxpreset_pitchoctdn(s));
  addpreset("ShifterWave", fxpreset_pitchwave(s));
  addpreset("ShifterChorus", fxpreset_pitchchorus(s));
  addpreset("ShifterRecUp", fxpreset_pitchrec(s));
  addpreset("ShifterRecDn", fxpreset_pitchrecdn(s));
  addpreset("MultiTest", fxpreset_multitest(s));
  addpreset("StereoDelay", fxpreset_stereodelay(s));
  addpreset("Vowels", fxpreset_vowels(s));
  addpreset("Violins", fxpreset_violins(s));
  addpreset("Amp-Test", fxpreset_testamp(s));
  addpreset("Amp-AT4050A", fxpreset_IR(s,"Fender SuperChamp AT4050.wav",1.0,-12));
  addpreset("Amp-AT4050B", fxpreset_IR(s,"Fender Bassman AT4050.wav",1.0,-12));
  addpreset("Amp-AT4050C", fxpreset_IR(s,"Fender 68-Vibrolux AT4050.wav",1.0,-6));
  addpreset("Amp-JCM2KA", fxpreset_IR(s,"Marshall JCM2000 SM57.wav",1.0,-15));
  addpreset("Amp-JCM2KB", fxpreset_IR(s,"Marshall JCM2000 SM57 off Axis.wav",1.0,-21));
  addpreset("Amp-JMKSC2", fxpreset_IR(s,"JoeMeek SC2 Impulse Hard.wav",1.0,-18));
  addpreset("Amp-SVTB52", fxpreset_IR(s,"Ampeg SVT Beta52.wav",1.0,-18));
  addpreset("IR-RadioAnn1", fxpreset_IR(s,"Sound 2.wav",0.05,-6));
  addpreset("IR-RadioAnn2", fxpreset_IR(s,"Sound 2.wav",0.20,-6));
  addpreset("IR-Mic1", fxpreset_IR(s,"Neumann U-87 AI - 15cm.wav",0.50,-16));
  addpreset("IR-Forest", fxpreset_IR(s,"forest.wav",0.0015,-6));
  addpreset("IR-RMX16b", fxpreset_IR(s,"rmx16-nonlin.wav",0.03,-6));
  addpreset("IR-RMX16", fxpreset_IR(s,"rmx16-nonlin.wav",0.01,-6));
  addpreset("IR-Spring1", fxpreset_IR(s,"spring1.wav",0.004,-6));
  addpreset("IR-Shower", fxpreset_IR(s,"shower.wav",0.0006,-6));
  addpreset("IR-Attic", fxpreset_IR(s,"attic1.wav",0.0003,-6));
  addpreset("IR-Rollo", fxpreset_IR(s,"rolloplate.wav",0.0015,-3));
  addpreset("IR-KnHall1", fxpreset_IR(s,"knightshall.wav",0.01,-3));
  addpreset("IR-KnHall2", fxpreset_IR(s,"knightshall.wav",0.003,-3));
  addpreset("IR-Cath5m1", fxpreset_IR(s,"cathedral5m.wav",0.01,-3));
  addpreset("IR-Cath5m2", fxpreset_IR(s,"cathedral5m.wav",0.003,-3));
  addpreset("IR-BH1", fxpreset_IR(s,"5012 Black Hole.SDIR",0.25,0));
  addpreset("IR-BH2", fxpreset_IR(s,"5012 Black Hole.SDIR",0.05,0));
  
}
} // namespace ork::audio::singularity
