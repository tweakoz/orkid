#pragma once
#include <ork/kernel/svariant.h>
#include "synthdata.h"
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
struct DspKeyOnInfo;
struct DspBuffer;
struct fm4syn {
  fm4syn();
  void compute(DspBuffer& dspbuf);
  void keyOn(const DspKeyOnInfo& koi);
  void keyOff();

  Fm4ProgData _data;
  ork::svarp_t _pimpl;
  float _opAmp[4];
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
  int _lastprg;
};

} // namespace ork::audio::singularity
