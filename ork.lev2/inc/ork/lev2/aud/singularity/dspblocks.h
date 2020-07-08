////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "modulation.h"
#include "filters.h"
#include "para.h"
#include "PolyBLEP.h"
#include "layer.h"
#include <ork/kernel/varmap.inl>

namespace ork::audio::singularity {

struct outputBuffer;
struct DspBlock;

///////////////////////////////////////////////////////////////////////////////

struct DspBuffer final {
  DspBuffer();
  void resize(int inumframes);

  float* channel(int ich);

  int _maxframes;
  int _numframes;

private:
  std::vector<float> _channels[kmaxdspblocksperstage];
};

///////////////////////////////////////////////////////////////////////////////

struct BlockModulationData final {

  BlockModulationData();
  BlockModulationData(const BlockModulationData&) = delete;

  controllerdata_ptr_t _src1;
  controllerdata_ptr_t _src2;
  controllerdata_ptr_t _src2DepthCtrl;

  float _src1Depth    = 0.0f;
  float _src2MinDepth = 0.0f;
  float _src2MaxDepth = 0.0f;
  evalit_t _evaluator;
};

///////////////////////////////////////////////////////////////////////////////

struct DspParamData final {

  DspParamData();

  void useDefaultEvaluator();
  void usePitchEvaluator();
  void useFrequencyEvaluator();
  void useAmplitudeEvaluator();
  void useKrzPosEvaluator();
  void useKrzEvnOddEvaluator();

  std::string _name;
  std::string _units;

  int _edit_coarse_numsteps = 1;
  float _edit_coarse_shape  = 1.0f;
  float _edit_coarse_min    = 0.0f;
  float _edit_coarse_max    = 1.0f;

  int _edit_fine_numsteps = 1;
  float _edit_fine_shape  = 1.0f;
  float _edit_fine_min    = 0.0f;
  float _edit_fine_max    = 1.0f;

  int _edit_keytrack_numsteps = 1;
  float _edit_keytrack_shape  = 1.0f;
  float _edit_keytrack_min    = 0.0f;
  float _edit_keytrack_max    = 1.0f;

  float _coarse         = 0.0f;
  float _fine           = 0.0f;
  float _fineHZ         = 0.0f;
  float _keyTrack       = 0.0f;
  float _velTrack       = 0.0f;
  int _keystartNote     = 60;
  bool _keystartBipolar = true; // false==unipolar
  // evalit_t _evaluator;
  dspparammod_ptr_t _mods;
};

///////////////////////////////////////////////////////////////////////////////
// IoMask:
//   specifies inputs and output configuration of a zpm module
////////////////////////////////////////////////////////////////////////////////

struct IoMask final {
  inline IoMask() {
  }
  inline size_t numInputs() const {
    return _inputs.size();
  }
  inline size_t numOutputs() const {
    return _outputs.size();
  }
  std::vector<int> _inputs;
  std::vector<int> _outputs;
};

///////////////////////////////////////////////////////////////////////////////

struct DspBlockData : public ork::Object {

  DeclareAbstractX(DspBlockData, ork::Object);

  DspBlockData(std::string name = "");

  virtual dspblk_ptr_t createInstance() const {
    return nullptr;
  }

  scopesource_ptr_t createScopeSource();

  std::string _blocktype;

  dspparam_ptr_t addParam();
  dspparam_ptr_t param(int index);

  std::string _name;
  int _dspchannel[kmaxdspblocksperstage];
  int _numParams  = 0;
  float _inputPad = 1.0f;
  int _blockIndex = -1;
  varmap::VarMap _vars;
  dspparam_ptr_t _paramd[kmaxparmperblock];
  scopesource_ptr_t _scopesource;
};

///////////////////////////////////////////////////////////////////////////////

struct OscillatorSyncTrack {
  inline void resize(int size) {
    _triggers.resize(size);
    for (int i = 0; i < size; i++)
      _triggers[i] = false;
  }
  std::vector<bool> _triggers;
};
struct ScopeSyncTrack {
  inline void resize(int size) {
    _triggers.resize(size);
    for (int i = 0; i < size; i++)
      _triggers[i] = false;
  }
  std::vector<bool> _triggers;
};

///////////////////////////////////////////////////////////////////////////////

struct DspBlock {
  DspBlock(const DspBlockData* dbd);
  virtual ~DspBlock() {
  }

  void keyOn(const KeyOnInfo& koi);
  void keyOff(Layer* l);

  virtual void compute(DspBuffer& dspbuf) = 0;

  virtual void doKeyOn(const KeyOnInfo& koi) {
  }
  virtual void doKeyOff() {
  }

  const float* getInpBuf(DspBuffer& dspbuf, int chanindex);
  float* getOutBuf(DspBuffer& dspbuf, int chanindex);

  DspParam initDspParam(dspparam_constptr_t dpd);

  virtual bool isHsyncSource() const {
    return false;
  }
  virtual bool isScopeSyncSource() const {
    return false;
  }

  const DspBlockData* _dbd;
  int _numParams;
  int numOutputs() const;
  int numInputs() const;
  Layer* _layer      = nullptr;
  int _verticalIndex = -1;

  varmap::VarMap _vars;

  int _dspchannel[kmaxdspblocksperstage];
  float _fval[kmaxparmperblock];
  DspParam _param[kmaxparmperblock];
  iomask_constptr_t _iomask;
};

///////////////////////////////////////////////////////////////////////////////
// a DspStage is a vertical stack of up to N dspblocks
///////////////////////////////////////////////////////////////////////////////

struct DspStageData final {
  DspStageData();
  dspblkdata_ptr_t appendBlock();

  template <typename T, typename... A> std::shared_ptr<typename T::dataclass_t> appendTypedBlock(std::string name, A&&... args) {
    OrkAssert(_numblocks < kmaxdspblocksperstage);
    auto blkdata              = std::make_shared<typename T::dataclass_t>(name, std::forward<A>(args)...);
    _blockdatas[_numblocks++] = blkdata;
    return blkdata;
  }
  void setNumIos(int numinp, int numout);

  dspblkdata_ptr_t _blockdatas[kmaxdspblocksperstage];
  std::map<std::string, dspblkdata_ptr_t> _namedblockdatas;
  iomask_ptr_t _iomask;
  int _numblocks = 0;
};
struct DspStage final {
  dspblk_ptr_t _blocks[kmaxdspblocksperstage];
  using blockfn_t = std::function<void(dspblk_ptr_t)>;
  void forEachBlock(blockfn_t fn);
};

///////////////////////////////////////////////////////////////////////////////

struct AlgData final {
  dspstagedata_ptr_t appendStage(const std::string& named);
  dspstagedata_ptr_t stageByName(const std::string& named);
  dspstagedata_ptr_t stageByIndex(int index);
  alg_ptr_t createAlgInst() const;

  int _numstages = 0;
  std::string _name;
  dspstagedata_ptr_t _stages[kmaxdspstagesperlayer];
  std::map<std::string, dspstagedata_ptr_t> _stageByName;
};

algdata_ptr_t configureKrzAlgorithm(int algid);

///////////////////////////////////////////////////////////////////////////////

struct Alg final {
  Alg(const AlgData& algd);
  ~Alg();

  using stagefn_t = std::function<void(dspstage_ptr_t)>;

  void keyOn(KeyOnInfo& koi);
  void keyOff();

  void forEachStage(stagefn_t fn);

  void beginCompute();
  void doComputePass();
  void endCompute();

  virtual void doKeyOn(KeyOnInfo& koi);
  dspblk_ptr_t lastBlock() const;

  dspstage_ptr_t _stages[kmaxdspstagesperlayer];

  const AlgData& _algdata;

  Layer* _layer;
};

///////////////////////////////////////////////////////////////////////////////
struct NOPDATA final : public DspBlockData {
  NOPDATA();
  dspblk_ptr_t createInstance() const override;
};

struct NOP final : public DspBlock {
  using dataclass_t = NOPDATA;
  NOP(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf);
};

} // namespace ork::audio::singularity
