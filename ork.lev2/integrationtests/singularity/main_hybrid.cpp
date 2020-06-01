#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/alg_nonlin.h>
#include <ork/lev2/aud/singularity/dsp_ringmod.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <random>

using namespace ork::audio::singularity;

int main(int argc, char** argv) {
  auto app = createEZapp(argc, argv);
  ////////////////////////////////////////////////
  // main bus effect
  ////////////////////////////////////////////////
  auto mainbus      = synth::instance()->outputBus("main");
  auto bussource    = mainbus->createScopeSource();
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  //  Kurzweil Distorion
  /////////////////
  auto fxstage1 = fxalg->appendStage("FX1");
  fxstage1->setNumIos(2, 2); // stereo in, stereo out
  auto dL                 = fxstage1->appendTypedBlock<Distortion>();
  auto dR                 = fxstage1->appendTypedBlock<Distortion>();
  dL->_dspchannel[0]      = 0;
  dR->_dspchannel[0]      = 1;
  auto& dLmod             = dL->getParam(0)._mods;
  auto& dRmod             = dR->getParam(0)._mods;
  auto DISTCONTROL        = fxlayer->appendController<CustomControllerData>("PAN");
  dLmod._src1             = DISTCONTROL;
  dLmod._src1Depth        = 1.0;
  dRmod._src1             = DISTCONTROL;
  dRmod._src1Depth        = 1.0;
  DISTCONTROL->_oncompute = [](CustomControllerInst* cci) { //
    float index = cci->_layer->_layerTime;
    float wave  = (0.5f + sinf(index) * 0.5);
    // cci->_curval = lerp(1.0, 0.5, wave); // Wrap
    cci->_curval = lerp(-30.0f, -24.0f, wave); // Distortion
  };
  /////////////////
  // stereo enhancer
  /////////////////
  auto fxstage2 = fxalg->appendStage("FX2");
  fxstage2->setNumIos(2, 2); // stereo in, stereo out
  auto stereoenh           = fxstage1->appendTypedBlock<StereoEnhancer>();
  auto& width_mod          = stereoenh->getParam(0)._mods;
  auto WIDTHCONTROL        = fxlayer->appendController<CustomControllerData>("PAN");
  width_mod._src1          = WIDTHCONTROL;
  width_mod._src1Depth     = 1.0;
  WIDTHCONTROL->_oncompute = [](CustomControllerInst* cci) { //
    float index  = cci->_layer->_layerTime;
    float wave   = (0.5f + sinf(index) * 0.5);
    cci->_curval = wave;
  };
  //
  mainbus->setBusDSP(fxlayer);
  ////////////////////////////////////////////////
  // create visualizers
  ////////////////////////////////////////////////
  auto scope    = create_oscilloscope(app->_hudvp);
  auto analyzer = create_spectrumanalyzer(app->_hudvp);
  auto envview  = create_envelope_analyzer(app->_hudvp);
  scope->setRect(0, 0, 480, 256, true);
  analyzer->setRect(480, 0, 810, 256, true);
  envview->setRect(-10, 720 - 467, 1300, 477, true);
  envview->setProperty<float>("timewidth", 5.0f);
  bussource->connect(scope->_sink);
  bussource->connect(analyzer->_sink);
  ////////////////////////////////////////////////
  // random generators
  ////////////////////////////////////////////////
  std::mt19937 gen(10);
  std::uniform_int_distribution<> rdist(0, 1000000);
  auto irandom = [&]() -> int { return rdist(gen); };
  auto frandom = [&]() -> float { return double(rdist(gen)) * 0.000001f; };
  auto rangedf = [&](float fmin, float fmax) -> float { return lerp(fmin, fmax, frandom()); };
  ////////////////////////////////////////////////
  // create random dsp programs and trigger them
  ////////////////////////////////////////////////
  for (int i = 0; i < 120; i++) {
    prgdata_constptr_t program;
    lyrdata_ptr_t layerdata;
    auto mutable_program   = std::make_shared<ProgramData>();
    layerdata              = mutable_program->newLayer();
    mutable_program->_role = "czx";
    mutable_program->_name = "test";
    program                = mutable_program;
    //////////////////////////////////////
    // setup dsp graph
    //////////////////////////////////////
    configureCz1Algorithm(layerdata, 2);
    auto dcostage = layerdata->stageByName("DCO");
    auto modstage = layerdata->stageByName("MOD");
    auto ampstage = layerdata->stageByName("AMP");

    auto make_dco = [&](int dcochannel) {
      auto czoscdata      = std::make_shared<CzOscData>();
      auto dco            = dcostage->appendTypedBlock<CZX>(czoscdata, dcochannel);
      auto amp            = ampstage->appendTypedBlock<AMP_MONOIO>();
      dco->_dspchannel[0] = dcochannel;
      amp->_dspchannel[0] = dcochannel;
      //////////////////////////////////////
      auto envname_dca = FormatString("DCAENV%d", dcochannel);
      auto envname_dcw = FormatString("DCWENV%d", dcochannel);
      auto envname_dco = FormatString("DCOENV%d", dcochannel);
      auto name_lfoa   = FormatString("LFOA%d", dcochannel);
      auto name_lfob   = FormatString("LFOB%d", dcochannel);
      //////////////////////////////////////
      // setup envelope
      //////////////////////////////////////
      auto DCAENV     = layerdata->appendController<RateLevelEnvData>(envname_dca);
      DCAENV->_ampenv = true;
      DCAENV->addSegment("seg0", .25, .25, 0.5);
      DCAENV->addSegment("seg1", .25, .5, 1.5);
      DCAENV->addSegment("seg2", .25, .75, 0.5);
      DCAENV->addSegment("seg3", .25, 1.0, -1.65);
      DCAENV->addSegment("seg4", 1, 1);
      DCAENV->addSegment("seg5", 3, .75, 0.5);
      DCAENV->addSegment("seg6", 3, .5, -9.0);
      DCAENV->addSegment("seg7", 3, .25, 4.0);
      DCAENV->addSegment("seg8", 3, 0, 0.25);
      //////////////////////////////////////
      auto DCWENV = layerdata->appendController<RateLevelEnvData>(envname_dcw);
      DCWENV->addSegment("seg0", 0.1, rangedf(0.3, 0.9));
      DCWENV->addSegment("seg1", 1, rangedf(0.5, 1.0));
      DCWENV->addSegment("seg2", 2, rangedf(0.25, 0.5));
      DCWENV->addSegment("seg3", 2, 1);
      DCWENV->addSegment("seg4", 2, 1);
      DCWENV->addSegment("seg5", 1, 1);
      DCWENV->addSegment("seg6", 1, 0);
      DCWENV->_ampenv = false;
      //////////////////////////////////////
      auto DCOENV = layerdata->appendController<RateLevelEnvData>(envname_dco);
      DCOENV->addSegment("seg0", rangedf(0.05, 0.1), rangedf(600, 1800));
      DCOENV->addSegment("seg1", rangedf(3, 5), rangedf(450, 900));
      DCOENV->addSegment("seg2", rangedf(2, 4), rangedf(350, 700));
      DCOENV->addSegment("seg3", rangedf(1.5, 2.5), rangedf(120, 250));
      DCOENV->addSegment("seg4", 2, 60);
      DCOENV->addSegment("seg5", 1, 1);
      DCOENV->addSegment("seg6", 1, 0);
      DCOENV->_ampenv = false;
      //////////////////////////////////////
      // setup LFO
      //////////////////////////////////////
      float rate1    = 0.5 + frandom() * 0.5;
      float rate2    = 0.5 + frandom() * 0.5;
      auto LFO1      = layerdata->appendController<LfoData>(name_lfoa);
      LFO1->_minRate = rate1;
      LFO1->_maxRate = rate1;
      LFO1->_shape   = "Sine";
      auto LFO2      = layerdata->appendController<LfoData>(name_lfob);
      LFO2->_minRate = rate2;
      LFO2->_maxRate = rate2;
      LFO2->_shape   = "Sine";
      //////////////////////////////////////
      // setup modulation routing
      //////////////////////////////////////
      auto& pitch_mod      = dco->_paramd[0]._mods;
      pitch_mod._src1      = DCOENV;
      pitch_mod._src1Depth = 1.0f;
      //////////////////////////////////////
      if (dcochannel == 1) { // add detune
        auto DETUNE             = layerdata->appendController<CustomControllerData>("DCO1DETUNE");
        pitch_mod._src2         = DETUNE;
        pitch_mod._src2MinDepth = 1.0;
        pitch_mod._src2MaxDepth = 1.0;
        DETUNE->_onkeyon        = [&](CustomControllerInst* cci, //
                               const KeyOnInfo& KOI) {    //
          cci->_curval = rangedf(-50, 50);
        };
      }
      //////////////////////////////////////
      auto& modulation_index_param          = dco->_paramd[1]._mods;
      modulation_index_param._src1          = DCWENV;
      modulation_index_param._src1Depth     = 1.0;
      modulation_index_param._src2          = LFO1;
      modulation_index_param._src2DepthCtrl = LFO2;
      modulation_index_param._src2MinDepth  = 0.5;
      modulation_index_param._src2MaxDepth  = 0.1;
      //////////////////////////////////////
      czoscdata->_octaveScale  = 0.5;
      czoscdata->_dcoBaseWaveA = irandom() & 0x7;
      czoscdata->_dcoBaseWaveB = irandom() & 0x7;
      czoscdata->_dcoWindow    = irandom() % 3;
      //////////////////////////////////////
      auto& amp_param   = amp->_paramd[0];
      amp_param._coarse = 0.0f;
      amp_param.useDefaultEvaluator();
      amp_param._mods._src1      = DCAENV;
      amp_param._mods._src1Depth = 1.0;
    };
    make_dco(0);
    make_dco(1);
    modstage->appendTypedBlock<RingMod>();
    //////////////////////////////////////
    // pan controller
    //////////////////////////////////////
    auto mixstage          = layerdata->stageByName("MIX");
    auto stereomix         = mixstage->_blockdatas[0];
    auto& panmod           = stereomix->getParam(1)._mods;
    auto PANCONTROL        = layerdata->appendController<CustomControllerData>("PAN");
    panmod._src1           = PANCONTROL;
    panmod._src1Depth      = 1.0;
    PANCONTROL->_oncompute = [](CustomControllerInst* cci) { //
      float index  = cci->_layer->_layerTime / 3.0f;
      index        = std::clamp(index, 0.0f, 1.0f);
      float pan    = lerp(-1.0f, 1.0f, index);
      pan          = std::clamp(pan, -1.0f, 1.0f);
      cci->_curval = pan;
    };
    //////////////////////////////////////
    // envelope viewer
    //////////////////////////////////////
    controllerdata_ptr_t inspect_env = layerdata->controllerByName("DCOENV0");
    auto env_source                  = inspect_env->createScopeSource();
    env_source->connect(envview->_sink);
    //////////////////////////////////////
    // play test notes
    //////////////////////////////////////
    enqueue_audio_event(program, 1.0f + (i * 3), 3.0, 48);
  }
  //////////////////////////////////////////////////////////////////////////////
  // test harness UI
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->exec();
  return 0;
}
