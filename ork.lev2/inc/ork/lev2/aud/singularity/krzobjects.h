#include <rapidjson/reader.h>
#include <rapidjson/document.h>
#include "krzdata.h"

using namespace rapidjson;

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

struct VastObjectsDB {
  void loadJson(const std::string& fname, int bank);

  const programData* findProgram(int idx) const;
  const programData* findProgramByName(const std::string named) const;
  const keymap* findKeymap(int kmID) const;

  //

  keymap* parseKeymap(int kmid, const rapidjson::Value& JO);
  AsrData* parseAsr(const rapidjson::Value& JO, const std::string& name);
  LfoData* parseLfo(const rapidjson::Value& JO, const std::string& name);
  FunData* parseFun(const rapidjson::Value& JO, const std::string& name);
  lyrdata_ptr_t parseLayer(const rapidjson::Value& JO, programData* pd);
  void parseEnvControl(const rapidjson::Value& JO, EnvCtrlData& ed);
  programData* parseProgram(const rapidjson::Value& JO);
  multisample* parseMultiSample(const rapidjson::Value& JO);
  sample* parseSample(const rapidjson::Value& JO, const multisample* parent);

  void parseAlg(const rapidjson::Value& JO, AlgData& algd);
  void parseKmpBlock(const Value& JO, KmpBlockData& kmblk);
  void parseFBlock(const Value& JO, DspParamData& fb);
  dspblkdata_ptr_t parseDspBlock(const Value& JO, lyrdata_ptr_t layd, bool force = false);
  dspblkdata_ptr_t parsePchBlock(const Value& JO, lyrdata_ptr_t layd);

  std::map<int, programData*> _programs;
  std::map<std::string, programData*> _programsByName;
  std::map<int, keymap*> _keymaps;
  std::map<int, multisample*> _multisamples;

  std::map<int, programData*> _tempprograms;
  std::map<int, keymap*> _tempkeymaps;
  std::map<int, multisample*> _tempmultisamples;
};

} // namespace ork::audio::singularity
