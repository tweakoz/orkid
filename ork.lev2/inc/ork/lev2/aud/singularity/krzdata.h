#pragma once

#include "krztypes.h"
#include "modulation.h"
#include "sf2.h"
#include <ork/kernel/svariant.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

struct ProgramData {
  lyrdata_ptr_t newLayer();
  lyrdata_ptr_t getLayer(int i) const {
    return _LayerDatas[i];
  }
  std::string _name;
  std::string _role;
  std::vector<lyrdata_ptr_t> _LayerDatas;
};

///////////////////////////////////////////////////////////////////////////////

struct programInst;

struct SynthData {
  SynthData();
  virtual ~SynthData() {
  }

  float seqTime(float dur);
  virtual const ProgramData* getProgram(int progID) const                     = 0;
  virtual const ProgramData* getProgramByName(const std::string& named) const = 0;

  programInst* _prog;
  float _synsr;
  float _seqCursor;
  std::string _staticBankName;
};

///////////////////////////////////////////////////////////////////////////////

struct KrzSynthData : public SynthData {
  static VastObjectsDB* baseObjects();

  KrzSynthData();
  const ProgramData* getProgram(int progID) const final;
  const ProgramData* getProgramByName(const std::string& named) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct Sf2TestSynthData : public SynthData {
  Sf2TestSynthData(const file::Path& syxpath, const std::string& bankname = "sf2");
  ~Sf2TestSynthData();
  sf2::SoundFont* _sfont;
  const ProgramData* getProgram(int progID) const final;
  const ProgramData* getProgramByName(const std::string& named) const final {
    return nullptr;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct Tx81zData : public SynthData {
  Tx81zData();
  ~Tx81zData();
  void loadBank(const file::Path& syxpath);

  const ProgramData* getProgram(int progID) const final;
  const ProgramData* getProgramByName(const std::string& named) const final {
    return nullptr;
  }
  VastObjectsDB* _zpmDB;
  keymap_ptr_t _zpmKM;
  int _lastprg;
};

///////////////////////////////////////////////////////////////////////////////

struct CzData : public SynthData {
  CzData();
  ~CzData();
  void loadBank(const file::Path& syxpath, const std::string& bnkname = "czb");

  const ProgramData* getProgram(int progID) const final;
  const ProgramData* getProgramByName(const std::string& named) const final {
    return nullptr;
  }
  VastObjectsDB* _zpmDB;
  keymap_ptr_t _zpmKM;
  int _lastprg;
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
