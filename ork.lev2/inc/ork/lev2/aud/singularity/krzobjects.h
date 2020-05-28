#include <rapidjson/reader.h>
#include <rapidjson/document.h>
#include "synthdata.h"

using namespace rapidjson;

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
struct KrzAlgData {
  int _algindex = -1;
  algdata_ptr_t _algdata;
};
///////////////////////////////////////////////////////////////////////////////

struct EnvCtrlData {
  bool _useNatEnv  = false; // kurzeril per-sample envelope
  float _atkAdjust = 1.0f;
  float _decAdjust = 1.0f;
  float _relAdjust = 1.0f;

  float _atkKeyTrack = 1.0f;
  float _atkVelTrack = 1.0f;
  float _decKeyTrack = 1.0f;
  float _decVelTrack = 1.0f;
  float _relKeyTrack = 1.0f;
  float _relVelTrack = 1.0f;
};

///////////////////////////////////////////////////////////////////////////////

struct SynthObjectsDB {
  void loadJson(const std::string& fname, int bank);

  void addProgram(int idx, const std::string& name, ProgramData* program);
  const ProgramData* findProgram(int idx) const;
  const ProgramData* findProgramByName(const std::string named) const;
  keymap_constptr_t findKeymap(int kmID) const;

  //

  keymap_ptr_t parseKeymap(int kmid, const rapidjson::Value& JO);
  void parseAsr(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const EnvCtrlData& ENVCTRL, const std::string& name);
  void parseLfo(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const std::string& name);
  void parseFun(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const std::string& name);
  lyrdata_ptr_t parseLayer(const rapidjson::Value& JO, ProgramData* pd);
  void parseEnvControl(const rapidjson::Value& JO, EnvCtrlData& ed);
  ProgramData* parseProgram(const rapidjson::Value& JO);
  multisample* parseMultiSample(const rapidjson::Value& JO);
  sample* parseSample(const rapidjson::Value& JO, const multisample* parent);

  KrzAlgData parseAlg(const rapidjson::Value& JO);
  void parseKmpBlock(const Value& JO, KmpBlockData& kmblk);
  void parseFBlock(const Value& JO, DspParamData& fb);
  dspblkdata_ptr_t parseDspBlock(const Value& JO, lyrdata_ptr_t layd, bool force = false);
  dspblkdata_ptr_t parsePchBlock(const Value& JO, lyrdata_ptr_t layd);

  std::map<int, ProgramData*> _programs;
  std::map<std::string, ProgramData*> _programsByName;
  std::map<int, keymap_ptr_t> _keymaps;
  std::map<int, multisample*> _multisamples;

  std::map<int, ProgramData*> _tempprograms;
  std::map<int, keymap_ptr_t> _tempkeymaps;
  std::map<int, multisample*> _tempmultisamples;
};

} // namespace ork::audio::singularity
