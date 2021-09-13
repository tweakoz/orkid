#include "harness.h"
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/fxgen.h>

using namespace ork;
using namespace ork::lev2;
using namespace ork::reflect::serdes;
using namespace ork::audio::singularity;

int main(int argc, char** argv) {
  auto app                       = createEZapp(argc, argv);
  synth::instance()->_masterGain = decibel_to_linear_amp_ratio(20.0f);
  ////////////////////////////////////////////////
  // main bus effect
  ////////////////////////////////////////////////
  auto mainbus   = synth::instance()->outputBus("main");
  auto bussource = mainbus->createScopeSource();
    //////////////////////////////////////////////////////////////////////////////
  // allocate program/layer data
  //////////////////////////////////////////////////////////////////////////////
  auto bank                              = std::make_shared<Tx81zData>();
  auto program                           = std::make_shared<ProgramData>();
  bank->_bankdata->_programs[0]          = program;
  bank->_bankdata->_programsByName["yo"] = program;
  auto layerdata                         = program->newLayer();
  auto prgdata81z                        = std::make_shared<Tx81zProgData>();
  program->_tags                         = "fm4";
  program->_name                         = "test";
  prgdata81z->_alg                       = 0;
  auto& opd0                             = prgdata81z->_ops[0];
  auto& opd1                             = prgdata81z->_ops[1];
  auto& opd2                             = prgdata81z->_ops[2];
  auto& opd3                             = prgdata81z->_ops[3];
  //////////////////////////////////////
  // setup dsp graph
  //////////////////////////////////////
  configureTx81zAlgorithm(layerdata, prgdata81z);
  auto ops_stage = layerdata->stageByName("OPS");
  auto op0       = ops_stage->_blockdatas[3];
  auto op1       = ops_stage->_blockdatas[2];
  auto op2       = ops_stage->_blockdatas[1];
  auto op3       = ops_stage->_blockdatas[0];
  //////////////////////////////////////
  op0->param(0)->_coarse   = 6000.0f; // op0 pitch
  op1->param(0)->_coarse   = 7200.0f; // op1 pitch
  op2->param(0)->_coarse   = 7200.0f; // op2 pitch
  op3->param(0)->_coarse   = 8400.0f; // op3 pitch
  op0->param(0)->_keyTrack = -100.0f; // op0 pitch keytrack
  op1->param(0)->_keyTrack = 100.0f;  // op1 pitch keytrack
  op2->param(0)->_keyTrack = 100.0f;  // op2 pitch keytrack
  op3->param(0)->_keyTrack = 100.0f;  // op3 pitch keytrack
  //////////////////////////////////////
  op0->param(1)->_coarse = 0.0f; // op0 amp
  op1->param(1)->_coarse = 0.0f; // op1 amp
  op2->param(1)->_coarse = 0.0f; // op2 amp
  op3->param(1)->_coarse = 0.0f; // op3 amp
  //////////////////////////////////////
  op3->param(2)->_coarse = 1.0f; // feedback = 2PI

  //////////////////////////////////////
  // setup modulators
  //////////////////////////////////////
  auto modop = [layerdata](
                   std::string opname, //
                   dspblkdata_ptr_t op) -> controllerdata_ptr_t {
    auto pitch_param             = op->param(0);
    auto amp_param               = op->param(1);
    auto feedback_param          = op->param(2);
    auto envname                 = FormatString("%sENV", opname.c_str());
    auto ENV                     = layerdata->appendController<YmEnvData>(envname);
    amp_param->_mods->_src1      = ENV;
    amp_param->_mods->_src1Depth = 1.0;
    return ENV;
  };
  auto op0env = modop("OP0", op0);
  auto op1env = modop("OP1", op1);
  auto op2env = modop("OP2", op2);
  auto op3env = modop("OP3", op3);
  ////////////////////////////////////////////////
  // UI layout
  ////////////////////////////////////////////////
  auto toplayout = app->_hudvp->_layout;
  auto guidehl   = toplayout->left();
  auto guidehc   = toplayout->centerH();
  auto guidehr   = toplayout->right();
  auto guidevt   = toplayout->top();
  auto guidev0   = toplayout->proportionalHorizontalGuide(1.0f / 6.0f);
  auto guidev1   = toplayout->proportionalHorizontalGuide(2.5f / 6.0f);
  auto guidev2   = toplayout->proportionalHorizontalGuide(3.0f / 6.0f);
  auto guidev3   = toplayout->proportionalHorizontalGuide(4.5f / 6.0f);
  auto guidev4   = toplayout->proportionalHorizontalGuide(5.0f / 6.0f);
  auto guidehd   = toplayout->proportionalVerticalGuide(0.75);
  auto guidevb   = toplayout->bottom();

  ui::anchor::Bounds top_left;
  ui::anchor::Bounds top_right;

  top_left._top    = guidevt;
  top_left._left   = guidehl;
  top_left._bottom = guidev0;
  top_left._right  = guidehd;

  top_right._top    = guidevt;
  top_right._left   = guidehd;
  top_right._bottom = guidev0;
  top_right._right  = guidehr;
  //////////////////////////////////////
  // create and connect oscilloscope
  //////////////////////////////////////
  auto progview = createProgramView(app->_hudvp, top_left, "program");
  auto perfview = createProfilerView(app->_hudvp, top_right, "profiler");
  auto color0   = fvec4(0.7, 0.1, 0.5, 1);
  auto color1   = fvec4(0.1, 0.7, 0.5, 1);
  auto color2   = fvec4(0.1, 0.5, 0.7, 1);
  auto color3   = fvec4(0.5, 0.5, 0.5, 1);
  auto pmxedit0 = createPmxEditView(
      app->_hudvp, //
      "pmxedit0",
      color0,
      op0,
      {guidev0, guidehl, guidev1, guidehc, 2});
  auto pmxedit1 = createPmxEditView(
      app->_hudvp, //
      "pmxedit1",
      color1,
      op1,
      {guidev0, guidehc, guidev1, guidehr, 2});
  auto envedit0 = createEnvYmEditView(
      app->_hudvp, //
      "envedit0",
      color0,
      op0env,
      {guidev1, guidehl, guidev2, guidehc, 2});
  auto envedit1 = createEnvYmEditView(
      app->_hudvp, //
      "envedit1",
      color1,
      op1env,
      {guidev1, guidehc, guidev2, guidehr, 2});

  auto pmxedit2 = createPmxEditView(
      app->_hudvp, //
      "pmxedit2",
      color2,
      op2,
      {guidev2, guidehl, guidev3, guidehc, 2});
  auto pmxedit3 = createPmxEditView(
      app->_hudvp, //
      "pmxedit3",
      color3,
      op3,
      {guidev2, guidehc, guidev3, guidehr, 2});
  auto envedit2 = createEnvYmEditView(
      app->_hudvp, //
      "envedit2",
      color2,
      op2env,
      {guidev3, guidehl, guidev4, guidehc, 2});
  auto envedit3 = createEnvYmEditView(
      app->_hudvp, //
      "envedit3",
      color3,
      op3env,
      {guidev3, guidehc, guidev4, guidehr, 2});
  auto source   = layerdata->createScopeSource();
  auto scope    = create_oscilloscope(app->_hudvp, {guidev4, guidehl, guidevb, guidehc, 2});
  auto analyzer = create_spectrumanalyzer(app->_hudvp, {guidev4, guidehc, guidevb, guidehr, 2});
  source->connect(scope->_sink);
  source->connect(analyzer->_sink);
  //////////////////////////////////////
  // play test notes
  //////////////////////////////////////
  synth::instance()->_globalbank  = bank->_bankdata;
  synth::instance()->_globalprgit = bank->_bankdata->_programs.begin();
  // enqueue_audio_event(program, 1.5f, 240.0, 48);
  //////////////////////////////////////////////////////////////////////////////
  auto __program = testpattern(bank, argc, argv);
  if (!__program) {
    return 0;
  }
  //////////////////////////////////////////////////////////////////////////////
  // test harness UI
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->runloop();
  return 0;
}
