#pragma once

#include "krztypes.h"
#include "modulation.h"
#include "sf2.h"
#include <ork/kernel/svariant.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// IoMask:
//   specifies inputs and output configuration of a zpm module
////////////////////////////////////////////////////////////////////////////////

struct IoMask {
  IoMask(int i = 1, int o = 1)
      : _inputMask(i)
      , _outputMask(o) {
  }
  int numInputs() const;
  int numOutputs() const;
  int _inputMask;
  int _outputMask;
};

///////////////////////////////////////////////////////////////////////////////

struct AlgConfig {
  IoMask _ioMasks[kmaxdspblocksperlayer];
};

///////////////////////////////////////////////////////////////////////////////

struct Alg;
struct AlgData {
  int _algID = -1;
  std::string _name;
  AlgConfig _config;

  Alg* createAlgInst() const;
};

///////////////////////////////////////////////////////////////////////////////

struct EnvPoint {
  float _rate;
  float _level;
};

///////////////////////////////////////////////////////////////////////////////

struct ControllerInst;

struct ControllerData {
  virtual ControllerInst* instantiate(synth& syn) const = 0;
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
  ControllerInst* instantiate(synth& syn) const final;
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
  ControllerInst* instantiate(synth& syn) const final;

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

struct keymap {
  std::string _name;
  std::vector<kmregion*> _regions;
  int _kmID;

  kmregion* getRegion(int note, int vel) const;
};

///////////////////////////////////////////////////////////////////////////////

struct LfoData : public ControllerData {
  LfoData();
  ControllerInst* instantiate(synth& syn) const final;

  float _initialPhase;
  float _minRate;
  float _maxRate;
  std::string _controller;
  std::string _shape;
};

///////////////////////////////////////////////////////////////////////////////

struct FunData : public ControllerData {
  ControllerInst* instantiate(synth& syn) const final;

  std::string _a, _b, _op;
};

///////////////////////////////////////////////////////////////////////////////

struct BlockModulationData {
  std::string _src1          = "OFF";
  std::string _src2          = "OFF";
  std::string _src2DepthCtrl = "OFF";

  float _src1Depth    = 0.0f;
  float _src2MinDepth = 0.0f;
  float _src2MaxDepth = 0.0f;
  evalit_t _evaluator = [](FPARAM& cec) -> float { return cec._coarse; };
};

///////////////////////////////////////////////////////////////////////////////

struct KmpBlockData {
  const keymap* _keymap;
  int _transpose   = 0;
  float _keyTrack  = 100.0f;
  float _velTrack  = 0.0f;
  int _timbreShift = 0;
  std::string _pbMode;
};

///////////////////////////////////////////////////////////////////////////////

struct DspParamData {
  void initEvaluators();
  //

  std::string _paramScheme;
  std::string _name;
  std::string _units;
  float _coarse         = 0.0f;
  float _fine           = 0.0f;
  float _fineHZ         = 0.0f;
  float _keyTrack       = 0.0f;
  float _velTrack       = 0.0f;
  int _keystartNote     = 60;
  bool _keystartBipolar = true; // false==unipolar
  BlockModulationData _mods;
};

///////////////////////////////////////////////////////////////////////////////

struct DspBlockData {
  ork::svarp_t getExtData(const std::string& name) const;
  void initEvaluators();
  //

  std::string _dspBlock;

  int _numParams  = 0;
  float _inputPad = 1.0f;
  int _blockIndex = -1;
  std::map<std::string, ork::svarp_t> _extdata;
  DspParamData _paramd[kmaxparmperblock];
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

struct controlBlockData {
  const ControllerData* _cdata[kmaxctrlperblock] = {0, 0, 0, 0};
};

///////////////////////////////////////////////////////////////////////////////

struct layerData {
  layerData();

  AlgData _algData;

  EnvCtrlData _envCtrlData;

  KmpBlockData _kmpBlock;
  dspblkdata_ptr_t _pchBlock;
  dspblkdata_ptr_t _dspBlocks[kmaxdspblocksperlayer];

  dspblkdata_ptr_t appendDspBlock();

  const keymap* _keymap   = nullptr;
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

  controlBlockData* _ctrlBlocks[kmaxctrlblocks] = {0, 0, 0, 0, 0, 0, 0, 0};
};

///////////////////////////////////////////////////////////////////////////////

struct programData {
  lyrdata_ptr_t newLayer();
  lyrdata_ptr_t getLayer(int i) const {
    return _layerDatas[i];
  }
  std::string _name;
  std::string _role;
  std::vector<lyrdata_ptr_t> _layerDatas;
};

///////////////////////////////////////////////////////////////////////////////

struct programInst;

struct SynthData {
  SynthData(synth* syn);
  virtual ~SynthData() {
  }

  float seqTime(float dur);
  virtual const programData* getProgram(int progID) const                     = 0;
  virtual const programData* getProgramByName(const std::string& named) const = 0;

  programInst* _prog;
  float _synsr;
  synth* _syn;
  float _seqCursor;
  std::string _staticBankName;
};

///////////////////////////////////////////////////////////////////////////////

struct KrzSynthData : public SynthData {
  static VastObjectsDB* baseObjects();

  KrzSynthData(synth* syn);
  const programData* getProgram(int progID) const final;
  const programData* getProgramByName(const std::string& named) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct Sf2TestSynthData : public SynthData {
  Sf2TestSynthData(const file::Path& syxpath, synth* syn, const std::string& bankname = "sf2");
  ~Sf2TestSynthData();
  sf2::SoundFont* _sfont;
  const programData* getProgram(int progID) const final;
  const programData* getProgramByName(const std::string& named) const final {
    return nullptr;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct Tx81zData : public SynthData {
  Tx81zData(synth* syn);
  ~Tx81zData();
  void loadBank(const file::Path& syxpath);

  const programData* getProgram(int progID) const final;
  const programData* getProgramByName(const std::string& named) const final {
    return nullptr;
  }
  VastObjectsDB* _zpmDB;
  keymap* _zpmKM;
  int _lastprg;
};

///////////////////////////////////////////////////////////////////////////////

struct CzData : public SynthData {
  CzData(synth* syn);
  ~CzData();
  void loadBank(const file::Path& syxpath, const std::string& bnkname = "czb");

  const programData* getProgram(int progID) const final;
  const programData* getProgramByName(const std::string& named) const final {
    return nullptr;
  }
  VastObjectsDB* _zpmDB;
  keymap* _zpmKM;
  int _lastprg;
};

///////////////////////////////////////////////////////////////////////////////

struct KrzTestData : public SynthData {
  KrzTestData(synth* syn);
  void genTestPrograms();
  const programData* getProgram(int progID) const final;
  const programData* getProgramByName(const std::string& named) const final {
    return nullptr;
  }
  std::vector<programData*> _testPrograms;
};

///////////////////////////////////////////////////////////////////////////////

struct KrzKmTestData : public SynthData {
  KrzKmTestData(synth* syn);
  const programData* getProgram(int progID) const final;
  const programData* getProgramByName(const std::string& named) const final {
    return nullptr;
  }
  std::map<int, programData*> _testKmPrograms;
};

} // namespace ork::audio::singularity
