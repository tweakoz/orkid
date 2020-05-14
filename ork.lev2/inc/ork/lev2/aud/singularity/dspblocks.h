#pragma once

#include "modulation.h"
#include "filters.h"
#include "para.h"
#include "PolyBLEP.h"
#include "layer.h"
#include <ork/kernel/svariant.h>
#include "alg.h"

namespace ork::audio::singularity {

struct outputBuffer;
struct DspBlock;

///////////////////////////////////////////////////////////////////////////////

struct DspBuffer {
  DspBuffer();
  void resize(int inumframes);

  float* channel(int ich);

  int _maxframes;
  int _numframes;

private:
  static const int kmaxchans = 4;
  float* _channels[kmaxchans];
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

struct DspParamData {

  DspParamData();

  void useDefaultEvaluator();
  void usePitchEvaluator();
  void useFrequencyEvaluator();
  void useAmplitudeEvaluator();
  void useKrzPosEvaluator();
  void useKrzEvnOddEvaluator();

  std::string _name;
  std::string _units;
  float _coarse         = 0.0f;
  float _fine           = 0.0f;
  float _fineHZ         = 0.0f;
  float _keyTrack       = 0.0f;
  float _velTrack       = 0.0f;
  int _keystartNote     = 60;
  bool _keystartBipolar = true; // false==unipolar
  evalit_t _evaluator;
  BlockModulationData _mods;
};

///////////////////////////////////////////////////////////////////////////////
// IoMask:
//   specifies inputs and output configuration of a zpm module
////////////////////////////////////////////////////////////////////////////////

struct IoMask {
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

struct DspBlockData {
  ork::svar16_t getExtData(const std::string& name) const;
  //

  std::string _dspBlock;

  int _numParams  = 0;
  float _inputPad = 1.0f;
  int _blockIndex = -1;
  std::map<std::string, ork::svar16_t> _extdata;
  DspParamData _paramd[kmaxparmperblock];
};

///////////////////////////////////////////////////////////////////////////////

struct OscHardSyncTrack {
  inline void resize(int size) {
    _values.resize(size);
    for (int i = 0; i < size; i++)
      _values[i] = false;
  }
  std::vector<bool> _values;
};
using oschardsynctrack_ptr_t = std::shared_ptr<OscHardSyncTrack>;

///////////////////////////////////////////////////////////////////////////////

struct DspBlock {
  DspBlock(dspblkdata_constptr_t dbd);
  virtual ~DspBlock() {
  }

  void resize(int inumframes);
  size_t numFrames() const;

  void keyOn(const DspKeyOnInfo& koi);
  void keyOff(layer* l);

  virtual void compute(DspBuffer& dspbuf) = 0;

  virtual void doKeyOn(const DspKeyOnInfo& koi) {
  }
  virtual void doKeyOff() {
  }

  const float* getInpBuf(DspBuffer& dspbuf, int chanindex);
  float* getOutBuf(DspBuffer& dspbuf, int chanindex);

  FPARAM initFPARAM(const DspParamData& dpd);

  dspblkdata_constptr_t _dbd;
  int _numParams;
  int numOutputs() const;
  int numInputs() const;
  layer* _layer     = nullptr;
  size_t _numFrames = 0;

  varmap::VarMap _vars;

  float _fval[kmaxparmperblock];
  FPARAM _param[kmaxparmperblock];
  IoMask _iomask;
};

///////////////////////////////////////////////////////////////////////////////
// a DspStage is a vertical stack of up to N dspblocks
///////////////////////////////////////////////////////////////////////////////

struct DspStageData {
  dspblkdata_ptr_t appendBlock();
  dspblkdata_constptr_t _blockdatas[kmaxdspblocksperstage];
  IoMask _iomask;
  int _numblocks = 0;
};
struct DspStage {
  dspblk_ptr_t _blocks[kmaxdspblocksperstage];
  using blockfn_t = std::function<void(dspblk_ptr_t)>;
  void forEachBlock(blockfn_t fn);
};

///////////////////////////////////////////////////////////////////////////////

struct AlgData {
  int _krzAlgIndex = -1;
  int _numstages   = 0;
  std::string _name;
  dspstagedata_constptr_t _stages[kmaxdspstagesperlayer];
  alg_ptr_t createAlgInst() const;
};

///////////////////////////////////////////////////////////////////////////////

struct Alg final {
  Alg(const AlgData& algd);
  ~Alg();

  using stagefn_t = std::function<void(dspstage_ptr_t)>;

  void keyOn(DspKeyOnInfo& koi);
  void keyOff();

  void forEachStage(stagefn_t fn);

  void compute(outputBuffer& obuf);

  virtual void doKeyOn(DspKeyOnInfo& koi);
  void intoDspBuf(const outputBuffer& obuf);
  void intoOutBuf(outputBuffer& obuf, int inumo);
  dspblk_ptr_t lastBlock() const;

  dspstage_ptr_t _stages[kmaxdspstagesperlayer];
  const AlgData& _algdata;

  layer* _layer;
};

} // namespace ork::audio::singularity
