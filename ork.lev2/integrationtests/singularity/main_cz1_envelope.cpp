////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>

using namespace ork::audio::singularity;

int main(int argc, char** argv,char**envp) {
  auto initdata = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto app = createEZapp(initdata);
  prgdata_constptr_t program;
  lyrdata_ptr_t layerdata;
  bool do_from_bank = true;
  //////////////////////////////////////////////////////////////////////////////
  // allocate program/layer data
  //////////////////////////////////////////////////////////////////////////////
  if (do_from_bank) {
    auto basepath = basePath() / "casioCZ";
    auto czdataa  = CzData::load(basepath / "factoryA.bnk", "bank1");
    auto czdatab  = CzData::load(basepath / "factoryB.bnk", "bank2");
    auto bnk      = czdataa->_bankdata;
    program       = bnk->findProgramByName("BRASS 2");
    layerdata     = program->getLayer(0);
  } else {
    auto mutable_program   = std::make_shared<ProgramData>();
    layerdata              = mutable_program->newLayer();
    auto czoscdata         = std::make_shared<CzOscData>();
    mutable_program->_tags = "czx";
    mutable_program->_name = "test";
    program                = mutable_program;
    //////////////////////////////////////
    // setup dsp graph
    //////////////////////////////////////
    auto czldc = configureCz1Algorithm(layerdata, 1);
    layerdata->_algdata = czldc._algdata;
    auto dcostage       = layerdata->stageByName("DCO");
    auto ampstage       = layerdata->stageByName("AMP");
    auto dco            = dcostage->appendTypedBlock<CZX>("dco", czoscdata, 0);
    auto amp            = ampstage->appendTypedBlock<AMP_MONOIO>("amp");
    //////////////////////////////////////
    // setup envelope
    //////////////////////////////////////
    auto DCAENV     = layerdata->appendController<RateLevelEnvData>("DCAENV0");
    DCAENV->_ampenv = true;
    DCAENV->addSegment("seg0", .25, .25, 0.5f);
    DCAENV->addSegment("seg1", .25, .5, 1.5f);
    DCAENV->addSegment("seg2", .25, .75, 0.5f);
    DCAENV->addSegment("seg3", .25, 1.0, -1.65f);
    DCAENV->addSegment("seg4", 1, 1);
    DCAENV->addSegment("seg5", 3, .75, 0.5f);
    DCAENV->addSegment("seg6", 3, .5, -9.0f);
    DCAENV->addSegment("seg7", 3, .25, 4.0f);
    DCAENV->addSegment("seg8", 3, 0, 0.25f);
    //////////////////////////////////////
    auto DCWENV = layerdata->appendController<RateLevelEnvData>("DCWENV0");
    DCWENV->addSegment("seg0", 0.1, .7);
    DCWENV->addSegment("seg1", 1, 1);
    DCWENV->addSegment("seg2", 2, .5);
    DCWENV->addSegment("seg3", 2, 1);
    DCWENV->addSegment("seg4", 2, 1);
    DCWENV->addSegment("seg5", 40, 1);
    DCWENV->addSegment("seg6", 40, 0);
    DCWENV->_ampenv = false;
    //////////////////////////////////////
    auto DCOENV = layerdata->appendController<RateLevelEnvData>("DCOENV0");
    DCOENV->addSegment("seg0", 0.1, 1200);
    DCOENV->addSegment("seg1", 4, 900);
    DCOENV->addSegment("seg2", 3, 700);
    DCOENV->addSegment("seg3", 2, 120);
    DCOENV->addSegment("seg4", 2, 60);
    DCOENV->addSegment("seg5", 1, 1);
    DCOENV->addSegment("seg6", 1, 0);
    DCOENV->_ampenv = false;
    //////////////////////////////////////
    // setup LFO
    //////////////////////////////////////
    auto LFO1      = layerdata->appendController<LfoData>("MYLFO1");
    LFO1->_minRate = 0.25;
    LFO1->_maxRate = 0.25;
    LFO1->_shape   = "Sine";
    auto LFO2      = layerdata->appendController<LfoData>("MYLFO2");
    LFO2->_minRate = 3.3;
    LFO2->_maxRate = 3.3;
    LFO2->_shape   = "Sine";
    //////////////////////////////////////
    // setup modulation routing
    //////////////////////////////////////
    auto pitch_mod        = dco->_paramd[0]->_mods;
    pitch_mod->_src1      = DCOENV;
    pitch_mod->_src1Depth = 1.0f;
    //////////////////////////////////////
    auto modulation_index_param        = dco->_paramd[1]->_mods;
    modulation_index_param->_src1      = DCWENV;
    modulation_index_param->_src1Depth = 1.0;
    // modulation_index_param->_src2      = LFO1;
    // modulation_index_param->_src2DepthCtrl = LFO2;
    modulation_index_param->_src2MinDepth = 0.5;
    modulation_index_param->_src2MaxDepth = 0.1;
    //////////////////////////////////////
    czoscdata->_dcoBaseWaveA = 6;
    czoscdata->_dcoBaseWaveB = 7;
    czoscdata->_dcoWindow    = 2;
    //////////////////////////////////////
    auto amp_param     = amp->_paramd[0];
    amp_param->_coarse = 0.0f;
    amp_param->useDefaultEvaluator();
    amp_param->_mods->_src1      = DCAENV;
    amp_param->_mods->_src1Depth = 1.0;
  }
  //////////////////////////////////////
  // create and connect oscilloscope
  //////////////////////////////////////
  ui::anchor::Bounds nobounds;
  auto source   = layerdata->createScopeSource();
  auto scope    = create_oscilloscope(app->_hudvp, nobounds);
  auto analyzer = create_spectrumanalyzer(app->_hudvp, nobounds);
  source->connect(scope->_sink);
  source->connect(analyzer->_sink);
  scope->setRect(0, 0, 480, 256, true);
  analyzer->setRect(480, 0, 810, 256, true);
  //////////////////////////////////////
  // envelope viewer
  //////////////////////////////////////
  auto ctrlblk = layerdata->_ctrlBlock;
  controllerdata_ptr_t inspect_env = ctrlblk->controllerByName("DCWENV1");
  auto env_source                  = inspect_env->createScopeSource();
  auto envview                     = create_envelope_analyzer(app->_hudvp, nobounds);
  env_source->connect(envview->_sink);
  envview->setRect(-10, 720 - 467, 1300, 477, true);
  envview->setProperty<float>("timewidth", 1.0f);
  //////////////////////////////////////
  // play test notes
  //////////////////////////////////////
  if (do_from_bank) {
    for (int i = 0; i < 36; i++)
      enqueue_audio_event(program, 1.0f + (i * 3), 3.0, 48);
  } else {
    enqueue_audio_event(program, 1.5f, 240.0, 48);
  }
  //////////////////////////////////////////////////////////////////////////////
  // test harness UI
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->mainThreadLoop();
  return 0;
}
