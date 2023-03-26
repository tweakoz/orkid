////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>

int main(int argc, char** argv,char**envp) {
  auto initdata = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto app = createEZapp(initdata);
  auto basepath = basePath() / "casioCZ";
  //////////////////////////////////////////////////////////////////////////////
  auto bnk = CzData::load(basepath / "edit.syx", "bank1");
  auto prg = bnk->getProgram(0);

  for (int i = 0; i < 128; i++) {
    int note = 36 + i % 24;
    enqueue_audio_event(prg, 1 + float(i) * 1.0, 1.0, note);
  }
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->mainThreadLoop();
  return 0;
}
