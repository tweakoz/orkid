#pragma once

#include "krztypes.h"
#include "modulation.h"
#include <ork/kernel/svariant.h>
#include "synthdata.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

struct KrzSynthData : public SynthData {
  static bankdata_ptr_t baseObjects();
  KrzSynthData();
};

///////////////////////////////////////////////////////////////////////////////

struct KrzTestData : public SynthData {
  KrzTestData();
  void genTestPrograms();
  std::vector<ProgramData*> _testPrograms;
};

///////////////////////////////////////////////////////////////////////////////

struct KrzKmTestData : public SynthData {
  KrzKmTestData();
  std::map<int, ProgramData*> _testKmPrograms;
};

} // namespace ork::audio::singularity
