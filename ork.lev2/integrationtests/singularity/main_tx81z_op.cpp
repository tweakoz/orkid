#include "harness.h"
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>

using namespace ork::audio::singularity;

int main(int argc, char** argv) {
  auto app = createEZapp(argc, argv);
  //////////////////////////////////////////////////////////////////////////////
  // allocate program/layer data
  //////////////////////////////////////////////////////////////////////////////
  auto program      = std::make_shared<ProgramData>();
  auto layerdata    = program->newLayer();
  auto fmdata       = std::make_shared<Fm4ProgData>();
  program->_role    = "fm4";
  program->_name    = "test";
  fmdata->_alg      = 0;
  fmdata->_feedback = 7.0f;
  auto& op0         = fmdata->_ops[0];
  auto& op1         = fmdata->_ops[1];
  auto& op2         = fmdata->_ops[2];
  auto& op3         = fmdata->_ops[3];
  op0._opEnable     = true;
  op1._opEnable     = true;
  op2._opEnable     = true;
  op3._opEnable     = true;
  op0._fixedFrqMode = false;
  op1._fixedFrqMode = false;
  op2._fixedFrqMode = false;
  op3._fixedFrqMode = false;
  op0._frqRatio     = 1.0;
  op1._frqRatio     = 2.0;
  op2._frqRatio     = 2.01;
  op3._frqRatio     = 3.04;
  op0._outLevel     = 99.0f;
  op1._outLevel     = 99.0f;
  op2._outLevel     = 99.0f;
  op3._outLevel     = 99.0f;
  op1._modIndex     = 0.125f;
  op2._modIndex     = 0.125f;
  op3._modIndex     = 0.125f;
  //////////////////////////////////////
  // setup dsp graph
  //////////////////////////////////////
  configureTx81zAlgorithm(layerdata, fmdata);
  auto ops_stage        = layerdata->stageByName("OPS");
  auto ops              = ops_stage->_blockdatas[0];
  ops->param(0)._coarse = 1.0f; // op0 amp
  ops->param(1)._coarse = 1.0f; // op1 amp
  ops->param(2)._coarse = 1.0f; // op2 amp
  ops->param(3)._coarse = 1.0f; // op3 amp
  // auto ampstage       = layerdata->stageByName("AMP");
  // auto osc            = dcostage->appendTypedBlock<CZX>(czoscdata, 0);
  // auto amp            = ampstage->appendTypedBlock<AMP_MONOIO>();
  //////////////////////////////////////
  // setup modulators
  //////////////////////////////////////
  // auto DCAENV = layerdata->appendController<RateLevelEnvData>("DCAENV");
  // auto DCWENV = layerdata->appendController<RateLevelEnvData>("DCWENV");
  // auto LFO2   = layerdata->appendController<LfoData>("MYLFO2");
  // auto LFO1   = layerdata->appendController<LfoData>("MYLFO1");
  //////////////////////////////////////
  // setup envelope
  //////////////////////////////////////
  // DCAENV->_ampenv = true;
  // DCAENV->addSegment("seg0", .2, .7);
  // DCAENV->addSegment("seg1", .2, .7);
  // DCAENV->addSegment("seg2", 1, 1);
  // DCAENV->addSegment("seg3", 120, .3);
  // DCAENV->addSegment("seg4", 120, 0);
  //
  // DCWENV->_ampenv = false;
  // DCWENV->addSegment("seg0", 0.1, .7);
  // DCWENV->addSegment("seg1", 1, 1);
  // DCWENV->addSegment("seg2", 2, .5);
  // DCWENV->addSegment("seg3", 2, 1);
  // DCWENV->addSegment("seg4", 2, 1);
  // DCWENV->addSegment("seg5", 40, 1);
  // DCWENV->addSegment("seg6", 40, 0);
  //////////////////////////////////////
  // create and connect oscilloscope
  //////////////////////////////////////
  auto source   = layerdata->createScopeSource();
  auto scope    = create_oscilloscope(app->_hudvp);
  auto analyzer = create_spectrumanalyzer(app->_hudvp);
  source->connect(scope->_sink);
  source->connect(analyzer->_sink);
  scope->setRect(0, 0, 1280, 256);
  analyzer->setRect(0, 720 - 256, 1280, 256);
  //////////////////////////////////////
  // play test notes
  //////////////////////////////////////
  enqueue_audio_event(program, 1.5f, 240.0, 48);
  //////////////////////////////////////////////////////////////////////////////
  // test harness UI
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->exec();
  return 0;
}
