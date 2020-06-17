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
  auto toplayout = app->_hudvp->_layout;
  auto guidehl   = toplayout->left();
  auto guidehm   = toplayout->centerH();
  auto guidehr   = toplayout->right();

  auto guidevt = toplayout->top();
  auto guidev0 = toplayout->proportionalHorizontalGuide(0.333333);
  auto guidev1 = toplayout->proportionalHorizontalGuide(0.666666);
  auto guidevb = toplayout->bottom();
  ////////////////////////////////////////////////
  // create visualizers
  ////////////////////////////////////////////////
  ui::anchor::Bounds top_left, middle_left, bottom_left;
  ui::anchor::Bounds top_right, middle_right, bottom_right;

  top_left._top    = guidevt;
  top_left._left   = guidehl;
  top_left._bottom = guidev0;
  top_left._right  = guidehm;

  middle_left         = top_left;
  middle_left._top    = guidev0;
  middle_left._bottom = guidev1;

  bottom_left         = top_left;
  bottom_left._top    = guidev1;
  bottom_left._bottom = guidevb;

  top_right._top    = guidevt;
  top_right._left   = guidehm;
  top_right._bottom = guidev0;
  top_right._right  = guidehr;

  middle_right         = top_right;
  middle_right._top    = guidev0;
  middle_right._bottom = guidev1;

  bottom_right         = top_right;
  bottom_right._top    = guidev1;
  bottom_right._bottom = guidevb;

  auto scope1    = create_oscilloscope(app->_hudvp, top_left, "fm4-op");
  auto scope2    = create_oscilloscope(app->_hudvp, middle_left, "layer");
  auto scope3    = create_oscilloscope(app->_hudvp, bottom_left, "main-bus");
  auto analyzer1 = create_spectrumanalyzer(app->_hudvp, top_right, "fm4-op");
  auto analyzer2 = create_spectrumanalyzer(app->_hudvp, middle_right, "layer");
  auto analyzer3 = create_spectrumanalyzer(app->_hudvp, bottom_right, "main-bus");
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
  auto layerdata = program->getLayer(0);
  //////////////////////////////////////
  // connect OPS to scope 1
  //////////////////////////////////////
  auto ops_stage = layerdata->stageByName("OPS");
  auto ops_block = ops_stage->_blockdatas[0];
  if (ops_block) {
    auto ops_source         = ops_block->createScopeSource();
    ops_source->_dspchannel = 0;
    ops_source->connect(scope1->_sink);
    ops_source->connect(analyzer1->_sink);
  }
  //////////////////////////////////////
  // connect layerout to scope 2
  //////////////////////////////////////
  auto layersource = layerdata->createScopeSource();
  layersource->connect(scope2->_sink);
  layersource->connect(analyzer2->_sink);
  //////////////////////////////////////
  // connect mainbus to scope 3
  //////////////////////////////////////
  bussource->connect(scope3->_sink);
  bussource->connect(analyzer3->_sink);
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->exec();
  return 0;
}
