#pragma once
#include <ork/kernel/svariant.h>
#include "synthdata.h"
#include "dspblocks.h"
#include <ork/file/path.h>

namespace ork::audio::singularity {

struct Fm4OpData {
  // bool _opEnable   = false;
  // int _egBiasSensa = 0;
  // int _kvSensa     = 0;
  // int _outLevel    = 0;
  // int _egShift     = 0;
  int _waveform   = 0;
  float _modindex = 1.0f;
  // int _OWF         = 0;
  // int _EFF         = 0;
  // int _F           = 0;

  float _modIndex = 0.0f;
};

struct Fm4ProgData {
  int _alg            = 0;
  bool _lfoSync       = false;
  int _lfoSpeed       = 0;
  int _lfoDepth       = 0;
  int _pchDepth       = 0;
  int _ampDepth       = 0;
  int _lfoWave        = 0;
  int _ampSensa       = 0;
  int _pchSensa       = 0;
  int _pitchBendRange = 0;
  bool _mono          = false;
  bool _portMode      = false;
  int _portRate       = 0;
  Fm4OpData _ops[4];
};

using fm4prgdata_ptr_t = std::shared_ptr<Fm4ProgData>;

struct DspBuffer;

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
  void doKeyOn(const KeyOnInfo& koi) override;
  void doKeyOff() override;
  ork::svar16_t _pimpl;
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
