#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>

int main(int argc, char** argv) {
  auto qtapp    = createEZapp(argc, argv);
  auto basepath = basePath() / "casioCZ";
  //////////////////////////////////////////////////////////////////////////////
  auto bank    = CzData::load(basepath / "factoryA.bnk", "bank1");
  auto program = testpattern(bank, argc, argv);
  if (!program) {
    return 0;
  }
  //////////////////////////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, 0});
  qtapp->exec();
  return 0;
}
