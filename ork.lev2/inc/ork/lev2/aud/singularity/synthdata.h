////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "reflection.h"
#include "krztypes.h"
#include "modulation.h"
#include <ork/kernel/svariant.h>
#include <ork/file/path.h>
#include <ork/util/hexdump.inl>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#include <rapidjson/reader.h>
#include <rapidjson/document.h>

#pragma GCC diagnostic pop

using namespace rapidjson;

namespace ork::audio::singularity {
file::Path basePath();

///////////////////////////////////////////////////////////////////////////////

struct ProgramData : public ork::Object {

  DeclareConcreteX(ProgramData, ork::Object);

  lyrdata_ptr_t newLayer();
  inline lyrdata_ptr_t getLayer(int i) const {
    return _layerdatas[i];
  }
  inline void addHudInfo(std::string str) {
    _hudinfos.push_back(str);
  }

  std::string _name;
  std::string _tags;
  std::vector<lyrdata_ptr_t> _layerdatas;
  std::vector<std::string> _hudinfos;
};

///////////////////////////////////////////////////////////////////////////////
struct KrzAlgData;
struct EnvCtrlData;

struct BankData : public ork::Object {

  DeclareConcreteX(BankData, ork::Object);

  void loadJson(const std::string& fname, int bank);

  void addProgram(int idx, const std::string& name, prgdata_ptr_t program);
  prgdata_constptr_t findProgram(int idx) const;
  prgdata_constptr_t findProgramByName(const std::string named) const;
  keymap_constptr_t findKeymap(int kmID) const;

  //

  keymap_ptr_t parseKeymap(int kmid, const rapidjson::Value& JO);
  void parseAsr(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const EnvCtrlData& ENVCTRL, const std::string& name);
  void parseLfo(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const std::string& name);
  void parseFun(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const std::string& name);
  lyrdata_ptr_t parseLayer(const rapidjson::Value& JO, prgdata_ptr_t pd);
  void parseEnvControl(const rapidjson::Value& JO, EnvCtrlData& ed);
  prgdata_ptr_t parseProgram(const rapidjson::Value& JO);
  multisample* parseMultiSample(const rapidjson::Value& JO);
  sample* parseSample(const rapidjson::Value& JO, const multisample* parent);

  KrzAlgData parseAlg(const rapidjson::Value& JO);
  void parseKmpBlock(const Value& JO, KmpBlockData& kmblk);
  void parseFBlock(const Value& JO, dspparam_ptr_t param);
  dspblkdata_ptr_t parseDspBlock(const Value& JO, lyrdata_ptr_t layd, bool force = false);
  dspblkdata_ptr_t parsePchBlock(const Value& JO, lyrdata_ptr_t layd);

  std::map<int, prgdata_ptr_t> _programs;
  std::map<std::string, prgdata_ptr_t> _programsByName;
  std::map<int, keymap_ptr_t> _keymaps;
  std::map<int, multisample*> _multisamples;

  std::map<int, prgdata_ptr_t> _tempprograms;
  std::map<int, keymap_ptr_t> _tempkeymaps;
  std::map<int, multisample*> _tempmultisamples;
};

///////////////////////////////////////////////////////////////////////////////

struct programInst;

struct SynthData {
  SynthData();
  virtual ~SynthData() {
  }

  float seqTime(float dur);
  prgdata_constptr_t getProgram(int progID) const;
  prgdata_constptr_t getProgramByName(const std::string& named) const;

  bankdata_ptr_t _bankdata;
  programInst* _prog;
  float _synsr;
  float _seqCursor;
  std::string _staticBankName;
};

} // namespace ork::audio::singularity
