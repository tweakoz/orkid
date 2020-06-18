#include "harness.h"
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/fxgen.h>

int main(int argc, char** argv) {
  auto app = createEZapp(argc, argv);
  // auto toplayoutgroup            = app->_topLayoutGroup;
  synth::instance()->_masterGain = decibel_to_linear_amp_ratio(30.0f);
  ////////////////////////////////////////////////
  // main bus effect
  ////////////////////////////////////////////////
  auto mainbus   = synth::instance()->outputBus("main");
  auto bussource = mainbus->createScopeSource();
  if (1) { // create mixbus effect ?
    auto fxlayer = fxpreset_multitest();
    mainbus->setBusDSP(fxlayer);
  }
  ////////////////////////////////////////////////
  // UI layout
  ////////////////////////////////////////////////
  auto toplayout = app->_hudvp->_layout;
  auto guidehl   = toplayout->left();
  auto guidehc   = toplayout->centerH();
  auto guidehr   = toplayout->right();
  auto guidevt   = toplayout->top();
  auto guidev0   = toplayout->fixedHorizontalGuide(240);
  auto guidev1   = toplayout->fixedHorizontalGuide(480);
  auto guidehd   = toplayout->proportionalVerticalGuide(0.75);
  auto guidevb   = toplayout->bottom();

  ui::anchor::Bounds top_left, middle_left, bottom_left;
  ui::anchor::Bounds top_right, middle_right, bottom_right;

  top_left._top    = guidevt;
  top_left._left   = guidehl;
  top_left._bottom = guidev0;
  top_left._right  = guidehd;

  top_right._top    = guidevt;
  top_right._left   = guidehd;
  top_right._bottom = guidev0;
  top_right._right  = guidehr;

  middle_left._top    = guidev0;
  middle_left._left   = guidehl;
  middle_left._bottom = guidev1;
  middle_left._right  = guidehc;

  middle_right._top    = guidev0;
  middle_right._left   = guidehc;
  middle_right._bottom = guidev1;
  middle_right._right  = guidehr;

  bottom_left         = middle_left;
  bottom_left._top    = guidev1;
  bottom_left._bottom = guidevb;

  bottom_right         = middle_right;
  bottom_right._top    = guidev1;
  bottom_right._bottom = guidevb;
  ////////////////////////////////////////////////
  // create visualizers
  ////////////////////////////////////////////////
  auto progview  = createProgramView(app->_hudvp, top_left, "program");
  auto perfview  = createProfilerView(app->_hudvp, top_right, "profiler");
  auto scope1    = create_oscilloscope(app->_hudvp, middle_left, "layer");
  auto scope2    = create_oscilloscope(app->_hudvp, bottom_left, "main-bus");
  auto analyzer1 = create_spectrumanalyzer(app->_hudvp, middle_right, "layer");
  auto analyzer2 = create_spectrumanalyzer(app->_hudvp, bottom_right, "main-bus");
  //////////////////////////////////////////////////////////////////////////////
  auto basepath = basePath() / "tx81z";
  auto bank     = std::make_shared<Tx81zData>();
  bank->loadBank(basepath / "tx81z_1.syx");
  bank->loadBank(basepath / "tx81z_2.syx");
  bank->loadBank(basepath / "tx81z_3.syx");
  bank->loadBank(basepath / "tx81z_4.syx");
  synth::instance()->_globalbank  = bank->_bankdata;
  synth::instance()->_globalprgit = synth::instance()->_globalbank->_programs.begin();
  //////////////////////////////////////////////////////////////////////////////
  // auto program   = bank->getProgramByName("LatelyBass");
  auto program = testpattern(bank, argc, argv);
  if (!program) {
    return 0;
  }
  //////////////////////////////////////
  // connect OPS to scope 1
  //////////////////////////////////////
  /*auto ops_stage = layerdata->stageByName("OPS");
  auto ops_block = ops_stage->_blockdatas[0];
  if (ops_block) {
    auto ops_source         = ops_block->createScopeSource();
    ops_source->_dspchannel = 0;
    ops_source->connect(scope1->_sink);
    ops_source->connect(analyzer1->_sink);
  }*/
  //////////////////////////////////////
  // connect all program layers to scope 1
  //////////////////////////////////////
  for (auto item : bank->_bankdata->_programs) {
    int id           = item.first;
    auto program     = item.second;
    auto layerdata   = program->getLayer(0);
    auto layersource = layerdata->createScopeSource();
    layersource->connect(scope1->_sink);
    layersource->connect(analyzer1->_sink);
  }
  //////////////////////////////////////
  // connect mainbus to scope 2
  //////////////////////////////////////
  bussource->connect(scope2->_sink);
  bussource->connect(analyzer2->_sink);
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->exec();
  return 0;
}
