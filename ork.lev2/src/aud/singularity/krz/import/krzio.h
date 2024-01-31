////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <string>
#include <sstream>
#include <math.h>
#include <vector>
#include <stack>
#include <queue>
#include <set>
#include <map>
#include <functional>
#include <unordered_set>
#include <sndfile.h>
//#include <libgig/DLS.h>
//#include <libgig/gig.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

#include <rapidjson/reader.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/document.h>

#pragma GCC diagnostic pop

#include <ork/kernel/svariant.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/singularity/synthdata.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::audio::singularity::krzio {
std::string convert(std::string krzpath);
typedef uint32_t u32;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint8_t u8;
typedef int8_t s8;

struct Keymap;
struct Program;
struct Layer;
struct Calvin;
struct Hobbes;

struct datablock;

///////////////////////////////////////////////////////////////////////////////

static const u32 kKRZMagic        = 0x5052414d;
static const u32 kKRZHwTypeK2000a = 0x03000000;
static const u32 kKRZHwTypeK2000b = 0x03400000;

///////////////////////////////////////////////////////////////////////////////

struct Keystart {
  std::string _note;
  int _octave = 0;
  std::string _mode;
};
///////////////////////////////////////////////////////////////////////////////

typedef ork::svar64_t fparamvar_t;

struct fparamVar {
  template <typename T> void set(const std::string& name, const std::string& units, const std::string& fmt, T value) {
    _name  = name;
    _units = units;
    _value.set<T>(value);
  }
  operator bool() const {
    return _value.isSet();
  }

  std::string _name;
  std::string _fmt;
  std::string _units;
  fparamvar_t _value;
};
struct fparam {
  int _inputALG;      // 1
  int _inputCourse;   // 2
  int _inputFine;     // 3
  int _inputKeyTrack; // 4
  int _inputVelTrack; // 5
  int _inputSrc1;     // 6
  int _inputDepth;    // 7
  int _inputDptCtl;   // 8
  int _inputMinDepth; // 9
  int _inputMaxDepth; // 10
  int _inputSrc2;     // 11
  //int _inputFiltAlg;  // 12
  int _inputMoreTSCR; // 13
  int _inputRESERVED; // 14
  int _input14;       // 15 (FineHz/KStart/OutputPanUL)
  int _input15;       // 15 (FineHz/KStart/OutputPanUL)

  int _downshift = -1;
  int _filtalg = -1;

  int _tscra      = -1;
  int _tscrb      = -1;
  int _owrda      = -1;
  int _owrdb      = -1;

  fparamVar _varCoarseAdjust;
  fparamVar _varFine;
  fparamVar _var14;
  fparamVar _var15;

  Keystart _varKeyStart;
  fparamVar _varKeyTrack;
  fparamVar _varVelTrack;
  fparamVar _varSrc1Depth;
  fparamVar _varSrc2MinDepth;
  fparamVar _varSrc2MaxDepth;

  std::string _blockScheme;
  std::string _algName;
  std::string _blockName;
  std::string _outputFiltAlg;
  // Keystart _keystart;

  int _blockIndex  = -1;
  float _outputPAD = 0.0f;
};

///////////////////////////////////////////////////////////////////////////////

struct SampleOpts {
  int start;
  int loopstart;
  int end;
  int samplerate;
  int inumchans;
  int iformat;
  std::vector<float> natenvSlopes;
  std::vector<float> natenvSegTimes;
};

struct SampleFile {
  SampleFile();
  void WriteSample(const std::string& name, const SampleOpts& opts);

  const short* mpSampleData;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void swapbytes(T& item) // inplace endian swap
{
  int isize = sizeof(T);
  T temp    = item;
  u8* src   = reinterpret_cast<u8*>(&item);
  u8* tmp   = reinterpret_cast<u8*>(&temp);
  for (int i = 0, j = isize - 1; i < isize; i++, j--) {
    tmp[j] = src[i];
  }
  for (int i = 0; i < isize; i++) {
    src[i] = tmp[i];
  }
}

///////////////////////////////////////////////////////////////////////////////

struct SampleItem {
  SampleItem();

  int miSampleId;
  int _subSampleIndex;

  // /gig::Sample*	mpDlsSample;
  std::string _subSampleName;

  std::vector<float> _natenvSlopes;
  std::vector<float> _natenvSegTimes;
  std::vector<float> _altnatenvSlopes;
  std::vector<float> _altnatenvSegTimes;

  u8 _uSubFlags;
  uint32_t _ustart, _uend, _uloop, _ualt;
  int _playbackMode; // 0 = Normal, 1 = Reverse, 2 = Bi-Direct
  float _volAdj;
  float _altVolAdj;
  float _sampleRate;
  float _pitchADJ;
  int _highestPitch;
  int _start;
  int _altStart;
  int _loopPoint;
  int _end;
  int _rootKey;
  void* _sampleData;
  int _numSamples;
  bool _valid;
  bool _isLooped;
};

struct MultiSample {
  MultiSample();

  int _objectId;
  std::string _multiSampleName;
  std::vector<SampleItem*> _subSamples;
  bool _isStereo;
  int _numSoundFiles;
};

///////////////////////////////////////////////////////////////////////////////

struct RegionData {
  int miSampleId;
  int miSubSample;
  int miTuning;
  float mVolumeAdjust;

  // SampleItem* mpSample;
  RegionData()
      : miSampleId(0)
      , miSubSample(-1)
      , miTuning(0)
      , mVolumeAdjust(0.0f) {
  }

  bool operator==(const RegionData& rhs) const {
    return (
        (miSampleId == rhs.miSampleId) &&
        (miSubSample ==
         rhs.miSubSample)); //&&(miTuning==rhs.miTuning)&&(mVolumeAdjust==rhs.mVolumeAdjust)&&(mpSample==rhs.mpSample));
  }
  bool operator!=(const RegionData& rhs) const {
    return (
        (miSampleId != rhs.miSampleId) || (miSubSample != rhs.miSubSample) || (miTuning != rhs.miTuning) ||
        (mVolumeAdjust != rhs.mVolumeAdjust));
  }
};

//////////////////////////////////////////////////////

static const int kNKEYS = 128;
static const int kNVELS = 8;

//////////////////////////////////////////////////////
struct RegionMap {
  RegionData mRgnData[kNKEYS * kNVELS];
};
//////////////////////////////////////////////////////
struct RegionInst {
  int miLoKey;
  int miHiKey;
  int miLoVel;
  int miHiVel;
  RegionData mData;
  bool operator<(const RegionInst& rhs) const;
  RegionInst();
  RegionInst(const RegionInst& oth);
};
//////////////////////////////////////////////////////
struct rdhasher {
  size_t operator()(const RegionData& r) const;
};
struct rdequer {
  bool operator()(const RegionData& a, const RegionData& b) const;
};
//////////////////////////////////////////////////////
struct Keymap {
  int RegionMapIndex(int ikey, int ivel) const {
    return (ivel * kNKEYS) + ikey;
  }
  RegionData& RegionDataForKeyVel(int ikey, int ivel) {
    return mRgnMap.mRgnData[RegionMapIndex(ikey, ivel)];
  }
  Keymap();

  typedef std::unordered_set<RegionData, rdhasher, rdequer> regionset_t;

  regionset_t mRegionSet;
  const RegionData* mRgnPtrMap[kNKEYS * kNVELS];
  RegionMap mRgnMap;

  std::set<RegionInst> mRegionInsts;

  int miKeymapID;
  std::string mKeymapName;

  int miKeymapSampleId;
  int miKeymapBasePitch;
  int miKeymapCentsPerEntry;
  int miKrzMethod;
  int miKrzNumEntries;
  int miKrzEntrySize;
};

///////////////////////////////////////////////////////////////////////////////
struct algcfg {
  int _wp;
  int _w1;
  int _w2;
  int _w3;
  int _wa;
};
///////////////////////////////////////////////////////////////////////////////
struct Controller {
  std::string _source;
  bool _flip;
  float _min;
  float _max;
};
///////////////////////////////////////////////////////////////////////////////
struct Asr {
  Asr();

  std::string _name;
  float _attack, _delay, _release;
  std::string _mode;
  std::string _triggerSource;
};
struct Env {
  Env();

  std::string _name;
  int _loopSeg, _loopCount;
  float _rates[7];
  float _levels[7];
  int _code;
};
struct EnvControl {
  std::string _mode;

  std::string _atkControl;
  float _atkAdjust, _atkKeyTrack, _atkDepth;
  float _atkVelTrack;

  std::string _decControl;
  float _decAdjust, _decKeyTrack, _decDepth;

  std::string _relControl;
  float _relAdjust, _relKeyTrack, _relDepth;
};
struct Fun {
  std::string _name;
  std::string _op;
  Controller _a, _b;
};
struct Lfo {
  Lfo();
  Controller _ctrlRate;
  std::string _name;
  float _phase;
  std::string _shape;
};
struct CalvinKmPage {
  CalvinKmPage(Calvin* c);
  Calvin* _calvin;
  int _kmid1, _kmid2;
  Keymap* _km1;
  Keymap* _km2;
  int _transposeTimbreShift;
  int _detune;
  int _keyTrack;
  int _velTrack;
  int _tControl, _tRange;
  int _sampleRoot, _sampleSkip;
  int _sampleBlockRoot;
  int _altControl;
  int _timbreShift;
  int _playbackMode;
};
struct Calvin {
  Calvin(Layer* p);

  Layer* _layer;
  int _algorithm;
  std::string _algname;
  algcfg _algconfig;
  CalvinKmPage _kmpage;
  Controller _ctrlPutch;
  fparam _pitchFP;
};
struct Hobbes {
  Hobbes();
  std::string _blockScheme;
  std::string _algName;
  std::string _blockName;
  std::string _outputAdjust;
  std::string _outputCourse;
  std::string _outputFine;
  std::string _outputKT;
  std::string _outputVT;
  std::string _outputDepth;
  std::string _outputMinDepth;
  std::string _outputMaxDepth;
  std::string _outputFineHZ;
  std::string _outputKStart;
  std::string _outputPAD;
  std::string _outputFiltAlg;

  fparam _hobbesFP;
  bool _ok2emit;
};
struct VTRIG {
  VTRIG()
      : _sense(false)
      , _level(0) {
  }
  bool _sense;
  int _level;
};
struct Layer {
  Layer(Program* p);

  int _layerIndex;
  int _loKey, _hiKey;
  int _loVel, _hiVel;
  int _transpose;
  int _tune;
  Controller _ctrlLayerEnable;
  Controller _ctrlDelay;
  int _xfade;
  bool _ignRels, _ignSust, _ignSost, _ignSusp;
  bool _atkHold, _susHold;
  bool _opaqueLayer, _xfadeSense, _stereoLayer;
  bool _chanNum, _trigOnKeyUp;
  int _bendMode;
  VTRIG _vt1, _vt2;

  int _headroom  = -1;
  int _pair      = -1;
  int _panmode   = -1;
  int _pan      = -1;

  uint8_t _dbg_vrange = 0;
  uint8_t _dbg_flags = 0;
  uint8_t _dbg_moreflags = 0;

  Program* _program;
  std::map<int, Asr*> _asrmap;
  std::map<int, Env*> _envmap;
  std::map<int, Fun*> _funmap;
  std::map<int, Lfo*> _lfomap;
  EnvControl* _envc;
  Calvin* _calvin;
  Hobbes* _hobbes[4];
};
struct Program {
  Program();

  Layer* newLayer();

  int _programID;
  std::string _programName;
  std::vector<Layer*> _layers;
  std::string _programFormat;
  Lfo* _glfo2;
  Asr* _gasr2;
  std::map<int, Fun*> _gfunmap;
  bool _mono, _porto, _atkPorto, _legato;
  bool _enableGlobals;
  bool _debug = false;
};

///////////////////////////////////////////////////////////////////////////////

struct Krz {
  Krz();

  u32 miFileHeaderAndPRAMLength;
  s16* mpSampleData;
  int miSampleDataLength;
};

///////////////////////////////////////////////////////////////////////////////

std::string getMidiNoteName(int notenum);
std::string getControlSourceName(int srcid);
std::string getFunOpName(int srcid);
std::string getDspBlockName(int fnID);
std::string getDspBlockScheme(int fnID);
std::string getLfoShape87(int ival);
algcfg getAlgConfig(int algID);
float getAsrTime(int ival);
float getEnvCtrl(int ival);
int getKeyTrack85(int ival);
float getLfoRate86(int ival);
algcfg getAlgConfig(int algID);
int getDspBlockWidth(int fnid);
int getKeyTrack85(int ival);
std::string getDspBlockScheme(int fnid);
std::string getDspBlockName(int srcid);
float getVelTrack97(int ival);
int getVelTrack96(int ival);
int getVelTrack98(int ival);
float get72Adjust(int index);
Keystart getKeyStart81(int uval);
std::string getFreq83(int ival);

int makeSigned(int inp);

void getFParamFRQ(fparam& fp);
void getFParamDRV(fparam& fp);
void getFParamPCH(fparam& fp);
void getFParamAMP(fparam& fp);
void getFParamPOS(fparam& fp);
void getFParamRES(fparam& fp);
void getFParamWRP(fparam& fp);
void getFParamDEP(fparam& fp);
void getFParamAMT(fparam& fp);
void getFParamWID(fparam& fp);
void getFParamPWM(fparam& fp);
void getFParamSEP(fparam& fp);
void getFParamEVNODD(fparam& fp);
void getFParamXFD(fparam& fp);

///////////////////////////////////////////////////////////////////////////////

struct datablock {
  struct iterator {
    int miIndex;

    iterator()
        : miIndex(0) {
    }

    iterator(const iterator& oth, int offset)
        : miIndex(oth.miIndex + offset) {
    }

    void SkipData(int icount) {
      miIndex += icount;
    }
  };

  std::vector<u8> mData;

  datablock() {
  }

  void AddBytes(const void* pdata, int isize) {
    const u8* pu8 = (const u8*)pdata;
    for (int i = 0; i < isize; i++)
      mData.push_back(pu8[i]);
  }
  const u8* RefData(const iterator& it) const {
    return &mData[it.miIndex];
  }
  template <typename T> bool GetData(T& output, iterator& it) const {
    int isiz = sizeof(output);
    memcpy((void*)&output, (const void*)&mData[it.miIndex], isiz);
    it.miIndex += isiz;
    swapbytes<T>(output);
    return it.miIndex < mData.size();
  }
  template <typename T> T GetTypedData(iterator& it) const {
    T output;
    int isiz = sizeof(output);
    memcpy((void*)&output, (const void*)&mData[it.miIndex], isiz);
    it.miIndex += isiz;
    swapbytes<T>(output);
    assert(it.miIndex < mData.size());
    return output;
  }
  void CopyFrom(const datablock& oth, iterator& it, int icount) {
    // printf( "  copy<%d>\n\n", icount );
    const u8* psrc = &oth.mData[it.miIndex];
    for (int i = 0; i < icount; i++) {
      u8 uch = psrc[i];
      mData.push_back(uch);
      // printf("%2x ", int(uch));
      // if( i%16 == 0xf ) printf("\n");
    }
    //	printf( "\n" );
    it.miIndex += icount;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct filescanner {
  filescanner(const char* pname);
  ~filescanner();
  void SkipData(int ibytes);

  template <typename T> bool GetData(T& output) {
    return mMainDataBlock.GetData(output, mMainIterator);
  }

  /////////////////////////////////////////////

  void ParseBlock(const datablock& db);
  void ParseObject(const datablock& db, datablock::iterator& it);
  void ParseKeyMap(const datablock& db, datablock::iterator& it, int iObjectID, std::string ObjName);
  void ParseSampleHeader(const datablock& db, datablock::iterator& it, int iObjectID, std::string ObjName);
  void ParseKeyMapEntryIt(Keymap* kmap, const datablock& db, datablock::iterator& it, int ivelrng);
  void ParseProgram(const datablock& db, datablock::iterator& it, int iObjectID, std::string ObjName);

  void parseCalvin(const datablock& db, datablock::iterator& it);
  void parseHobbes(const datablock& db, datablock::iterator& it, u8 code);
  void parseControllers(const datablock& db, datablock::iterator& it, u8 code);

  void fparamOutput(const fparam& fp, const std::string& blkname, rapidjson::Value& jsono);
  void fparamVarOutput(const fparamVar& fp, const std::string& blkname, rapidjson::Value& jsono);

  /////////////////////////////////////////////

  void emitMultiSample(const MultiSample* ms, rapidjson::Value& parent);
  void emitSample(const SampleItem* si, rapidjson::Value& parent);
  void emitKeymap(const Keymap* km, rapidjson::Value& parent);

  void emitProgram(const Program* p, rapidjson::Value& parent);
  void emitLayer(const Layer* l, rapidjson::Value& parent);
  void emitAsr(const Asr* a, rapidjson::Value& parent);
  void emitEnv(const Env* e, rapidjson::Value& parent);
  void emitEnvControl(const EnvControl& ec, rapidjson::Value& parent);
  void emitFun(const Fun* f, rapidjson::Value& parent);
  void emitLfo(const Lfo* f, rapidjson::Value& parent);
  void emitCalvin(const Calvin* c, rapidjson::Value& parent);
  void emitHobbes(const Hobbes* h, rapidjson::Value& parent);

  /////////////////////////////////////////////

  void scanAndDump();

  /////////////////////////////////////////////

  void AddStringKVMember(rapidjson::Value& parent, const std::string& key, const std::string& val);

  void AddMember(rapidjson::Value& parent, const std::string& key, const std::string& val) {
    AddStringKVMember(parent, key, val);
  }
  void AddMember(rapidjson::Value& parent, const std::string& key, rapidjson::Value& child) {
    rapidjson::Value kv;
    kv.SetString(key.c_str(), _japrog);
    parent.AddMember(kv, child, _japrog);
  }

  template <typename T> void AddMember(rapidjson::Value& parent, const std::string& key, T val) {
    rapidjson::Value kv;
    kv.SetString(key.c_str(), _japrog);
    parent.AddMember(kv, val, _japrog);
  }

  /////////////////////////////////////////////

  std::string jsonPrograms() const;

  /////////////////////////////////////////////

  FILE* mpFile;
  int miSize;
  void* mpData;

  datablock mMainDataBlock;
  std::vector<datablock> mDatablocks;
  datablock::iterator mMainIterator;

  std::map<int, Program*> _programs;
  std::map<int, Keymap*> _keymaps;
  std::map<int, MultiSample*> _samples;
  std::map<int, SampleItem*> _subSamples;

  // std::map<int, DLS::Instrument*> _dlsInstruments;

  // gig::File _dlsFile;
  SampleFile _sfile;

  rapidjson::Document _joprog;
  rapidjson::Value _joprogroot;
  rapidjson::Value _joprogobjs;
  rapidjson::Document::AllocatorType& _japrog;

  std::stack<rapidjson::Value> _jopstack;

  rapidjson::Document _jsonoutSample;
  // rapidjson::Value* _curLayerObject;
  Layer* _curLayer;
  Program* _curProgram;

  bool _globalsFlag;
  int _curALG;
  std::map<std::string, std::string> _algschmap;

  std::queue<std::string> _algschmq;
};

///////////////////////////////////////////////////////////////////////////////

float log_base(float base, float inp);
int GenSampleKey(int iobjid, int isubi);

} // namespace ork::audio::singularity::krzio
