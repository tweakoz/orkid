////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "harness.h"
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/kernel/string/string.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/environment.h>

using namespace ork;
using namespace ork::lev2;
using namespace ork::reflect::serdes;
using namespace ork::audio::singularity;

int main(int argc, char** argv, char** envp) {
  auto initdata = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto app = createEZapp(initdata);
  Environment env;

  OrkAssert(env.has("ORKID_WORKSPACE_DIR"));

  std::string orkdirstr;
  env.get("ORKID_WORKSPACE_DIR", orkdirstr);
  auto orkdir = file::Path(orkdirstr);

  auto inppath = orkdir               //
                 / "ork.lev2"         //
                 / "integrationtests" //
                 / "singularity"      //
                 / "serdes_test1.json";

  File jsonfile(inppath, EFM_READ);
  size_t length = 0;
  jsonfile.GetLength(length);
  std::string jsonstr;
  jsonstr.resize(length + 1);
  jsonfile.Read(jsonstr.data(), length);
  jsonstr.data()[length] = 0;

  // printf("%s\n", jsonstr.c_str());
  object_ptr_t instance_out;
  JsonDeserializer deser(jsonstr);
  deser.deserializeTop(instance_out);

  auto bankdata = std::dynamic_pointer_cast<BankData>(instance_out);
  auto prgdata  = bankdata->findProgramByName("test");
  auto lyrdata  = prgdata->getLayer(0);

  auto ops_stage = lyrdata->stageByName("OPS");
  auto op0       = ops_stage->_blockdatas[3];
  auto op1       = ops_stage->_blockdatas[2];
  auto op2       = ops_stage->_blockdatas[1];
  auto op3       = ops_stage->_blockdatas[0];
  auto op0env    = lyrdata->controllerByName("OP0ENV");
  auto op1env    = lyrdata->controllerByName("OP1ENV");
  auto op2env    = lyrdata->controllerByName("OP2ENV");
  auto op3env    = lyrdata->controllerByName("OP3ENV");
  ////////////////////////////////////////////////
  // main bus effect
  ////////////////////////////////////////////////
  auto mainbus   = synth::instance()->outputBus("main");
  auto bussource = mainbus->createScopeSource();
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
  auto source   = lyrdata->createScopeSource();
  auto scope    = create_oscilloscope(app->_hudvp, {guidev4, guidehl, guidevb, guidehc, 2});
  auto analyzer = create_spectrumanalyzer(app->_hudvp, {guidev4, guidehc, guidevb, guidehr, 2});
  source->connect(scope->_sink);
  source->connect(analyzer->_sink);
  //////////////////////////////////////
  // play test notes
  //////////////////////////////////////
  synth::instance()->_globalbank  = bankdata;
  synth::instance()->_globalprgit = bankdata->_programs.begin();
  // enqueue_audio_event(program, 1.5f, 240.0, 48);
  //////////////////////////////////////////////////////////////////////////////
  auto syndata       = std::make_shared<SynthData>();
  syndata->_bankdata = bankdata;

  auto __program = testpattern(syndata, argc, argv);
  if (!__program) {
    return 0;
  }
  //////////////////////////////////////////////////////////////////////////////
  // test harness UI
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->mainThreadLoop();
  return 0;
}
