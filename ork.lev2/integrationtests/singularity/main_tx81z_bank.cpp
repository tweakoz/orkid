#include "harness.h"
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/fxgen.h>

int main(int argc, char** argv) {
  auto app = createEZapp(argc, argv);
  ////////////////////////////////////////////////
  // main bus effect
  ////////////////////////////////////////////////
  synth::instance()->_masterGain = decibel_to_linear_amp_ratio(30.0f);
  auto mainbus                   = synth::instance()->outputBus("main");
  auto bussource                 = mainbus->createScopeSource();
  if (1) { // create mixbus effect ?

    // auto fxlayer = fxpreset_stereochorus();
    // auto fxlayer = fxpreset_fdn4reverb();
    auto fxlayer = fxpreset_multitest();
    // auto fxlayer = fxpreset_niceverb();
    // auto fxlayer = fxpreset_echoverb();
    // auto fxlayer = fxpreset_wackiverb();
    // auto fxlayer = fxpreset_pitchoctup();
    // auto fxlayer = fxpreset_pitchwave();
    // auto fxlayer = fxpreset_pitchchorus();
    mainbus->setBusDSP(fxlayer);
  }
  ////////////////////////////////////////////////
  // create visualizers
  ////////////////////////////////////////////////
  ui::anchor::Bounds nobounds;
  auto scope1    = create_oscilloscope(app->_hudvp, nobounds, "layer");
  auto scope2    = create_oscilloscope(app->_hudvp, nobounds, "main-bus");
  auto analyzer1 = create_spectrumanalyzer(app->_hudvp, nobounds, "layer");
  auto analyzer2 = create_spectrumanalyzer(app->_hudvp, nobounds, "main-bus");
  scope1->setRect(-10, 0, 480, 240, true);
  scope2->setRect(-10, 480, 480, 240, true);
  analyzer1->setRect(480, 0, 810, 240, true);
  analyzer2->setRect(480, 480, 810, 240, true);
  bussource->connect(scope2->_sink);
  bussource->connect(analyzer2->_sink);
  //////////////////////////////////////////////////////////////////////////////
  auto progview = createProgramView(app->_hudvp, nobounds, "program");
  auto perfview = createProfilerView(app->_hudvp, nobounds, "profiler");
  progview->setRect(-10, 240, 890, 240, true);
  perfview->setRect(900, 240, 1290 - 900, 240, true);
  //////////////////////////////////////////////////////////////////////////////
  auto basepath = basePath() / "tx81z";
  auto bank     = std::make_shared<Tx81zData>();
  bank->loadBank(basepath / "tx81z_1.syx");
  bank->loadBank(basepath / "tx81z_2.syx");
  bank->loadBank(basepath / "tx81z_3.syx");
  bank->loadBank(basepath / "tx81z_4.syx");
  int count = 0;
  for (int i = 0; i < 128; i++) { // 2 32 patch banks
    auto prg       = bank->getProgram(i);
    auto layerdata = prg->getLayer(0);
    //////////////////////////////////////
    // connect OPS to scope 1
    //////////////////////////////////////
    auto ops_stage = layerdata->stageByName("OPS");
    auto ops_block = ops_stage->_blockdatas[0];
    if (ops_block) {
      auto ops_source         = ops_block->createScopeSource();
      ops_source->_dspchannel = 0;
      // ops_source->connect(scope1->_sink);
      // ops_source->connect(analyzer1->_sink);
    }
    //////////////////////////////////////
    // connect layerout to scope 1
    //////////////////////////////////////
    auto layersource = layerdata->createScopeSource();
    layersource->connect(scope1->_sink);
    layersource->connect(analyzer1->_sink);
    //////////////////////////////////////
    seq1(180.0f, i * 4, prg);
  }
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->exec();
  return 0;
}
