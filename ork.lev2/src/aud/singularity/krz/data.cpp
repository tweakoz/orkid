////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <string>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/layer.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/kernel/datacache.h>
#include <ork/kernel/datablock.h>
#include "import/krzio.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
  const s16* getK2V3InternalSoundBlock();

bankdata_ptr_t KrzSynthData::baseObjects() {

  auto SOUNDBLOCKS = getK2V3InternalSoundBlock();

  auto base      = ork::audio::singularity::basePath() / "kurzweil";
  auto base_path = base/"k2v3base.bin";

  auto bin_file = fopen(base_path.c_str(), "rb");
  //printf("file<%p:%s>\n", (void*) bin_file, base_path.c_str());
  fseek(bin_file, 0, SEEK_END);
  size_t ilen = ftell(bin_file);
  //printf("length<%zu>\n", ilen);
  auto bin_data = malloc(ilen);
  fseek(bin_file, 0, SEEK_SET);
  fread(bin_data, ilen, 1, bin_file);
  fclose(bin_file);

  auto krzbasehasher = DataBlock::createHasher();
  krzbasehasher->accumulateString("krzbaseobjects"); // identifier
  krzbasehasher->accumulateItem<float>(1.6);         // version code
  krzbasehasher->accumulate(bin_data, ilen);         // data

  krzbasehasher->finish();
  uint64_t krzbase_hash = krzbasehasher->result();
  datablock_ptr_t dblock = nullptr; //DataBlockCache::findDataBlock(krzbase_hash);
  if (dblock == nullptr) {
    auto krz_import = krzio::convert(base_path.c_str());

    auto& base_json = krz_import->_json_programs;
    dblock        = std::make_shared<DataBlock>();
    auto json_path = base/"k2v3base.json";
    if(false) {
      auto array = std::vector<uint8_t>(base_json.begin(), base_json.end());
      array.push_back(0);
      FILE* json_file = fopen(json_path.c_str(), "wb");
      fwrite(base_json.c_str(), base_json.size(), 1, json_file);
      fclose(json_file);
      dblock->addData(array.data(), array.size());
      DataBlockCache::setDataBlock(krzbase_hash, dblock);
    }
    else { // load from file
      FILE* json_file = fopen(json_path.c_str(), "rb");
      size_t length = 0;
      fseek(json_file, 0, SEEK_END);
      length = ftell(json_file);
      printf("length<%zu>\n", length);
      auto mem = malloc(length+1);
      fseek(json_file,0,SEEK_SET);
      fread(mem,length,1,json_file);
      dblock->addData(mem, length+1);

    }
  }
  KrzBankDataParser parser;
  parser._parsingROM = true;
  parser._sampledata = SOUNDBLOCKS;
  auto as_str = std::string(dblock->_storage.begin(), dblock->_storage.end());
  parser.loadKrzJsonFromString(as_str, 0);
  return parser._objdb;
}

///////////////////////////////////////////////////////////////////////////////

KrzSynthData::KrzSynthData(bool base_data)
    : SynthData() {
  if(base_data){
    _bankdata = baseObjects();
  }
  else{
    _bankdata = std::make_shared<BankData>();
  }
  
}

///////////////////////////////////////////////////////////////////////////////

bankdata_ptr_t KrzSynthData::loadBank(const file::Path& syxpath, int remap_base){
  auto krz_data = krzio::convert(syxpath.c_str());
  KrzBankDataParser parser;
  parser._sampledata = (const s16*) krz_data->_sample_data.data();
  
  bankdata_ptr_t bankdata = parser.loadKrzJsonFromString(krz_data->_json_programs, remap_base);
  _bankdata->merge(*bankdata);

  _bankdata->_uservars.set<krzio::krzimportdata_ptr_t>(syxpath.c_str(),krz_data);
  return bankdata;
}


///////////////////////////////////////////////////////////////////////////////

KrzKmTestData::KrzKmTestData()
    : SynthData() {
}

///////////////////////////////////////////////////////////////////////////////

KrzTestData::KrzTestData()
    : SynthData() {
  genTestPrograms();
}

///////////////////////////////////////////////////////////////////////////////

void KrzTestData::genTestPrograms() {
  auto t1   = new ProgramData;
  t1->_tags = "PrgTest";
  _testPrograms.push_back(t1);
  t1->_name = "YO";

  auto l1               = t1->newLayer();
  const int keymap_sine = 163;
  const int keymap_saw  = 151;

  l1->_keymap = KrzSynthData::baseObjects()->findKeymap(keymap_sine);

  auto ampenv  = l1->appendController<RateLevelEnvData>("AMPENV");
  auto& aesegs = ampenv->_segments;
  aesegs.push_back(EnvPoint{0, 1});
  aesegs.push_back(EnvPoint{0, 0});
  aesegs.push_back(EnvPoint{0, 0});
  aesegs.push_back(EnvPoint{0, 1});
  aesegs.push_back(EnvPoint{0, 0});
  aesegs.push_back(EnvPoint{0, 0});
  aesegs.push_back(EnvPoint{1, 0});

  auto env2    = l1->appendController<RateLevelEnvData>("ENV2");
  auto& e2segs = env2->_segments;
  e2segs.push_back(EnvPoint{2, 1});
  e2segs.push_back(EnvPoint{0, 1});
  e2segs.push_back(EnvPoint{0, 1});
  e2segs.push_back(EnvPoint{0, 0.5});
  e2segs.push_back(EnvPoint{0, 0});
  e2segs.push_back(EnvPoint{0, 0});
  e2segs.push_back(EnvPoint{1, 0});

  auto env3    = l1->appendController<RateLevelEnvData>("ENV3");
  auto& e3segs = env3->_segments;
  e3segs.push_back(EnvPoint{8, 1});
  e3segs.push_back(EnvPoint{0, 1});
  e3segs.push_back(EnvPoint{0, 1});
  e3segs.push_back(EnvPoint{0, 0.5});
  e3segs.push_back(EnvPoint{0, 0});
  e3segs.push_back(EnvPoint{0, 0});
  e3segs.push_back(EnvPoint{1, 0});

  auto lfo1         = l1->appendController<LfoData>("LFO1");
  lfo1->_controller = "ON";
  lfo1->_maxRate    = 0.1;

  /*

  l1->_algData = configureKrzAlgorithm(1);

  if (0) {
    auto F1                = l1->_dspBlocks[1];
    F1->_blocktype          = "2PARAM SHAPER";
    F1->_blockIndex        = 0;
    F1->_paramd[0]._coarse = -60.0;
    F1->_paramd[0].useKrzEvnOddEvaluator();
    F1->_paramd[0]._units = "dB";
  } else if (0) {
    auto F2                = l1->_dspBlocks[2];
    F2->_blockIndex        = 0;
    F2->_blocktype          = "SHAPER";
    F2->_paramd[0]._coarse = 0.1;
    // F2->_paramd[0]._paramScheme = "AMT"; need new evaluator
    F2->_paramd[0]._units = "x";
  }
  if (0) {
    auto F3                = l1->_dspBlocks[3];
    F3->_blockIndex        = 1;
    F3->_paramd[0]._coarse = -96.0;
    // F3->_paramd[0]._paramScheme = "ODD"; need new evaluator
    F3->_paramd[0]._units = "dB";
  }*/
}

} // namespace ork::audio::singularity
