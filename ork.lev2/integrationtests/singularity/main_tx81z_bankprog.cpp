#include "harness.h"
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/fxgen.h>

int main(int argc, char** argv) {
  auto app                       = createEZapp(argc, argv);
  auto toplayoutgroup            = app->_topLayoutGroup;
  auto toplayout                 = toplayoutgroup->_layout;
  synth::instance()->_masterGain = decibel_to_linear_amp_ratio(30.0f);
  ////////////////////////////////////////////////
  auto guidehl = toplayout->left();
  auto guidehm = toplayout->centerH();
  auto guidehr = toplayout->right();

  auto guidevt = toplayout->top();
  auto guidev0 = toplayout->proportionalHorizontalGuide(0.333333);
  auto guidev1 = toplayout->proportionalHorizontalGuide(0.666666);
  auto guidevb = toplayout->bottom();
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
  // create visualizers
  ////////////////////////////////////////////////
  auto scope1    = create_oscilloscope(app->_hudvp, "fm4-op");
  auto scope2    = create_oscilloscope(app->_hudvp, "layer");
  auto scope3    = create_oscilloscope(app->_hudvp, "main-bus");
  auto analyzer1 = create_spectrumanalyzer(app->_hudvp, "fm4-op");
  auto analyzer2 = create_spectrumanalyzer(app->_hudvp, "layer");
  auto analyzer3 = create_spectrumanalyzer(app->_hudvp, "main-bus");
  ///////////////////////////////
  // attach to layout
  ///////////////////////////////
  auto a3panel                    = analyzer3->_hudpanel;
  auto a3layout                   = toplayoutgroup->layoutAndAddChild(a3panel->_uipanel);
  a3panel->_panelLayout           = a3layout;
  a3panel->_uipanel->_enableClose = false;
  ///////////////////////////////
  a3layout->top()->anchorTo(guidev1);
  a3layout->bottom()->anchorTo(guidevb);
  a3layout->left()->anchorTo(guidehm);
  a3layout->right()->anchorTo(guidehr);
  ///////////////////////////////

  scope1->setRect(-10, 0, 480, 240, true);
  scope2->setRect(-10, 240, 480, 240, true);
  scope3->setRect(-10, 480, 480, 240, true);
  analyzer1->setRect(480, 0, 810, 240, true);
  analyzer2->setRect(480, 240, 810, 240, true);
  analyzer3->setRect(480, 480, 810, 240, true);
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
