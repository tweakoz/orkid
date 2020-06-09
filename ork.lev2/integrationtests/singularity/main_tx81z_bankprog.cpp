#include "harness.h"
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>

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
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  auto stereoenh           = fxstage->appendTypedBlock<StaticStereoEcho>();
  auto& width_mod          = stereoenh->param(0)._mods;
  auto WIDTHCONTROL        = fxlayer->appendController<CustomControllerData>("WIDTH");
  width_mod._src1          = WIDTHCONTROL;
  width_mod._src1Depth     = 1.0;
  WIDTHCONTROL->_oncompute = [](CustomControllerInst* cci) { //
    cci->_curval = 0.7f;
  };
  // auto echo              = fxstage->appendTypedBlock<StaticStereoEcho>();
  // echo->param(0)._coarse = 0.5; // delay time
  // echo->param(1)._coarse = 0.5; // feedback
  // echo->param(2)._coarse = 0.5; // wet/dry mix
  //
  mainbus->setBusDSP(fxlayer);
  ////////////////////////////////////////////////
  // create visualizers
  ////////////////////////////////////////////////
  auto scope1    = create_oscilloscope(app->_hudvp);
  auto scope2    = create_oscilloscope(app->_hudvp);
  auto analyzer1 = create_spectrumanalyzer(app->_hudvp);
  auto analyzer2 = create_spectrumanalyzer(app->_hudvp);
  scope1->setRect(-10, 0, 480, 240, true);
  scope2->setRect(-10, 480, 480, 240, true);
  analyzer1->setRect(480, 0, 810, 240, true);
  analyzer2->setRect(480, 480, 810, 240, true);
  //////////////////////////////////////////////////////////////////////////////
  auto basepath = basePath() / "tx81z";
  auto bank     = std::make_shared<Tx81zData>();
  bank->loadBank(basepath / "tx81z_1.syx");
  bank->loadBank(basepath / "tx81z_2.syx");
  bank->loadBank(basepath / "tx81z_3.syx");
  bank->loadBank(basepath / "tx81z_4.syx");
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
  // connect layerout to scope 3
  //////////////////////////////////////
  auto layersource = layerdata->createScopeSource();
  layersource->connect(scope2->_sink);
  layersource->connect(analyzer2->_sink);
  //////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->exec();
  return 0;
}
