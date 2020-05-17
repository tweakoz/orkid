#pragma once

#include "krztypes.h"
#include "modulation.h"
#include <ork/kernel/svariant.h>
#include "synthdata.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

struct KrzSynthData : public SynthData {
  static SynthObjectsDB* baseObjects();

  KrzSynthData();
  const ProgramData* getProgram(int progID) const final;
  const ProgramData* getProgramByName(const std::string& named) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct KrzTestData : public SynthData {
  KrzTestData();
  void genTestPrograms();
  const ProgramData* getProgram(int progID) const final;
  const ProgramData* getProgramByName(const std::string& named) const final {
    return nullptr;
  }
  std::vector<ProgramData*> _testPrograms;
};

///////////////////////////////////////////////////////////////////////////////

struct KrzKmTestData : public SynthData {
  KrzKmTestData();
  const ProgramData* getProgram(int progID) const final;
  const ProgramData* getProgramByName(const std::string& named) const final {
    return nullptr;
  }
  std::map<int, ProgramData*> _testKmPrograms;
};

} // namespace ork::audio::singularity
