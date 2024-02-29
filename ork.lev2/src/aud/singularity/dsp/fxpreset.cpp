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
lyrdata_ptr_t fxpreset_distortionpluschorus() {
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
lyrdata_ptr_t fxpreset_distortionplusecho() {
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
  appendStereoHighFreqStimulator(
      fxlayer, //
      fxstage,
      500.0f, // cutoff
      18.0f,  // drive
      0.0f);  // output gain
  appendStereoDistortion(fxlayer, fxstage, -12.0);
  auto chorus               = appendStereoChorus(fxlayer, fxstage);
  chorus->param(0)->_coarse = 0.5f;  // delay time (L)
  chorus->param(1)->_coarse = 0.25f; // delay time (R)
  chorus->param(2)->_coarse = 0.25;  // feedback
  chorus->param(3)->_coarse = 0.35;  // wet/dry mix
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_stereochorus() {
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
lyrdata_ptr_t fxpreset_fdn4reverb() {
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
lyrdata_ptr_t fxpreset_fdnxreverb() {
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
lyrdata_ptr_t fxpreset_fdn8reverb() {
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
lyrdata_ptr_t fxpreset_oiltankreverb() {
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
lyrdata_ptr_t fxpreset_guywireeverb() {
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
lyrdata_ptr_t fxpreset_niceverb() {
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
lyrdata_ptr_t fxpreset_testverb() {
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
lyrdata_ptr_t fxpreset_echoverb() {
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
lyrdata_ptr_t fxpreset_wackiverb() {
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
lyrdata_ptr_t fxpreset_pitchoctup() {
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
lyrdata_ptr_t fxpreset_pitchfifthup() {
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
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_pitchwave() {
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
lyrdata_ptr_t fxpreset_pitchchorus() {
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
lyrdata_ptr_t fxpreset_pitchrec() {
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
lyrdata_ptr_t fxpreset_stereodelay() {
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
lyrdata_ptr_t fxpreset_multitest() {
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
lyrdata_ptr_t fxpreset_vowels() {
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
  auto A = std::make_shared<SpectralImpulseResponse>();
  auto E = std::make_shared<SpectralImpulseResponse>();
  auto I = std::make_shared<SpectralImpulseResponse>();
  auto O = std::make_shared<SpectralImpulseResponse>();
  auto U = std::make_shared<SpectralImpulseResponse>();
  A->vowelFormant('A',strength);
  E->vowelFormant('E',strength);
  I->vowelFormant('I',strength);
  O->vowelFormant('O',strength);
  U->vowelFormant('U',strength);
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
lyrdata_ptr_t fxpreset_violins() {
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
    IR->violinFormant(16.0f);
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
lyrdata_ptr_t fxpreset_testamp() {
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
lyrdata_ptr_t fxpreset_IR(std::string ampname, float mix, float postgain) {
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
  auto base      = ork::audio::singularity::basePath() / "IRs";
  auto ir_path = base/ampname;
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
lyrdata_ptr_t fxpreset_none() {
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

  addpreset("none", fxpreset_none());
  addpreset("Reverb:FDN4", fxpreset_fdn4reverb());
  addpreset("Reverb:FDN8", fxpreset_fdn8reverb());
  addpreset("Reverb:FDNX", fxpreset_fdnxreverb());
  addpreset("Reverb:OilTank", fxpreset_oiltankreverb());
  //addpreset("Reverb:TEST", fxpreset_testverb());
  addpreset("Reverb:GuyWire", fxpreset_guywireeverb());
  addpreset("Reverb:NiceVerb", fxpreset_niceverb());
  addpreset("Reverb:EchoVerb", fxpreset_echoverb());
  addpreset("Reverb:WackiVerb", fxpreset_wackiverb());
  addpreset("Distortion+Chorus", fxpreset_distortionpluschorus());
  addpreset("Distortion+Echo", fxpreset_distortionplusecho());
  addpreset("StereoChorus", fxpreset_stereochorus());
  addpreset("ShifterFifthup", fxpreset_pitchfifthup());
  addpreset("ShifterOctaveup", fxpreset_pitchoctup());
  addpreset("ShifterWave", fxpreset_pitchwave());
  addpreset("ShifterChorus", fxpreset_pitchchorus());
  addpreset("ShifterRec", fxpreset_pitchrec());
  addpreset("MultiTest", fxpreset_multitest());
  addpreset("StereoDelay", fxpreset_stereodelay());
  addpreset("Vowels", fxpreset_vowels());
  addpreset("Violins", fxpreset_violins());
  addpreset("AmpTest", fxpreset_testamp());
  addpreset("AmpAT4050A", fxpreset_IR("Fender SuperChamp AT4050.wav",1.0,-12));
  addpreset("AmpAT4050B", fxpreset_IR("Fender Bassman AT4050.wav",1.0,-12));
  addpreset("AmpAT4050C", fxpreset_IR("Fender 68-Vibrolux AT4050.wav",1.0,+0));
  addpreset("AmpJCM2KA", fxpreset_IR("Marshall JCM2000 SM57.wav",1.0,-12));
  addpreset("AmpJCM2KB", fxpreset_IR("Marshall JCM2000 SM57 off Axis.wav",1.0,-18));
  addpreset("AmpJMKSC2", fxpreset_IR("JoeMeek SC2 Impulse Hard.wav",1.0,-6));
  addpreset("AmpSVTB52", fxpreset_IR("Ampeg SVT Beta52.wav",1.0,-6));
  addpreset("IR-WLANE", fxpreset_IR("WoodruffLane.wav",0.5,-6));
  addpreset("IR-1", fxpreset_IR("TunnelToHeaven.wav",0.25,-18));
  addpreset("IR-2", fxpreset_IR("TunnelToHell.wav",0.1,-24));
  addpreset("IR-3", fxpreset_IR("SteinmanHall.wav",0.20,-6));
  addpreset("IR-4", fxpreset_IR("StorageTankNo7.wav",0.20,-6));
  addpreset("IR-5", fxpreset_IR("PacificHall.wav",0.20,-6));
  addpreset("IR-6", fxpreset_IR("PepperCanyonHall.wav",0.18,-18));
  addpreset("IR-7", fxpreset_IR("5012 Black Hole.SDIR",0.25,0));
  
}
} // namespace ork::audio::singularity
