////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
// IoConfig:
//   specifies inputs and output configuration of a zpm module
////////////////////////////////////////////////////////////////////////////////

struct IoConfig final : public ork::Object {
  DeclareConcreteX(IoConfig, ork::Object);
  IoConfig();
  size_t numInputs() const;
  size_t numOutputs() const;
  std::vector<int> _inputs;
  std::vector<int> _outputs;
};

///////////////////////////////////////////////////////////////////////////////

struct DspBlockData : public ork::Object {

  DeclareAbstractX(DspBlockData, ork::Object);
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) override;

  DspBlockData(std::string name = "");

  virtual dspblk_ptr_t createInstance() const {
    return nullptr;
  }

  scopesource_ptr_t createScopeSource();

  std::string _blocktype;

  dspparam_ptr_t addParam(std::string name = "", std::string units="");
  dspparam_ptr_t param(int index);
  dspparam_ptr_t paramByName(std::string named);
  int addDspChannel(int channel);

  std::string _name;
  std::vector<int> _dspchannels;
  int _numParams  = 0;
  float _inputPad = 1.0f;
  int _blockIndex = -1;
  varmap::VarMap _vars;
  bool _bypass = false;
  std::vector<dspparam_ptr_t> _paramd;
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

  float* getRawBuf(DspBuffer& dspbuf, int chanindex);
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
  layer_ptr_t _layer      = nullptr;
  int _verticalIndex = -1;

  varmap::VarMap _vars;

  int _dspchannel[kmaxdspblocksperstage];
  float _fval[kmaxparmperblock];
  DspParam _param[kmaxparmperblock];
  ioconfig_constptr_t _ioconfig;
};

///////////////////////////////////////////////////////////////////////////////
// a DspStage is a vertical stack of up to N dspblocks
///////////////////////////////////////////////////////////////////////////////

struct DspStageData final : public ork::Object {

  DeclareConcreteX(DspStageData, ork::Object);
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) override;

  DspStageData();
  dspblkdata_ptr_t appendBlock();

  template <typename T, typename... A>     //
  std::shared_ptr<typename T::dataclass_t> //
  appendTypedBlock(
      std::string named, //
      A&&... args) {
    OrkAssert(_numblocks < kmaxdspblocksperstage);
    auto blkdata              = std::make_shared<typename T::dataclass_t>(named, std::forward<A>(args)...);
    blkdata->_name            = named;
    blkdata->_blockIndex      = _numblocks;
    _blockdatas[_numblocks++] = blkdata;
    _namedblockdatas[named]   = blkdata;
    return blkdata;
  }
  void setNumIos(int numinp, int numout);

  std::string _name;
  int _stageIndex = -1;
  dspblkdata_ptr_t _blockdatas[kmaxdspblocksperstage];
  std::map<std::string, dspblkdata_ptr_t> _namedblockdatas;
  ioconfig_ptr_t _ioconfig;
  int _numblocks = 0;
  void dump() const;
};
struct DspStage final {
  dspblk_ptr_t _blocks[kmaxdspblocksperstage];
  using blockfn_t = std::function<void(dspblk_ptr_t)>;
  void forEachBlock(blockfn_t fn);
};

///////////////////////////////////////////////////////////////////////////////
// TODO - reuse DspStages
///////////////////////////////////////////////////////////////////////////////

struct AlgStageBlock{
  dspstage_ptr_t _stages[kmaxdspstagesperlayer];
};

using algstacgeblock_ptr_t = std::shared_ptr<AlgStageBlock>();

///////////////////////////////////////////////////////////////////////////////

struct AlgData final : public ork::Object {

  DeclareConcreteX(AlgData, ork::Object);
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) override;

  dspstagedata_ptr_t appendStage(const std::string& named);
  dspstagedata_ptr_t stageByName(const std::string& named);
  dspstagedata_ptr_t stageByIndex(int index);
  alg_ptr_t createAlgInst() const;
  void returnAlgInst(alg_ptr_t alg) const;

  int _numstages = 0;
  std::string _name;
  dspstagedata_ptr_t _stages[kmaxdspstagesperlayer];
  std::map<std::string, dspstagedata_ptr_t> _stageByName;

  mutable std::vector<alg_ptr_t> _voicecache;
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

  AlgStageBlock _stageblock;

  const AlgData& _algdata;

  layer_ptr_t _layer;
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
