#pragma once
#include <ork/kernel/svariant.h>
#include "synthdata.h"
#include "dspblocks.h"
#include "fmosc.h"
#include <ork/file/path.h>

namespace ork::audio::singularity {

struct Pm4ProgData {
  std::string _name;
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
  PmOscData _ops[4];
};

using pm4prgdata_ptr_t = std::shared_ptr<Pm4ProgData>;

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
// PMX - 1-8 input PM operator
///////////////////////////////////////////////////////////////////////////////
struct PMXData final : public DspBlockData {
  PMXData();
  dspblk_ptr_t createInstance() const override;
  int _inpchannel                     = 0;
  float _feedback                     = 0.0f;
  float _modIndex                     = 1.0f;
  static constexpr int kmaxmodulators = 8;
  int _pmInpChannels[kmaxmodulators]  = {-1, -1, -1, -1, -1, -1, -1, -1};
  PmOscData _pmoscdata;
  int _opindex = 0;
  pm4prgdata_ptr_t _txprogramdata; // temp for debugging
  bool _modulator = false;
};

struct PMX final : public DspBlock {
  using dataclass_t = PMXData;
  PMX(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) override;
  void doKeyOn(const KeyOnInfo& koi) override;
  void doKeyOff() override;
  PmOsc _pmosc;
  float _modIndex         = 1.0f;
  const PMXData* _pmxdata = nullptr;
  bool _modulator         = false;
  float _amp              = 0.0f;
  float _frq              = 0.0f;
};
///////////////////////////////////////////////////////////////////////////////
struct PMXMixData final : public DspBlockData {
  PMXMixData();
  dspblk_ptr_t createInstance() const override;
  static constexpr int kmaxinputs  = 8;
  int _pmixInpChannels[kmaxinputs] = {-1, -1, -1, -1, -1, -1, -1, -1};
};

struct PMXMix final : public DspBlock {
  using dataclass_t = PMXMixData;
  PMXMix(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) override;
  void doKeyOn(const KeyOnInfo& koi) override;
  void doKeyOff() override;
  const PMXMixData* _pmixdata = nullptr;
  float _finalamp             = 1.0f;
};
///////////////////////////////////////////////////////////////////////////////

void configureTx81zAlgorithm(lyrdata_ptr_t layerdata, pm4prgdata_ptr_t fmdata);

///////////////////////////////////////////////////////////////////////////////

struct Tx81zData : public SynthData {
  Tx81zData();
  ~Tx81zData();
  void loadBank(const file::Path& syxpath);

  int _lastprg;
};

} // namespace ork::audio::singularity
