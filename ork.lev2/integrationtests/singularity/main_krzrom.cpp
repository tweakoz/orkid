////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "harness.h"
#include <ork/lev2/aud/singularity/krzdata.h>

int main(int argc, char** argv,char**envp) {

  auto initdata = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto app      = createEZapp(initdata);
  auto bank     = std::make_shared<KrzSynthData>();
  auto drums    = bank->getProgramByName("Castle_Drums");
  auto doomsday = bank->getProgramByName("Doomsday");
  auto ceetuar  = bank->getProgramByName("Cee_Tuar");
  auto piano    = bank->getProgramByName("Grand_Piano");
  auto winds    = bank->getProgramByName("Northern_Winds");
  auto sweep    = bank->getProgramByName("Hi_Res_Sweeper");
  //////////////////////////////////////////////////////////////////////////////
  enqueue_audio_event(winds, 0.0, 50.0, 48);
  enqueue_audio_event(sweep, 0.0, 30.0, 36);
  enqueue_audio_event(sweep, 1.0, 30.0, 48);
  enqueue_audio_event(sweep, 2.0, 30.0, 60);
  enqueue_audio_event(doomsday, 1.0, 20.0, 36);
  enqueue_audio_event(piano, 10.0, 10.0, 36);
  enqueue_audio_event(piano, 10.2, 10.0, 48);
  enqueue_audio_event(doomsday, 20.0, 20.0, 48);
  enqueue_audio_event(doomsday, 30.0, 20.0, 60);
  enqueue_audio_event(ceetuar, 1.0, 5.0, 36);
  enqueue_audio_event(ceetuar, 2.0, 5.0, 43);
  enqueue_audio_event(ceetuar, 3.0, 5.0, 36);
  enqueue_audio_event(ceetuar, 4.0, 5.0, 48);
  for (int i = 0; i < 60; i++) {
    float t = float(i) * 1.0;
    enqueue_audio_event(drums, t, 1.0, 48);
  }
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->mainThreadLoop();
  return 0;
}
