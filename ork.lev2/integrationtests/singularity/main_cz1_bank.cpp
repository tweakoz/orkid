////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/fxgen.h>

int main(int argc, char** argv,char**envp) {
  auto initdata = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto app = createEZapp(initdata);
  ////////////////////////////////////////////////
  // main bus effect
  ////////////////////////////////////////////////
  synth::instance()->_masterGain = decibel_to_linear_amp_ratio(-6.0f);
  auto mainbus      = synth::instance()->outputBus("main");
  auto bussource    = mainbus->createScopeSource();
  //auto fxprog       = std::make_shared<ProgramData>();
  //auto fxlayer      = fxprog->newLayer();
  //auto fxalg        = std::make_shared<AlgData>();
  //fxlayer->_algdata = fxalg;
  //fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  if (1) { // create mixbus effect ?
     auto fxlayer = fxpreset_fdn4reverb();
    mainbus->setBusDSP(fxlayer);
  }
  ////////////////////////////////////////////////
  // create visualizers
  ////////////////////////////////////////////////
  ui::anchor::Bounds nobounds;
  auto scope1    = create_oscilloscope(app->_hudvp, nobounds, "dco-1");
  auto scope2    = create_oscilloscope(app->_hudvp, nobounds, "dco-2");
  auto scope3    = create_oscilloscope(app->_hudvp, nobounds, "layer");
  auto analyzer1 = create_spectrumanalyzer(app->_hudvp, nobounds, "dco-1");
  auto analyzer2 = create_spectrumanalyzer(app->_hudvp, nobounds, "dco-2");
  auto analyzer3 = create_spectrumanalyzer(app->_hudvp, nobounds, "layer");
  scope1->setRect(-10, 0, 480, 240, true);
  scope2->setRect(-10, 240, 480, 240, true);
  scope3->setRect(-10, 480, 480, 240, true);
  analyzer1->setRect(480, 0, 810, 240, true);
  analyzer2->setRect(480, 240, 810, 240, true);
  analyzer3->setRect(480, 480, 810, 240, true);
  //////////////////////////////////////////////////////////////////////////////
  auto basepath = basePath() / "casioCZ";
  auto czdata1  = CzData::load(basepath / "factoryA.bnk", "bank1");
  auto czdata2  = CzData::load(basepath / "factoryB.bnk", "bank2");
  // auto czdata1  = CzData::load(basepath / "cz1_1.bnk", "bank1");
  // auto czdata2  = CzData::load(basepath / "cz1_2.bnk", "bank2");
  // czdata->loadBank(basepath / "edit.syx", "bank1");

  //////////////////////////////////////////////////////////////////////////////
  // auto program   = bank->getProgramByName("LatelyBass");
  auto program = testpattern(czdata1, argc, argv);
  if (!program) {
    return 0;
  }


  int count = 0;
  for (int i = 0; i < 64; i++) { // 2 32 patch banks
    auto bnk       = (i >> 5) ? czdata2 : czdata1;
    auto prg       = bnk->getProgram(i % 32);
    auto layerdata = prg->getLayer(0);

    synth::instance()->_globalbank  = bnk->_bankdata;
    synth::instance()->_globalprgit = synth::instance()->_globalbank->_programs.begin();

    //////////////////////////////////////
    // connect DCO's to scopes 1&2
    //////////////////////////////////////
    auto dcostage = layerdata->stageByName("DCO");
    auto dco0     = dcostage->_blockdatas[0];
    auto dco1     = dcostage->_blockdatas[1];
    if (dco0) {
      auto dco0_source         = dco0->createScopeSource();
      dco0_source->_dspchannel = 0;
      dco0_source->connect(scope1->_sink);
      dco0_source->connect(analyzer1->_sink);
    }
    if (dco1) {
      auto dco1_source         = dco1->createScopeSource();
      dco1_source->_dspchannel = 1;
      dco1_source->connect(scope2->_sink);
      dco1_source->connect(analyzer2->_sink);
    }
    //////////////////////////////////////
    // connect layerout to scope 3
    //////////////////////////////////////
    auto layersource = layerdata->createScopeSource();
    layersource->connect(scope3->_sink);
    layersource->connect(analyzer3->_sink);
    //////////////////////////////////////
  }
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->mainThreadLoop();
  return 0;
}
