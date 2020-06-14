#include <ork/lev2/aud/singularity/fxgen.h>

namespace ork::audio::singularity {
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
  appendStereoChorus(fxlayer, fxstage);
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
  appendStereoReverb(fxlayer, fxstage, 0.47);
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
  auto rv2              = appendStereoReverbX(fxlayer, fxstage, 10, 1.77, 0.01, 0.15, 0.00001, 0.001);
  auto rv1              = appendStereoReverbX(fxlayer, fxstage, 10, 0.47, 0.01, 0.15, 0.00001, 0.001);
  auto rv0              = appendStereoReverbX(fxlayer, fxstage, 10, 0.27, 0.01, 0.15, 0.00001, 0.001);
  rv0->param(0)._coarse = 0.1f; // wet/dry mix
  rv1->param(0)._coarse = 0.1f; // wet/dry mix
  rv2->param(0)._coarse = 0.1f; // wet/dry mix
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
  auto chorus              = appendStereoChorus(fxlayer, fxstage);
  chorus->param(2)._coarse = 0.5;  // feedback
  chorus->param(3)._coarse = 0.35; // wet/dry mix
  /////////////////
  auto rv2 = appendStereoReverbX(fxlayer, fxstage, 10, 5.0, 0.01, 0.15, 0.0001, 0.1);
  auto rv1 = appendStereoReverbX(fxlayer, fxstage, 11, 1.47, 0.01, 0.15, 0.00001, 0.01);
  auto rv0 = appendStereoReverbX(fxlayer, fxstage, 12, 0.17, 0.01, 0.15, 0.00001, 0.001);
  /////////////////
  appendStereoParaEQ(fxlayer, fxstage, 10, 8, -36);
  appendStereoParaEQ(fxlayer, fxstage, 20, 8, -12);
  // appendStereoParaEQ(fxlayer, fxstage, 30, 8, -3);
  /////////////////
  rv0->param(0)._coarse = 0.07f; // wet/dry mix
  rv1->param(0)._coarse = 0.07f; // wet/dry mix
  rv2->param(0)._coarse = 0.07f; // wet/dry mix
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
  auto chorus              = appendStereoChorus(fxlayer, fxstage);
  chorus->param(2)._coarse = 0.5;  // feedback
  chorus->param(3)._coarse = 0.35; // wet/dry mix
  /////////////////
  appendWackiVerb(fxlayer, fxstage);
  appendStereoParaEQ(fxlayer, fxstage, 10, 8, -36);
  appendStereoParaEQ(fxlayer, fxstage, 20, 8, -12);
  // appendStereoParaEQ(fxlayer, fxstage, 30, 8, -3);
  /////////////////
  // rv1->param(0)._coarse = 0.27f; // wet/dry mix
  // rv2->param(0)._coarse = 0.27f; // wet/dry mix
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
  auto shifter              = appendPitchShifter(fxlayer, fxstage);
  shifter->param(0)._coarse = 0.5;  // wet/dry mix
  shifter->param(1)._coarse = 1200; // 1 octave up
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
  auto shifter              = appendPitchShifter(fxlayer, fxstage);
  shifter->param(0)._coarse = 0.5; // wet/dry mix
  /////////////////
  auto PITCHMOD        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT");
  auto& pmod           = shifter->param(1)._mods;
  pmod._src1           = PITCHMOD;
  pmod._src1Depth      = 1.0;
  PITCHMOD->_oncompute = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->_curval = (1.0f + sinf(time * pi2 * 0.03f)) * 2400.0f;
    return cci->_curval;
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
  appendPitchChorus(fxlayer, fxstage, 0.5, 35.0f);
  return fxlayer;
} ///////////////////////////////////////////////////////////////////////////////
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
      50.0f); // pitchmod (cents)
  /////////////////
  appendStereoHighFreqStimulator(
      fxlayer, //
      fxstage,
      2000.0f, // cutoff
      36.0f,   // drive
      -12.0f); // output gain
  /////////////////
  return fxlayer;
}

} // namespace ork::audio::singularity
