#include "harness.h"
#include <ork/lev2/aud/singularity/cz101.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>

using namespace ork::audio::singularity;

int main(int argc, char** argv) {
  auto qtapp = createEZapp(argc, argv);
  startupAudio();
  //////////////////////////////////////////////////////////////////////////////
  // allocate components
  //////////////////////////////////////////////////////////////////////////////
  auto program = std::make_shared<ProgramData>();
  auto czdata  = std::make_shared<CzOscData>();
  auto keymap  = std::make_shared<KeyMap>();
  auto CB0     = std::make_shared<ControlBlockData>();
  auto AE      = std::make_shared<RateLevelEnvData>();
  //////////////////////////////////////
  // set names
  //////////////////////////////////////
  program->_role = "czx";
  program->_name = "test";
  keymap->_name  = "CZX";
  keymap->_kmID  = 1;
  //_zpmDB->_keymaps[1] = keymap;
  //////////////////////////////////////
  // create layer
  //////////////////////////////////////
  auto layerdata                     = program->newLayer();
  layerdata->_algdata->_krzAlgIndex  = 1;
  layerdata->_algdata->_name         = "CZTEST";
  layerdata->_keymap                 = keymap.get();
  layerdata->_kmpBlock._keymap       = keymap.get();
  layerdata->_ctrlBlocks[0]          = CB0.get();
  layerdata->_envCtrlData._useNatEnv = false;
  //////////////////////////////////////
  // set envelope
  //////////////////////////////////////
  CB0->_cdata[0] = AE.get();
  AE->_name      = "AMPENV";
  AE->_ampenv    = true;
  AE->_segments.push_back({.5, .7}); // atk1
  AE->_segments.push_back({1, 1});   // atk2
  AE->_segments.push_back({0, 0});   // atk3
  AE->_segments.push_back({0, 0});   // dec
  AE->_segments.push_back({2, 0});   // rel1
  AE->_segments.push_back({0, 0});   // rel2
  AE->_segments.push_back({0, 0});   // rel3
  //////////////////////////////////////
  // setup dsp graph
  //////////////////////////////////////
  auto stage0 = layerdata->appendStage();
  auto stage1 = layerdata->appendStage();
  stage0->_iomask._inputs.push_back(0);  // 1 input
  stage0->_iomask._outputs.push_back(0); // 1 output
  stage1->_iomask._inputs.push_back(0);  // 1 input
  stage1->_iomask._outputs.push_back(0); // 1 output
  auto osc = stage0->appendBlock();
  auto amp = stage1->appendBlock();
  CZX::initBlock(osc, czdata);
  AMP::initBlock(amp);
  //////////////////////////////////////
  // play a test note
  //////////////////////////////////////
  enqueue_audio_event(program.get(), 1.5f, 60.0, 48);
  //////////////////////////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, 0});
  qtapp->exec();
  tearDownAudio();
  return 0;
}
