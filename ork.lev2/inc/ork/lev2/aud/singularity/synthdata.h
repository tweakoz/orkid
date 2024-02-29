////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "reflection.h"
#include "krztypes.h"
#include "modulation.h"
#include <ork/kernel/svariant.h>
#include <ork/file/path.h>

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

  ProgramData();

  lyrdata_ptr_t newLayer();
  lyrdata_ptr_t getLayer(int i) const;
  void setLayer(int i, lyrdata_ptr_t l);
  void addHudInfo(std::string str);
  void merge(const ProgramData& oth);

  std::string _name;
  std::string _tags;
  bool _monophonic = false;
  float _portamento_rate = 0.0f;
  float _gainDB = 0.0f;
  std::vector<lyrdata_ptr_t> _layerdatas;
  std::vector<std::string> _hudinfos;
  varmap::varmap_ptr_t _varmap;
};

///////////////////////////////////////////////////////////////////////////////
struct KrzAlgData;
struct EnvCtrlData;

struct BankData : public ork::Object {

  DeclareConcreteX(BankData, ork::Object);

  void addProgram(int idx, const std::string& name, prgdata_ptr_t program);
  prgdata_ptr_t findProgram(int idx) const;
  prgdata_ptr_t findProgramByName(const std::string named) const;
  multisample_ptr_t findMultiSampleByName(const std::string named) const;
  keymap_ptr_t findKeymap(int kmID) const;
  keymap_ptr_t findKeymapByName(std::string kmID) const;

  void merge( const BankData& oth );
  //

  std::map<int, prgdata_ptr_t> _programs;
  std::map<std::string, prgdata_ptr_t> _programsByName;
  std::map<int, keymap_ptr_t> _keymaps;
  std::map<std::string, keymap_ptr_t> _keymapsByName;
  std::map<int, multisample_ptr_t> _multisamples;
  std::map<std::string, multisample_ptr_t> _multisamplesByName;
  varmap::VarMap _uservars;

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
