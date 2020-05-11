#include "harness.h"
#include <ork/lev2/aud/singularity/cz101.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>

using namespace ork::audio::singularity;

int main(int argc, char** argv) {
  auto qtapp    = createEZapp(argc, argv);
  auto basepath = file::Path::share_dir() / "singularity" / "casioCZ";
  startupAudio();
  the_synth->resetFenables();
  //////////////////////////////////////////////////////////////////////////////
  auto program = std::make_shared<programData>();
  auto czdata  = std::make_shared<CzProgData>();
  auto zpmKM   = std::make_shared<keymap>();
  auto CB0     = std::make_shared<controlBlockData>();
  auto AE      = std::make_shared<RateLevelEnvData>();
  //////////////////////////////////////
  program->_role = "czx";
  program->_name = "test";
  czdata->_name  = "test";
  zpmKM->_name   = "CZX";
  zpmKM->_kmID   = 1;
  //_zpmDB->_keymaps[1] = zpmKM;
  //////////////////////////////////////
  auto ld               = program->newLayer();
  ld->_keymap           = zpmKM.get();
  ld->_kmpBlock._keymap = zpmKM.get();
  ld->_ctrlBlocks[0]    = CB0.get();
  CB0->_cdata[0]        = AE.get();
  AE->_name             = "AMPENV";
  AE->_ampenv           = true;
  AE->_segments.push_back({0, 1}); // atk1
  AE->_segments.push_back({0, 0}); // atk2
  AE->_segments.push_back({0, 0}); // atk3
  AE->_segments.push_back({0, 0}); // dec
  AE->_segments.push_back({2, 0}); // rel1
  AE->_segments.push_back({0, 0}); // rel2
  AE->_segments.push_back({0, 0}); // rel3

  auto osc = ld->appendDspBlock();
  auto amp = ld->appendDspBlock();
  CZX::initBlock(osc, czdata);
  AMP::initBlock(amp);
  ld->_envCtrlData._useNatEnv = false;
  ld->_algData._algID         = 1;
  ld->_algData._name          = "ALG1";
  enqueue_audio_event(program.get(), 0.0f, 60.0, 48);
  //////////////////////////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, 0});
  qtapp->exec();
  tearDownAudio();
  return 0;
}
