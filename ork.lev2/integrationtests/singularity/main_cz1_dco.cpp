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
  auto program = std::make_shared<programData>();
  auto czdata  = std::make_shared<CzProgData>();
  auto keymap  = std::make_shared<KeyMap>();
  auto CB0     = std::make_shared<controlBlockData>();
  auto AE      = std::make_shared<RateLevelEnvData>();
  //////////////////////////////////////
  // set names
  //////////////////////////////////////
  program->_role = "czx";
  program->_name = "test";
  czdata->_name  = "test";
  keymap->_name  = "CZX";
  keymap->_kmID  = 1;
  //_zpmDB->_keymaps[1] = keymap;
  //////////////////////////////////////
  // create layer
  //////////////////////////////////////
  auto layerdat                     = program->newLayer();
  layerdat->_algData._algID         = 1;
  layerdat->_algData._name          = "ALG1";
  layerdat->_keymap                 = keymap.get();
  layerdat->_kmpBlock._keymap       = keymap.get();
  layerdat->_ctrlBlocks[0]          = CB0.get();
  layerdat->_envCtrlData._useNatEnv = false;
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
  auto osc = layerdat->appendDspBlock();
  auto amp = layerdat->appendDspBlock();
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
