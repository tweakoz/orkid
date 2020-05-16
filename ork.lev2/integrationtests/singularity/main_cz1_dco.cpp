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
  auto program   = std::make_shared<ProgramData>();
  auto layerdata = program->newLayer();
  auto czdata    = std::make_shared<CzOscData>();
  auto keymap    = std::make_shared<KeyMap>();
  auto AE        = layerdata->appendController<RateLevelEnvData>("MYENV");
  auto LFO1      = layerdata->appendController<LfoData>("MYLFO1");
  auto LFO2      = layerdata->appendController<LfoData>("MYLFO2");
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
  layerdata->_algdata->_krzAlgIndex  = 1;
  layerdata->_algdata->_name         = "CZTEST";
  layerdata->_keymap                 = keymap.get();
  layerdata->_kmpBlock._keymap       = keymap.get();
  layerdata->_envCtrlData._useNatEnv = false;
  //////////////////////////////////////
  // set envelope
  //////////////////////////////////////
  AE->_ampenv = true;
  AE->_segments.push_back({.3, .7}); // atk1
  AE->_segments.push_back({1, 1});   // atk2
  AE->_segments.push_back({1, .3});  // atk3
  AE->_segments.push_back({1, .5});  // dec
  AE->_segments.push_back({2, 0});   // rel1
  AE->_segments.push_back({0, 0});   // rel2
  AE->_segments.push_back({0, 0});   // rel3
  //////////////////////////////////////
  // set LFO
  //////////////////////////////////////
  LFO1->_minRate = 0.25;
  LFO1->_maxRate = 0.25;
  LFO1->_shape   = "Sine";
  LFO2->_minRate = 3.3;
  LFO2->_maxRate = 3.3;
  LFO2->_shape   = "Sine";
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
  auto& modulation_index_param      = osc->_paramd[1]._mods;
  modulation_index_param._src1      = "MYENV";
  modulation_index_param._src1Depth = 1.0;
  modulation_index_param._src2      = "MYLFO1";
  // modulation_index_param._src2DepthCtrl = "MYLFO2";
  modulation_index_param._src2MinDepth = 0.5;
  modulation_index_param._src2MaxDepth = 0.1;
  //////////////////////////////////////
  // play a test note
  //////////////////////////////////////
  enqueue_audio_event(program.get(), 1.5f, 240.0, 48);
  //////////////////////////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, 0});
  qtapp->exec();
  tearDownAudio();
  return 0;
}
