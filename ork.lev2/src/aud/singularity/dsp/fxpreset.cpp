////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/fxgen.h>

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
      -12.0f);  // output gain
  //auto chorus = appendStereoChorus(fxlayer, fxstage);
  appendPitchChorus(fxlayer, fxstage, 0.5, 12.50f,0.25);
  //chorus->param(0)->_coarse  = 0.05f; // delay time (L)
  //chorus->param(1)->_coarse  = 0.03f; // delay time (R)
  //chorus->param(2)->_coarse  = 0.5; // feedback
  //chorus->param(3)->_coarse  = 0.5;  // wet/dry mix
  auto gain               = fxstage->appendTypedBlock<STEREO_GAIN>("final-gain");
  gain->param(0)->_coarse  = 12.0f; // delay time (L)
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
      18.0f,   // drive
      0.0f);  // output gain
  appendStereoDistortion(fxlayer, fxstage, -12.0);
  auto chorus = appendStereoChorus(fxlayer, fxstage);
  chorus->param(0)->_coarse  = 0.5f; // delay time (L)
  chorus->param(1)->_coarse  = 0.25f; // delay time (R)
  chorus->param(2)->_coarse  = 0.25; // feedback
  chorus->param(3)->_coarse  = 0.35;  // wet/dry mix
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
  auto chorus = appendStereoChorus(fxlayer, fxstage);
  chorus->param(0)->_coarse  = 0.025f; // delay time (L)
  chorus->param(1)->_coarse  = 0.0125f; // delay time (R)
  chorus->param(2)->_coarse  = 0.75; // feedback
  chorus->param(3)->_coarse  = 0.45;  // wet/dry mix
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
  auto reverb = appendStereoReverb(fxlayer, fxstage);
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_oilbarrelreverb() {
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
  auto reverb = appendOilBarrelReverb(fxlayer, fxstage);
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
  pmod->_src1Depth     = 1.0;
  PITCHMOD->_oncompute = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->_value.x = (1.0f + sinf(time * pi2 * 0.03f)) * 2400.0f;
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
  appendPitchChorus(fxlayer, fxstage, 0.5, 12.50f,0.25);
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
  appendPitchRec(fxlayer, fxstage,700,0.5,0.5);
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
      0.25,   // wetness
      50.0f,  // pitchmod (cents)
      0.5);   // feedback
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
  s->_fxpresets["none"]              = fxpreset_none();
  s->_fxpresets["distortion+chorus"] = fxpreset_distortionpluschorus();
  s->_fxpresets["distortion+echo"]   = fxpreset_distortionplusecho();
  s->_fxpresets["stereo-chorus"]     = fxpreset_stereochorus();
  s->_fxpresets["Reverb::FDN4"]      = fxpreset_fdn4reverb();
  s->_fxpresets["Reverb:OilBarrel"]  = fxpreset_oilbarrelreverb();
  s->_fxpresets["Reverb:GuyWire"]    = fxpreset_guywireeverb();
  s->_fxpresets["Reverb::NiceVerb"]  = fxpreset_niceverb();
  s->_fxpresets["Reverb::EchoVerb"]  = fxpreset_echoverb();
  s->_fxpresets["Reverb::WackiVerb"] = fxpreset_wackiverb();
  s->_fxpresets["shifter-octave-up"] = fxpreset_pitchoctup();
  s->_fxpresets["shifter-wave"]      = fxpreset_pitchwave();
  s->_fxpresets["shifter-chorus"]    = fxpreset_pitchchorus();
  s->_fxpresets["shifter-rec"]       = fxpreset_pitchrec();
  s->_fxpresets["multitest"]         = fxpreset_multitest();
}
} // namespace ork::audio::singularity
