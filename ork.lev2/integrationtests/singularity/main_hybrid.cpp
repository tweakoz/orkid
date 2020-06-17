#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/alg_nonlin.h>
#include <ork/lev2/aud/singularity/dsp_ringmod.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/alg_filters.h>
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
  // stereo enhancer
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  auto stereoenh           = fxstage->appendTypedBlock<StereoEnhancer>();
  auto WIDTHCONTROL        = fxlayer->appendController<CustomControllerData>("STEREOWIDTH");
  auto& width_mod          = stereoenh->param(0)._mods;
  width_mod._src1          = WIDTHCONTROL;
  width_mod._src1Depth     = 1.0;
  WIDTHCONTROL->_oncompute = [](CustomControllerInst* cci) { //
    float index  = cci->_layer->_layerTime;
    float wave   = (0.5f + sinf(index) * 0.5);
    cci->_curval = wave;
  };
  /////////////////
  // stereo echo
  /////////////////
  auto echo              = fxstage->appendTypedBlock<StereoDynamicEcho>();
  echo->param(0)._coarse = 2.0;  // delay time (sec)
  echo->param(1)._coarse = 0.25; // feedback
  echo->param(2)._coarse = 0.15; // wet/dry mix
  //
  mainbus->setBusDSP(fxlayer);
  ////////////////////////////////////////////////
  // create visualizers
  ////////////////////////////////////////////////
  ui::anchor::Bounds nobounds;
  auto scope1    = create_oscilloscope(app->_hudvp, nobounds);
  auto scope2    = create_oscilloscope(app->_hudvp, nobounds);
  auto scope3    = create_oscilloscope(app->_hudvp, nobounds);
  auto analyzer1 = create_spectrumanalyzer(app->_hudvp, nobounds);
  auto analyzer2 = create_spectrumanalyzer(app->_hudvp, nobounds);
  auto analyzer3 = create_spectrumanalyzer(app->_hudvp, nobounds);
  scope1->setRect(-10, 0, 480, 240, true);
  scope2->setRect(-10, 240, 480, 240, true);
  scope3->setRect(-10, 480, 480, 240, true);
  analyzer1->setRect(480, 0, 810, 240, true);
  analyzer2->setRect(480, 240, 810, 240, true);
  analyzer3->setRect(480, 480, 810, 240, true);
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
    auto layersource = layerdata->createScopeSource();
    layersource->connect(scope3->_sink);
    layersource->connect(analyzer3->_sink);
    //////////////////////////////////////
    // setup dsp graph
    //////////////////////////////////////
    configureCz1Algorithm(layerdata, 2);
    auto dcostage = layerdata->stageByName("DCO");
    auto modstage = layerdata->stageByName("MOD");
    auto ampstage = layerdata->stageByName("AMP");

    auto make_dco = [&](int dcochannel) {
      auto czoscdata  = std::make_shared<CzOscData>();
      auto dco        = dcostage->appendTypedBlock<CZX>(czoscdata, dcochannel);
      auto distortion = ampstage->appendTypedBlock<Distortion>(); //  Kurzweil Distorion
      auto allpass    = ampstage->appendTypedBlock<TwoPoleAllPass>();
      auto lopass1    = ampstage->appendTypedBlock<FourPoleLowPassWithSep>();
      // auto lopass2               = ampstage->appendTypedBlock<FourPoleLowPassWithSep>();
      auto amp                   = ampstage->appendTypedBlock<AMP_MONOIO>();
      dco->_dspchannel[0]        = dcochannel;
      distortion->_dspchannel[0] = dcochannel;
      allpass->_dspchannel[0]    = dcochannel;
      lopass1->_dspchannel[0]    = dcochannel;
      // lopass2->_dspchannel[0]    = dcochannel;
      amp->_dspchannel[0] = dcochannel;
      //////////////////////////////////////
      auto dco_source         = dco->createScopeSource();
      dco_source->_dspchannel = dcochannel;
      bool isch1              = dcochannel == 1;
      dco_source->connect(isch1 ? scope2->_sink : scope1->_sink);
      dco_source->connect(isch1 ? analyzer2->_sink : analyzer1->_sink);
      //////////////////////////////////////
      distortion->param(0)._coarse = rangedf(-30.0f, -21.0f);
      //////////////////////////////////////
      // 2 4poles in series == 48db/octave
      //////////////////////////////////////
      allpass->_inputPad        = 0.75f;
      allpass->param(0)._coarse = 5500.0f; // center
      allpass->param(1)._coarse = 0.0f;    // width
      lopass1->_inputPad        = 0.75f;
      lopass1->param(0)._coarse = 4500.0f; // cutoff
      lopass1->param(1)._coarse = 0.0f;    // resonance
      lopass1->param(2)._coarse = 1200.0f; // sep
      // lopass2->_inputPad        = 0.75f;
      // lopass2->param(0)._coarse = 4500.0f; // cutoff
      // lopass2->param(1)._coarse = 0.0f;    // resonance
      // lopass2->param(2)._coarse = 1200.0f; // sep
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
      DCWENV->addSegment("seg0", rangedf(0.1, 0.2), rangedf(0.3, 0.75));
      DCWENV->addSegment("seg1", rangedf(0.5, 1.2), rangedf(0.5, 1.0));
      DCWENV->addSegment("seg2", rangedf(1.5, 2.3), rangedf(0.25, 0.75));
      DCWENV->addSegment("seg3", rangedf(1.5, 2.3), rangedf(0.25, 0.75));
      DCWENV->addSegment("seg4", rangedf(1.5, 2.3), rangedf(0.25, 0.75));
      DCWENV->addSegment("seg5", rangedf(1.5, 2.3), rangedf(0.25, 0.75));
      DCWENV->addSegment("seg6", 1, 0);
      DCWENV->_ampenv = false;
      //////////////////////////////////////
      auto DCOENV = layerdata->appendController<RateLevelEnvData>(envname_dco);
      DCOENV->addSegment("seg0", rangedf(0.1, 0.2), rangedf(600, 1800));
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
    auto& panmod           = stereomix->param(1)._mods;
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
