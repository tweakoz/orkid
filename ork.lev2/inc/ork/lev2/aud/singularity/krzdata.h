////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

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
  void loadBank(const file::Path& syxpath);
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

struct KrzBankDataParser {
  void loadKrzJsonFromFile(const std::string& fname, int bank);
  void loadKrzJsonFromString(const std::string& json, int bank);
  keymap_ptr_t parseKeymap(int kmid, const rapidjson::Value& JO);
  void parseAsr(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const EnvCtrlData& ENVCTRL, const std::string& name);
  void parseLfo(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const std::string& name);
  void parseFun(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const std::string& name);
  void parseEnvControl(const rapidjson::Value& JO, EnvCtrlData& ed);
  prgdata_ptr_t parseProgram(const rapidjson::Value& JO);
  multisample* parseMultiSample(const rapidjson::Value& JO);
  sample* parseSample(const rapidjson::Value& JO, const multisample* parent);

  KrzAlgData parseAlg(const rapidjson::Value& JO);
  void parseKmpBlock(const Value& JO, KmpBlockData& kmblk);
  void parseFBlock(const Value& JO, dspparam_ptr_t param);
  dspblkdata_ptr_t parseDspBlock(const Value& JO, dspstagedata_ptr_t stage, lyrdata_ptr_t layd, bool force = false);
  dspblkdata_ptr_t parsePchBlock(const Value& JO, dspstagedata_ptr_t stage, lyrdata_ptr_t layd);

  lyrdata_ptr_t parseLayer(const rapidjson::Value& JO, prgdata_ptr_t pd);

  bankdata_ptr_t _objdb;
  std::map<int, prgdata_ptr_t> _tempprograms;
  std::map<int, keymap_ptr_t> _tempkeymaps;
  std::map<int, multisample*> _tempmultisamples;
};

} // namespace ork::audio::singularity
