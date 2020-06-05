#pragma once
#include <ork/kernel/svariant.h>
#include "synthdata.h"
#include "dspblocks.h"
#include <ork/file/path.h>

namespace ork::audio::singularity {

struct Fm4OpData {
  int _atkRate       = 0; // 0..31
  int _dec1Rate      = 0; // 0..31
  int _dec1Lev       = 0; // 0..15
  int _dec2Rate      = 0; // 0..31
  int _relRate       = 0; // 0..15
  int _levScaling    = 0;
  int _ratScaling    = 0;
  bool _opEnable     = false;
  int _egBiasSensa   = 0;
  int _kvSensa       = 0;
  int _outLevel      = 0;
  int _coarseFrq     = 0;
  int _fineFrq       = 0;
  int _detune        = 0;
  int _egShift       = 0;
  bool _fixedFrqMode = false;
  int _fixedRange    = 0;
  int _waveform      = 0;
  int _OWF           = 0;
  int _EFF           = 0;
  int _F             = 0;

  float _modIndex = 0.0f;
  float _frqRatio = 1.0f;
  float _frqFixed = 0.0f;
};

struct Fm4ProgData {
  int _alg            = 0;
  int _feedback       = 0;
  bool _lfoSync       = false;
  int _lfoSpeed       = 0;
  int _lfoDepth       = 0;
  int _pchDepth       = 0;
  int _ampDepth       = 0;
  int _lfoWave        = 0;
  int _ampSensa       = 0;
  int _pchSensa       = 0;
  int _middleC        = 0;
  int _pitchBendRange = 0;
  bool _mono          = false;
  bool _portMode      = false;
  int _portRate       = 0;
  Fm4OpData _ops[4];
};

using fm4prgdata_ptr_t = std::shared_ptr<Fm4ProgData>;

struct DspKeyOnInfo;
struct DspBuffer;
struct fm4syn {
  fm4syn();
  void compute(Layer* layer);
  void keyOn(const DspKeyOnInfo& koi);
  void keyOff();

  Fm4ProgData _data;
  ork::svarp_t _pimpl;
  float _opAmp[4];
};

///////////////////////////////////////////////////////////////////////////////

struct op4frame {
  float _envout = 0.0f;
  int _envph    = 0;
  float _mi     = 0.0f;
  float _r      = 0.0f;
  float _f      = 0.0f;
  int _ar       = 0;
  int _d1r      = 0;
  int _d1l      = 0;
  int _d2r      = 0;
  int _rr       = 0;
  int _egshift  = 0;
  int _wav      = 0;
  int _olev     = 0;
};

hudpanel_ptr_t create_op4panel();

///////////////////////////////////////////////////////////////////////////////
struct FM4Data final : public DspBlockData {
  FM4Data(fm4prgdata_ptr_t fmdata);
  dspblk_ptr_t createInstance() const override;
  fm4prgdata_ptr_t _fmdata;
};

struct FM4 final : public DspBlock {
  using dataclass_t = FM4Data;
  FM4(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) override;
  void doKeyOn(const DspKeyOnInfo& koi) override;
  void doKeyOff() override;
  fm4syn _fm4;
};

///////////////////////////////////////////////////////////////////////////////

void configureTx81zAlgorithm(lyrdata_ptr_t layerdata, fm4prgdata_ptr_t fmdata);

///////////////////////////////////////////////////////////////////////////////

struct Tx81zData : public SynthData {
  Tx81zData();
  ~Tx81zData();
  void loadBank(const file::Path& syxpath);

  int _lastprg;
};

} // namespace ork::audio::singularity
