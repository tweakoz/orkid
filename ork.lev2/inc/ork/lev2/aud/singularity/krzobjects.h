#include <rapidjson/reader.h>
#include <rapidjson/document.h>
#include "krzdata.h"

using namespace rapidjson;

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

struct VastObjectsDB {
  void loadJson(const std::string& fname, int bank);

  const ProgramData* findProgram(int idx) const;
  const ProgramData* findProgramByName(const std::string named) const;
  keymap_constptr_t findKeymap(int kmID) const;

  //

  keymap_ptr_t parseKeymap(int kmid, const rapidjson::Value& JO);
  void parseAsr(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const std::string& name);
  void parseLfo(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const std::string& name);
  void parseFun(const rapidjson::Value& JO, controlblockdata_ptr_t cblock, const std::string& name);
  lyrdata_ptr_t parseLayer(const rapidjson::Value& JO, ProgramData* pd);
  void parseEnvControl(const rapidjson::Value& JO, EnvCtrlData& ed);
  ProgramData* parseProgram(const rapidjson::Value& JO);
  multisample* parseMultiSample(const rapidjson::Value& JO);
  sample* parseSample(const rapidjson::Value& JO, const multisample* parent);

  algdata_ptr_t parseAlg(const rapidjson::Value& JO);
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
