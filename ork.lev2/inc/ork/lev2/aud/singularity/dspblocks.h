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
#include "dspbuffer.h"

namespace ork::audio::singularity {

struct outputBuffer;
struct DspBlock;

///////////////////////////////////////////////////////////////////////////////
// IoConfig:
//   specifies inputs and output configuration of a zpm module
////////////////////////////////////////////////////////////////////////////////

struct IoConfig final : public ork::Object {
  DeclareConcreteX(IoConfig, ork::Object);
  IoConfig();

  ioconfig_ptr_t clone() const;

  size_t numInputs() const;
  size_t numOutputs() const;
  std::vector<int> _inputs;
  std::vector<int> _outputs;
};

///////////////////////////////////////////////////////////////////////////////
// dsp instance storage recycler.
//  a note-on instantiates the dsp grid of every layer it keys, on the audio
//  thread. the instances themselves cannot be reused across notes (a recycled
//  block would carry the previous note's filter/phase state), so what is pooled
//  is the STORAGE: allocate_shared draws the combined control-block+object from
//  a size-classed free list that the block's destruction refills, and the
//  object is constructed fresh on top of it exactly as make_shared did.
//  a momentarily empty (or full) free list falls through to the global
//  allocator - the pool refills itself on the next return, so the degrade is
//  bounded to the warmup and to growth past any previous concurrency peak.
///////////////////////////////////////////////////////////////////////////////

static constexpr size_t kdspinstancealign = 64;

void* dspInstanceAlloc(size_t nbytes);
void dspInstanceFree(void* ptr, size_t nbytes);
size_t dspInstancePoolMisses(); // storage that had to come from the allocator

template <typename T> struct DspInstanceAllocator {
  using value_type = T;
  DspInstanceAllocator() = default;
  template <typename U> constexpr DspInstanceAllocator(const DspInstanceAllocator<U>&) noexcept {
  }
  T* allocate(size_t count) {
    static_assert(alignof(T) <= kdspinstancealign, "dsp instance storage is 64 byte aligned");
    return static_cast<T*>(dspInstanceAlloc(count * sizeof(T)));
  }
  void deallocate(T* ptr, size_t count) noexcept {
    dspInstanceFree(ptr, count * sizeof(T));
  }
  template <typename U> bool operator==(const DspInstanceAllocator<U>&) const noexcept {
    return true;
  }
  template <typename U> bool operator!=(const DspInstanceAllocator<U>&) const noexcept {
    return false;
  }
};

// every DspBlockData::createInstance override instantiates through this.
template <typename T, typename... A> std::shared_ptr<T> createDspInstance(A&&... args) {
  return std::allocate_shared<T>(DspInstanceAllocator<T>(), std::forward<A>(args)...);
}

///////////////////////////////////////////////////////////////////////////////

struct DspBlockData : public ork::Object {

  DeclareAbstractX(DspBlockData, ork::Object);
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) override;

  DspBlockData(std::string name = "");
  virtual dspblkdata_ptr_t clone() const { return nullptr; }

  virtual dspblk_ptr_t createInstance() const {
    return nullptr;
  }

  scopesource_ptr_t createScopeSource();

  dspparam_ptr_t addParam(std::string name = "", std::string units="");
  dspparam_ptr_t param(int index);
  dspparam_ptr_t paramByName(std::string named);
  int addDspChannel(int channel);

  std::string _name;
  std::string _blocktype;
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
  svar64_t _impl[4];

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

  dspstagedata_ptr_t clone() const;
  void clear();
  
  dspblkdata_ptr_t appendBlock();

  template <typename T, typename... A>     //
  std::shared_ptr<typename T::dataclass_t> //
  appendTypedBlock(
      std::string named, //
      A&&... args) {
    auto blkdata              = std::make_shared<typename T::dataclass_t>(named, std::forward<A>(args)...);
    blkdata->_name            = named;
    blkdata->_blockIndex      = _numblocks++;
    _blockdatas.push_back(blkdata);
    _namedblockdatas[named]   = blkdata;
    return blkdata;
  }
  void setNumIos(int numinp, int numout);

  std::string _name;
  int _stageIndex = -1;
  std::vector<dspblkdata_ptr_t> _blockdatas;
  std::map<std::string, dspblkdata_ptr_t> _namedblockdatas;
  ioconfig_ptr_t _ioconfig;
  int _numblocks = 0;
  void dump() const;
};
// the block stack is capped by kmaxdspblocksperstage (structural), so it is a
//  fixed array: the vector's growth was a heap allocation per stage, on the
//  audio thread, at every note-on. the traversals are templates for the same
//  reason - a std::function capturing more than a pointer heap-allocates, and
//  these run per stage per block per control pass.
struct DspStage final {
  template <typename F> void forEachBlock(const F& fn) {
    for (int i = 0; i < _numblocks; i++) {
      const auto& b = _blocks[i];
      if (b)
        fn(b);
    }
  }
  void clear() {
    for (int i = 0; i < _numblocks; i++)
      _blocks[i] = nullptr;
    _numblocks = 0;
  }
  std::array<dspblk_ptr_t, kmaxdspblocksperstage> _blocks;
  int _numblocks = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct AlgStageBlock{
  dspstage_ptr_t _stages[kmaxdspstagesperlayer];
};

using algstacgeblock_ptr_t = std::shared_ptr<AlgStageBlock>();

///////////////////////////////////////////////////////////////////////////////

struct AlgData final : public ork::Object {

  DeclareConcreteX(AlgData, ork::Object);
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) override;
  algdata_ptr_t clone() const;

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
  // guards _voicecache: post-H2 setEffect can alloc (createAlgInst, caller
  // thread) while a prior install's commit event returns a spent alg
  // (returnAlgInst, audio thread) against the same shared AlgData.
  mutable std::mutex _voicecache_mutex;
  // instances handed out by createAlgInst. _voicecache is kept reserved to at
  // least this many entries - and that growth is done unlocked on the create
  // path - so returnAlgInst (audio thread) can neither reallocate nor wait on
  // a thread that is mid-malloc.
  mutable size_t _numalginstances = 0;
};

algdata_ptr_t configureKrzAlgorithm(int algid);


///////////////////////////////////////////////////////////////////////////////

struct Alg final {
  Alg(const AlgData& algd);
  ~Alg();

  void keyOn(KeyOnInfo& koi);
  void keyOff();

  template <typename F> void forEachStage(const F& fn) {
    for (int istage = 0; istage < kmaxdspstagesperlayer; istage++) {
      const auto& stage = _stageblock._stages[istage];
      if (stage)
        fn(stage);
    }
  }

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
