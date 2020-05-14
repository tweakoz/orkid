#pragma once

#include "krztypes.h"
#include "modulation.h"
#include "sf2.h"
#include <ork/kernel/svariant.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

struct EnvPoint {
  float _rate;
  float _level;
};

///////////////////////////////////////////////////////////////////////////////

struct ControllerInst;

struct ControllerData {
  virtual ControllerInst* instantiate() const = 0;
  virtual ~ControllerData() {
  }

  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////

enum struct RlEnvType {
  ERLTYPE_DEFAULT = 0,
  ERLTYPE_KRZAMPENV,
  ERLTYPE_KRZMODENV,
};

///////////////////////////////////////////////////////////////////////////////

struct RateLevelEnvData : public ControllerData {
  RateLevelEnvData();
  ControllerInst* instantiate() const final;
  bool isBiPolar() const;

  std::vector<EnvPoint> _segments;
  bool _ampenv;
  bool _bipolar;
  RlEnvType _envType;
};

///////////////////////////////////////////////////////////////////////////////

struct natenvseg {
  float _slope;
  float _time;
};

///////////////////////////////////////////////////////////////////////////////

struct AsrData : public ControllerData {
  ControllerInst* instantiate() const final;

  std::string _trigger;
  std::string _mode;
  float _delay;
  float _attack;
  float _release;
};

enum struct eLoopMode {
  NOTSET = -1,
  NONE   = 0,
  FWD,
  BIDIR,
  FROMKM,
};

///////////////////////////////////////////////////////////////////////////////

struct sample {
  sample();

  std::string _name;
  const s16* _sampleBlock;

  int _blk_start;
  int _blk_alt;

  int _blk_loopstart;
  int _blk_loopend;

  int _blk_end;

  int _loopPoint;
  int _subid;
  float _sampleRate;
  float _linGain;
  int _rootKey;
  int _highestPitch;

  eLoopMode _loopMode = eLoopMode::NONE;
  std::vector<natenvseg> _natenv;

  int _pitchAdjust = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct multisample {
  std::string _name;
  int _objid;
  std::map<int, sample*> _samples;
};

///////////////////////////////////////////////////////////////////////////////

struct kmregion {
  int _lokey = 0, _hikey = 0;
  int _lovel = 0, _hivel = 127;
  int _tuning                 = 0;
  eLoopMode _loopModeOverride = eLoopMode::NOTSET;
  float _volAdj               = 0.0f;
  float _linGain              = 1.0f;
  int _multsampID = -1, _sampID = -1;
  std::string _sampleName;
  const multisample* _multiSample = nullptr;
  const sample* _sample           = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct KeyMap {
  std::string _name;
  std::vector<kmregion*> _regions;
  int _kmID;

  kmregion* getRegion(int note, int vel) const;
};

///////////////////////////////////////////////////////////////////////////////

struct LfoData : public ControllerData {
  LfoData();
  ControllerInst* instantiate() const final;

  float _initialPhase;
  float _minRate;
  float _maxRate;
  std::string _controller;
  std::string _shape;
};

///////////////////////////////////////////////////////////////////////////////

struct FunData : public ControllerData {
  ControllerInst* instantiate() const final;

  std::string _a, _b, _op;
};

///////////////////////////////////////////////////////////////////////////////

struct KmpBlockData {
  const KeyMap* _keymap;
  int _transpose   = 0;
  float _keyTrack  = 100.0f;
  float _velTrack  = 0.0f;
  int _timbreShift = 0;
  std::string _pbMode;
};

///////////////////////////////////////////////////////////////////////////////

struct EnvCtrlData {
  bool _useNatEnv  = true;
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

struct ControlBlockData {
  const ControllerData* _cdata[kmaxctrlperblock] = {0, 0, 0, 0};
};

///////////////////////////////////////////////////////////////////////////////

struct LayerData {
  LayerData();

  algdata_ptr_t _algdata;

  EnvCtrlData _envCtrlData;

  KmpBlockData _kmpBlock;
  dspblkdata_ptr_t _pchBlock;

  dspstagedata_ptr_t appendStage();

  const KeyMap* _keymap   = nullptr;
  int _numdspblocks       = 0;
  int _loKey              = 0;
  int _hiKey              = 127;
  int _loVel              = 0;
  int _hiVel              = 127;
  float _channelGains[4]  = {0, 0, 0, 0};
  int _channelPans[4]     = {0, 0, 0, 0};
  int _channelPanModes[4] = {0, 0, 0, 0};
  bool _ignRels           = false;
  bool _atk1Hold          = false; // ThrAtt
  bool _atk3Hold          = false; // TilDec

  ControlBlockData* _ctrlBlocks[kmaxctrlblocks] = {0, 0, 0, 0, 0, 0, 0, 0};
};

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
  KeyMap* _zpmKM;
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
  KeyMap* _zpmKM;
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
