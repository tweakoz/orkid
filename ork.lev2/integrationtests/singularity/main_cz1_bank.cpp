#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>
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
  // stereo enhancer
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  auto stereoenh           = fxstage->appendTypedBlock<StereoEnhancer>();
  auto& width_mod          = stereoenh->getParam(0)._mods;
  auto WIDTHCONTROL        = fxlayer->appendController<CustomControllerData>("PAN");
  width_mod._src1          = WIDTHCONTROL;
  width_mod._src1Depth     = 1.0;
  WIDTHCONTROL->_oncompute = [](CustomControllerInst* cci) { //
    cci->_curval = 0.7f;
  };
  //
  mainbus->setBusDSP(fxlayer);
  //////////////////////////////////////////////////////////////////////////////
  auto scope    = create_oscilloscope(app->_hudvp);
  auto analyzer = create_spectrumanalyzer(app->_hudvp);
  scope->setRect(-10, 0, 1300, 256, true);
  analyzer->setRect(-10, 268, 1300, 720 - 256, true);
  //////////////////////////////////////////////////////////////////////////////
  auto basepath = basePath() / "casioCZ";
  auto czdata1  = CzData::load(basepath / "factoryA.bnk", "bank1");
  auto czdata2  = CzData::load(basepath / "factoryB.bnk", "bank2");
  // czdata->loadBank(basepath / "edit.syx", "bank1");
  int count = 0;
  for (int i = 0; i < 64; i++) { // 2 32 patch banks
    auto bnk       = (i >> 5) ? czdata2 : czdata1;
    auto prg       = bnk->getProgram(i % 32);
    auto layerdata = prg->getLayer(0);
    auto source    = layerdata->createScopeSource();
    source->connect(scope->_sink);
    source->connect(analyzer->_sink);
    for (int n = 0; n <= 24; n += 3) {
      enqueue_audio_event(prg, count * 0.5, 0.5, 48 + n);
      count++;
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->exec();
  return 0;
}
