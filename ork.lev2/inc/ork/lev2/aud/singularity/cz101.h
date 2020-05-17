#pragma once
#include <ork/kernel/svariant.h>
#include "krztypes.h"
#include "dspblocks.h"
#include "synthdata.h"

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

struct CzEnvelope {
  int _endStep   = 0;
  int _sustPoint = -1;
  bool _decreasing[8];
  float _time[8];
  int _level[8];
};

///////////////////////////////////////////////////////////////////////////////

struct CzOscData {
  int _dcoWaveA     = 0;
  int _dcoWaveB     = 0;
  bool _enaWaveB    = false;
  int _dcoWindow    = 0;
  int _dcaKeyFollow = 0;
  int _dcwKeyFollow = 0;
  int _dcaVelFollow = 0;
  int _dcwVelFollow = 0;
  int _dcaDepth     = 0;
  int _dcwDepth     = 0;
  CzEnvelope _dcoEnv;
  CzEnvelope _dcaEnv;
  CzEnvelope _dcwEnv;
};

///////////////////////////////////////////////////////////////////////////////

using czxdata_ptr_t      = std::shared_ptr<CzOscData>;
using czxdata_constptr_t = std::shared_ptr<const CzOscData>;

///////////////////////////////////////////////////////////////////////////////

struct CzProgData {
  void dump() const;

  int _octave       = 0;
  int _lineSel      = 0;
  int _lineMod      = 0;
  int _detuneCents  = 0;
  int _vibratoWave  = 0;
  int _vibratoDelay = 0;
  int _vibratoRate  = 0;
  int _vibratoDepth = 0;
  czxdata_constptr_t _oscData[2];
  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////

struct CZX final : public DspBlock {
  CZX(dspblkdata_constptr_t dbd);
  void compute(DspBuffer& dspbuf) final;

  using oscmethod_t = std::function<void(CZX& inst, DspBuffer& dspbuf)>;

  void doKeyOn(const DspKeyOnInfo& koi) final;
  void doKeyOff() final;

  static constexpr float kinv64k = 1.0f / 65536.0f;
  static constexpr float kinv32k = 1.0f / 32768.0f;

  bool isHsyncSource() const override {
    return true;
  }
  bool isScopeSyncSource() const override {
    return true;
  }

  float _baseFrequency;
  float _modIndex;
  float _waveoutputs[8];
  int _waveIDA = 0;
  int _waveIDB = 0;
  int64_t _phase;
  int64_t _resophase;
  oschardsynctrack_ptr_t _hsynctrack;
  scopesynctrack_ptr_t _scopetrack;

  static void initBlock(dspblkdata_ptr_t blockdata, czxdata_constptr_t czdata);
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
  int _lastprg;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
