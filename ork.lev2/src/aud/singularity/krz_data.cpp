#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/layer.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/sampler.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

static const auto krzbasedir = basePath() / "kurzweil" / "krz";

VastObjectsDB* KrzSynthData::baseObjects() {
  static VastObjectsDB* objdb = nullptr;
  if (nullptr == objdb) {
    objdb = new VastObjectsDB;
    objdb->loadJson("k2v3base", 0);
  }

  return objdb;
}

KrzSynthData::KrzSynthData()
    : SynthData() {
}
const ProgramData* KrzSynthData::getProgram(int progID) const {
  auto ObjDB = baseObjects();
  return ObjDB->findProgram(progID);
}
const ProgramData* KrzSynthData::getProgramByName(const std::string& named) const {
  auto ObjDB = baseObjects();
  return ObjDB->findProgramByName(named);
}

///////////////////////////////////////////////////////////////////////////////

KrzKmTestData::KrzKmTestData()
    : SynthData() {
}
const ProgramData* KrzKmTestData::getProgram(int kmID) const {
  ProgramData* rval = nullptr;
  auto it           = _testKmPrograms.find(kmID);
  if (it == _testKmPrograms.end()) {
    auto ObjDB  = KrzSynthData::baseObjects();
    rval        = new ProgramData;
    rval->_role = "KmTest";
    auto km     = ObjDB->findKeymap(kmID);
    if (km) {
      auto lyr     = rval->newLayer();
      lyr->_keymap = km;
      rval->_name  = ork::FormatString("%s", km->_name.c_str());
    } else
      rval->_name = ork::FormatString("\?\?\?\?");
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

KrzTestData::KrzTestData()
    : SynthData() {
  genTestPrograms();
}

const ProgramData* KrzTestData::getProgram(int progid) const {
  int inumtests = _testPrograms.size();
  int testid    = progid % inumtests;
  printf("test<%d>\n", testid);
  auto test = _testPrograms[testid];
  return test;
}

///////////////////////////////////////////////////////////////////////////////

void KrzTestData::genTestPrograms() {
  auto t1   = new ProgramData;
  t1->_role = "PrgTest";
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
    F1->_dspBlock          = "2PARAM SHAPER";
    F1->_blockIndex        = 0;
    F1->_paramd[0]._coarse = -60.0;
    F1->_paramd[0].useKrzEvnOddEvaluator();
    F1->_paramd[0]._units = "dB";
  } else if (0) {
    auto F2                = l1->_dspBlocks[2];
    F2->_blockIndex        = 0;
    F2->_dspBlock          = "SHAPER";
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
